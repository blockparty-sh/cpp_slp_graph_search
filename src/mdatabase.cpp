#include <iostream>
#include <vector>
#include <thread>
#include <cstdint>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/string/to_string.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/instance.hpp>
#include <spdlog/spdlog.h>
#include <gs++/transaction.hpp>
#include <gs++/txhash.hpp>
#include <gs++/mdatabase.hpp>


std::vector<txhash> mdatabase::get_all_token_ids()
{
    auto client = pool.acquire();
    auto collection = (*client)[db_name]["tokens"];

    mongocxx::options::find opts{};
    opts.projection(
        bsoncxx::builder::stream::document{}
    << "tokenDetails.tokenIdHex" << 1
    << bsoncxx::builder::stream::finalize
    );

    std::vector<txhash> ret;
    auto cursor = collection.find({}, opts);
    for (auto&& doc : cursor) {
        const auto el = doc["tokenDetails"]["tokenIdHex"];
        assert(el && el.type() == bsoncxx::type::k_utf8);
        const std::string tokenIdHex_str = bsoncxx::string::to_string(el.get_utf8().value);
        ret.emplace_back(tokenIdHex_str);
    }

    return ret;
}


int mdatabase::get_current_block_height()
{
    auto client = pool.acquire();
    auto collection = (*client)[db_name]["statuses"];

    mongocxx::options::find opts{};
    opts.projection(
        bsoncxx::builder::stream::document{}
    << "blockHeight" << 1
    << bsoncxx::builder::stream::finalize
    );

    auto cursor = collection.find({}, opts);

    for (auto&& doc : cursor) {
        auto el = doc["blockHeight"];
        assert(el && (el.type() == bsoncxx::type::k_int32 || el.type() == bsoncxx::type::k_int64));

        if (el.type() == bsoncxx::type::k_int32) {
            return el.get_int32().value;
        }

        if (el.type() == bsoncxx::type::k_int64) {
            return el.get_int64().value;
        }
    }

    return -1;
}


void mdatabase::watch_for_status_update(
    txgraph & g,
    int & current_block_height
) {
    const std::chrono::milliseconds await_time { 1000 };
    auto client = pool.acquire();
    auto collection = (*client)[db_name]["statuses"];

    while (continue_watching_mongo) {
        const int block_height = get_current_block_height();

        if (block_height > 0 && current_block_height < block_height) {
            for (int h=current_block_height+1; h<=block_height; ++h) {
                absl::flat_hash_map<txhash, std::vector<transaction>> block_data = load_block(h);
                int tid = 1;

                for (auto it : block_data) {
                    spdlog::info("block: {} token: {}\t{}\t({}/{})",
                        h, it.first,
                        g.insert_token_data(it.first, it.second),
                        tid, block_data.size()
                    );
                    ++tid;
                }

                current_block_height = h;
            }
        }

        std::this_thread::sleep_for(await_time);
    }
}

std::vector<transaction> mdatabase::load_token(
    const txhash tokenid,
    const int max_block_height
) {
    using bsoncxx::builder::basic::make_document;
    using bsoncxx::builder::basic::kvp;

    auto client = pool.acquire();
    auto collection = (*client)[db_name]["graphs"];

    mongocxx::pipeline pipe{};
    pipe.match(make_document(
        kvp("tokenDetails.tokenIdHex", tokenid)
    ));
    pipe.lookup(make_document(
        kvp("from", "confirmed"),
        kvp("localField", "graphTxn.txid"),
        kvp("foreignField", "tx.h"),
        kvp("as", "tx")
    ));

    if (max_block_height >= 0) {
        pipe.match(make_document(
            kvp("tx.blk.i", make_document(
                kvp("$lte", max_block_height)
            ))
        ));
    }

    pipe.project(make_document(
        kvp("graphTxn.txid", 1),
        kvp("graphTxn.inputs.txid", 1),
        kvp("tx.tx.raw", 1)
    ));

    std::vector<transaction> ret;
    auto cursor = collection.aggregate(pipe, mongocxx::options::aggregate{});
    for (auto&& doc : cursor) {
        const auto txid_el = doc["graphTxn"]["txid"];
        assert(txid_el && txid_el.type() == bsoncxx::type::k_utf8);
        const std::string txid_str = bsoncxx::string::to_string(txid_el.get_utf8().value);

        std::string txdata_str;

        const auto tx_el = doc["tx"];
        assert(tx_el && tx_el.type() == bsoncxx::type::b_array);
        const bsoncxx::array::view tx_sarr { tx_el.get_array().value };

        if (tx_sarr.empty()) {
            spdlog::warn("load_token: associated tx not found in confirmed {}", txid_str);
            continue;
        }
        for (bsoncxx::array::element tx_s_el : tx_sarr) {
            auto txdata_el = tx_s_el["tx"]["raw"];
            assert(txdata_el && txdata_el.type() == bsoncxx::type::k_binary);
            auto txdata_bin = txdata_el.get_binary();

            txdata_str.resize(txdata_bin.size, '\0');
            std::copy(
                txdata_bin.bytes,
                txdata_bin.bytes+txdata_bin.size,
                std::begin(txdata_str)
            );

            break; // this is used for $lookup so just 1 item
        }

        std::vector<txhash> inputs;
        const auto inputs_el = doc["graphTxn"]["inputs"];
        assert(inputs_el && inputs_el.type() == bsoncxx::type::b_array);
        const bsoncxx::array::view inputs_sarr { inputs_el.get_array().value };

        for (bsoncxx::array::element input_s_el : inputs_sarr) {
            auto input_txid_el = input_s_el["txid"];
            assert(input_txid_el && input_txid_el.type() == bsoncxx::type::k_utf8);
            const std::string input_txid_str = bsoncxx::string::to_string(input_txid_el.get_utf8().value);
            inputs.emplace_back(input_txid_str);
        }

        ret.emplace_back(transaction(txid_str, txdata_str, inputs));
    }

    return ret;
}

absl::flat_hash_map<txhash, std::vector<transaction>> mdatabase::load_block(
    const int block_height
) {
    using bsoncxx::builder::basic::make_document;
    using bsoncxx::builder::basic::kvp;

    auto client = pool.acquire();
    auto collection = (*client)[db_name]["confirmed"];

    mongocxx::pipeline pipe{};
    pipe.match(make_document(
        kvp("blk.i", block_height),
        kvp("slp.valid", true)
    ));
    pipe.lookup(make_document(
        kvp("from", "graphs"),
        kvp("localField", "tx.h"),
        kvp("foreignField", "graphTxn.txid"),
        kvp("as", "graph")
    ));
    pipe.project(make_document(
        kvp("tx.h", 1),
        kvp("tx.raw", 1),
        kvp("slp.detail.tokenIdHex", 1),
        kvp("graph.graphTxn.inputs.txid", 1)
    ));

    absl::flat_hash_map<txhash, std::vector<transaction>> ret;
    auto cursor = collection.aggregate(pipe, mongocxx::options::aggregate{});
    for (auto&& doc : cursor) {
        const auto txid_el = doc["tx"]["h"];
        assert(txid_el && txid_el.type() == bsoncxx::type::k_utf8);
        const std::string txid_str = bsoncxx::string::to_string(txid_el.get_utf8().value);

        const auto txdata_el = doc["tx"]["raw"];
        assert(txdata_el && txdata_el.type() == bsoncxx::type::k_binary);

        auto txdata_bin = txdata_el.get_binary();

        std::string txdata_str(txdata_bin.size, '\0');
        std::copy(
            txdata_bin.bytes,
            txdata_bin.bytes+txdata_bin.size,
            std::begin(txdata_str)
        );

        const auto tokenidhex_el = doc["slp"]["detail"]["tokenIdHex"];
        assert(tokenidhex_el && tokenidhex_el.type() == bsoncxx::type::k_utf8);
        const std::string tokenidhex_str = bsoncxx::string::to_string(tokenidhex_el.get_utf8().value);


        const auto graph_el = doc["graph"];
        assert(graph_el && graph_el.type() == bsoncxx::type::b_array);
        const bsoncxx::array::view graph_sarr { graph_el.get_array().value };

        if (graph_sarr.empty()) {
            spdlog::warn("load_block: associated tx not found in graphs {}", txid_str);
            continue;
        }
        for (bsoncxx::array::element graph_s_el : graph_sarr) {
            std::vector<txhash> inputs;

            const auto inputs_el = graph_s_el["graphTxn"]["inputs"];
            assert(inputs_el && inputs_el.type() == bsoncxx::type::b_array);
            const bsoncxx::array::view inputs_sarr { inputs_el.get_array().value };

            for (bsoncxx::array::element input_s_el : inputs_sarr) {
                auto input_txid_el = input_s_el["txid"];
                assert(input_txid_el && input_txid_el.type() == bsoncxx::type::k_utf8);
                const std::string input_txid_str = bsoncxx::string::to_string(input_txid_el.get_utf8().value);
                inputs.emplace_back(input_txid_str);
            }

            if (! ret.count(tokenidhex_str)) {
                ret.insert({ tokenidhex_str, {} });
            }

            ret[tokenidhex_str].emplace_back(transaction(txid_str, txdata_str, inputs));

            break; // this is used for $lookup so just 1 item
        }
    }

    return ret;
}
