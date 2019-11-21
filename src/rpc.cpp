#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <iostream>
#include <httplib/httplib.h>
#include <nlohmann/json.hpp>
#include <gs++/util.hpp>
#include <gs++/bhash.hpp>
#include <gs++/rpc.hpp>

namespace gs {

std::shared_ptr<httplib::Response> rpc::query(
    const std::string & method,
    const nlohmann::json & params
) {
    nlohmann::json robj {{
        { "jsonrpc", "2.0" },
        { "method", method },
        { "params", params },
        { "id", 0 }
    }};

    return cli.Post("/", {
            httplib::make_basic_authentication_header(rpc_user, rpc_pass)
        },
        robj.dump(),
        "text/plain"
    );
}

std::pair<bool, std::vector<std::uint8_t>> rpc::get_raw_block(
    const std::size_t height
) {
    std::string block_hash;
    {
        std::shared_ptr<httplib::Response> res = query("getblockhash", nlohmann::json::array({ height }));
        if (! res) {
            return { false, {} };
        }

        if (res->status != 200) {
            return { false, {} };
        }

        auto jbody = nlohmann::json::parse(res->body);
        // std::cout << jbody << std::endl;

        if (jbody.size() == 0) {
            return { false, {} };
        }

        if (! jbody[0]["error"].is_null()) {
            std::cerr << jbody[0]["error"] << "\n";
            return { false, {} };
        }

        block_hash = jbody[0]["result"].get<std::string>();
    }

    std::string block_data_str;
    {
        std::shared_ptr<httplib::Response> res = query("getblock", nlohmann::json::array({ block_hash, 0 }));
        if (! res) {
            return { false, {} };
        }

        if (res->status != 200) {
            return { false, {} };
        }

        auto jbody = nlohmann::json::parse(res->body);
        // std::cout << jbody << std::endl;

        if (jbody.size() == 0) {
            return { false, {} };
        }
        if (! jbody[0]["error"].is_null()) {
            std::cerr << jbody[0]["error"] << "\n";
            return { false, {} };
        }

        block_data_str = jbody[0]["result"].get<std::string>();
    }

    return { true, gs::util::compress_hex(block_data_str) };
}

std::pair<bool, std::uint32_t> rpc::get_best_block_height()
{
    std::shared_ptr<httplib::Response> res = query("getblockchaininfo", {});

    if (! res) {
        return { false, {} };
    }

    if (res->status != 200) {
        return { false, {} };
    }

    auto jbody = nlohmann::json::parse(res->body);

    if (jbody.size() == 0) {
        return { false, {} };
    }
    if (! jbody[0]["error"].is_null()) {
        std::cerr << jbody[0]["error"] << "\n";
        return { false, {} };
    }

    // std::cout << jbody << std::endl;
    return { true, jbody[0]["result"]["blocks"].get<std::uint32_t>() };
}

std::pair<bool, nlohmann::json> rpc::get_decode_raw_transaction(const std::string hex_str)
{
    std::shared_ptr<httplib::Response> res = query("decoderawtransaction", nlohmann::json::array({ hex_str }));
    if (! res) {
        return { false, {} };
    }

    if (res->status != 200) {
        return { false, {} };
    }

    // std::cout << res->body << std::endl;
    auto jbody = nlohmann::json::parse(res->body);

    if (jbody.size() == 0) {
        return { false, {} };
    }

    if (! jbody[0]["error"].is_null()) {
        std::cerr << jbody[0]["error"] << "\n";
        return { false, {} };
    }

    return { true, jbody[0]["result"] };
}

std::pair<bool, std::vector<gs::txid>> rpc::get_raw_mempool()
{
    std::shared_ptr<httplib::Response> res = query("getrawmempool", nlohmann::json::array({ }));
    if (! res) {
        return { false, {} };
    }

    if (res->status != 200) {
        return { false, {} };
    }

    // std::cout << res->body << std::endl;
    auto jbody = nlohmann::json::parse(res->body);

    if (jbody.size() == 0) {
        return { false, {} };
    }

    if (! jbody[0]["error"].is_null()) {
        std::cerr << jbody[0]["error"] << "\n";
        return { false, {} };
    }

    std::vector<gs::txid> ret;
    for (auto & jtxid : jbody[0]["result"]) {
        gs::txid txid(jtxid.get<std::string>());
        std::reverse(txid.v.begin(), txid.v.end());
        ret.push_back(txid);

    }
    return { true, ret };
}

std::pair<bool, std::vector<std::uint8_t>> rpc::get_raw_transaction(const gs::txid& txid)
{
    std::shared_ptr<httplib::Response> res = query("getrawtransaction", nlohmann::json::array({ txid.decompress(true), 0 }));
    if (! res) {
        return { false, {} };
    }

    if (res->status != 200) {
        return { false, {} };
    }

    // std::cout << res->body << std::endl;
    auto jbody = nlohmann::json::parse(res->body);

    if (jbody.size() == 0) {
        return { false, {} };
    }

    if (! jbody[0]["error"].is_null()) {
        std::cerr << jbody[0]["error"] << "\n";
        return { false, {} };
    }

    std::vector<std::uint8_t> ret = gs::util::compress_hex(jbody[0]["result"].get<std::string>());

    return { true, ret };
}

}
