#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <variant>
#include <memory>

#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <mio/mmap.hpp>

#include <httplib/httplib.h>
#include <nlohmann/json.hpp>

#include <gs++/bhash.hpp>
#include <gs++/output.hpp>
#include <gs++/pk_script.hpp>
#include <gs++/utxodb.hpp>
#include <gs++/util.hpp>

#include <3rdparty/picosha2.h>


namespace gs {

bool utxodb::load_from_bchd_checkpoint(const std::string & path)
{
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }

	outpoint_map.reserve(40000000);
    mio::mmap_source mmap(fd, 0, mio::map_entire_file);

	std::size_t i =0;
	for (auto it = mmap.begin(); it != mmap.end();) {
        gs::txid prev_tx_id;
		std::reverse_copy(it, it+32, reinterpret_cast<char*>(prev_tx_id.begin()));
        it+=32;

		std::uint32_t prev_out_idx;
		std::copy(it, it+4, reinterpret_cast<char*>(&prev_out_idx));
        it+=4;


		std::uint32_t height;
		std::copy(it, it+4, reinterpret_cast<char*>(&height));
        it+=4;

		std::uint64_t value;
		std::copy(it, it+8, reinterpret_cast<char*>(&value));
        it+=8;

		bool is_coinbase; // TODO

		std::uint32_t script_len;
		std::copy(it, it+4, reinterpret_cast<std::uint8_t*>(&script_len));
        it+=4;

        gs::pk_script pk_script(script_len);
		std::copy(it, it+script_len, std::back_inserter(pk_script.v));
        it+=script_len;


		const gs::outpoint op(prev_tx_id, prev_out_idx);
        gs::output* const oid = &(*outpoint_map.insert({
			op,
			gs::output(
				prev_tx_id,
				prev_out_idx,
				height,
				value,
				pk_script
			)
		}).first).second;

		if (! pk_script_to_output.count(pk_script)) {
			pk_script_to_output.insert({ pk_script, { oid } });
		} else {
			pk_script_to_output[pk_script].emplace(oid);
		}

		// std::cout << prev_tx_id.decompress() << "\t" << prev_out_idx << std::endl; 
        /*
		std::cout << (int) is_coinbase << std::endl;
		std::cout << height << std::endl;
		std::cout << value << std::endl;
		std::cout << script_len << std::endl;
        */
		++i;
		if (i % 100000 == 0) {
			std::cout << i << "\n";
		}
	}

    return true;
}

std::shared_ptr<httplib::Response> rpc_query(
    httplib::Client & cli,
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
            httplib::make_basic_authentication_header("user", "password919191828282777wq")
        },
        robj.dump(),
        "text/plain"
    );
}

std::vector<std::uint8_t> utxodb::get_raw_block(
    httplib::Client & cli,
    const std::size_t height
) {
    std::cout << height << std::endl;

    std::string block_hash;
    {
        std::shared_ptr<httplib::Response> res = rpc_query(cli, "getblockhash", nlohmann::json::array({ height }));
        if (res->status == 200) {
            auto jbody = nlohmann::json::parse(res->body);
            // std::cout << jbody << std::endl;

            if (jbody.size() > 0) {
                if (! jbody[0]["error"].is_null()) {
                    std::cerr << jbody[0]["error"] << "\n";
                }

                block_hash = jbody[0]["result"].get<std::string>();
            }
        }
    }

    std::string block_data_str;
    {
        std::shared_ptr<httplib::Response> res = rpc_query(cli, "getblock", nlohmann::json::array({ block_hash, 0 }));
        if (res->status == 200) {
            auto jbody = nlohmann::json::parse(res->body);

            if (jbody.size() > 0) {
                if (! jbody[0]["error"].is_null()) {
                    std::cerr << jbody[0]["error"] << "\n";
                }

                block_data_str = jbody[0]["result"].get<std::string>();
            }
        }
    }


    std::vector<std::uint8_t> block_data;
    block_data.reserve(block_data_str.size() / 2);
    for (unsigned i=0; i<block_data_str.size() / 2; ++i) {
        const std::uint8_t p1 = block_data_str[(i<<1)+0];
        const std::uint8_t p2 = block_data_str[(i<<1)+1];

        assert((p1 >= '0' && p1 <= '9') || (p1 >= 'a' && p1 <= 'f'));
        assert((p2 >= '0' && p2 <= '9') || (p2 >= 'a' && p2 <= 'f'));

        block_data[i] = ((p1 >= '0' && p1 <= '9' ? p1 - '0' : p1 - 'a' + 10) << 4)
                      +  (p2 >= '0' && p2 <= '9' ? p2 - '0' : p2 - 'a' + 10);
    }

    auto it = block_data.begin();
    const std::uint32_t version { gs::util::extract_u32(it) };

    gs::txid prev_block;
    std::reverse_copy(it, it+32, reinterpret_cast<char*>(prev_block.begin()));
    it+=32;

    gs::txid merkle_root;
    std::reverse_copy(it, it+32, reinterpret_cast<char*>(merkle_root.begin()));
    it+=32;

    const std::uint32_t timestamp { gs::util::extract_u32(it) };
    const std::uint32_t bits      { gs::util::extract_u32(it) };
    const std::uint32_t nonce     { gs::util::extract_u32(it) };
    const std::uint64_t txn_count { gs::util::extract_var_int(it) };


    /*
    std::cout
        << version << "\n"
        << prev_block.decompress() << "\n"
        << merkle_root.decompress() << "\n"
        << timestamp << "\n"
        << bits << "\n"
        << nonce << "\n"
        << txn_count << "\n";
    */

    std::vector<gs::outpoint> blk_inputs;
    std::vector<gs::output>   blk_outputs;

    for (std::uint64_t i=0; i<txn_count; ++i) {
        const auto tx_begin_it = it;

        const std::int32_t  version     { gs::util::extract_i32(it) };
        const std::uint64_t tx_in_count { gs::util::extract_var_int(it) };

        std::vector<gs::outpoint> tx_inputs;
        tx_inputs.reserve(tx_in_count);
        for (std::uint32_t in_i=0; in_i<tx_in_count; ++in_i) {
            gs::txid prev_tx_id;
            std::reverse_copy(it, it+32, reinterpret_cast<char*>(prev_tx_id.begin()));
            it+=32;

            const std::uint32_t prev_out_idx { gs::util::extract_u32(it) };
            const std::uint64_t script_len   { gs::util::extract_var_int(it) };
            
            std::vector<std::uint8_t> sigscript;
            sigscript.reserve(script_len);
            std::copy(it, it+script_len, std::back_inserter(sigscript));
            it+=script_len;

            const std::uint32_t sequence { gs::util::extract_u32(it) };

            tx_inputs.push_back(gs::outpoint(prev_tx_id, prev_out_idx));
        }

        const std::uint64_t tx_out_count { gs::util::extract_var_int(it) };
        
        std::vector<gs::output> tx_outputs;
        tx_outputs.reserve(tx_out_count);
        for (std::uint32_t out_i=0; out_i<tx_out_count; ++out_i) {
            const std::uint64_t value      { gs::util::extract_u64(it) };
            const std::uint64_t script_len { gs::util::extract_var_int(it) };

            gs::pk_script pk_script(script_len);
            std::copy(it, it+script_len, std::back_inserter(pk_script.v));
            it+=script_len;

            // skip OP_RETURN
            if (pk_script.v[0] == 0x6a) {
                continue;
            }
            tx_outputs.push_back(gs::output({}, out_i, height, value, pk_script));
        }

        const std::uint32_t lock_time { gs::util::extract_u32(it) };

        const auto tx_end_it = it;

        std::vector<std::uint8_t> serialized_tx;
        serialized_tx.reserve(tx_end_it - tx_begin_it);
        std::copy(tx_begin_it, tx_end_it, std::back_inserter(serialized_tx));

        gs::txid txid;
        picosha2::hash256(serialized_tx, txid.v);
        picosha2::hash256(txid.v, txid.v);
        std::reverse(txid.begin(), txid.end());

        for (auto & m : tx_inputs) {
            blk_inputs.emplace_back(m);
        }

        for (auto & m : tx_outputs) {
            m.prev_tx_id = txid;
            blk_outputs.emplace_back(m);
        }
    }

    for (auto & m : blk_outputs) {
        gs::output* const oid = &(*outpoint_map.insert({
            gs::outpoint(m.prev_tx_id, m.prev_out_idx),
			m
		}).first).second;

		if (! pk_script_to_output.count(m.pk_script)) {
			pk_script_to_output.insert({ m.pk_script, { oid } });
		} else {
			pk_script_to_output[m.pk_script].emplace(oid);
		}

        // std::cout << height << "\tadded: " << m.prev_tx_id.decompress() << ":" << m.prev_out_idx << "\n";
    }

    for (auto & m : blk_inputs) {
        if (outpoint_map.count(m) == 0) {
            continue;
        }

        const gs::output& o           = outpoint_map.at(m);
        const gs::pk_script pk_script = o.pk_script;

        if (! pk_script_to_output.count(pk_script)) {
            continue;
        }
        absl::flat_hash_set<gs::output*> & addr_map = pk_script_to_output.at(pk_script);
        if (addr_map.erase(&o)) {
            // std::cout << height << "\tremoved: " << m.txid.decompress() << ":" << m.vout << "\n";
        }
        if (addr_map.empty()) {
            pk_script_to_output.erase(pk_script);
        }
        
    }

    return block_data;
}

}


int main() {
	httplib::Client cli("0.0.0.0", 8332);
    gs::utxodb utxodb;
    utxodb.get_raw_block(cli, 600000);

	// 582680
    utxodb.load_from_bchd_checkpoint("../utxo-checkpoints/QmXkBQJrMKkCKNbwv4m5xtnqwU9Sq7kucPigvZW8mWxcrv");

    for (std::uint32_t h=582680; h<602365; ++h) {
        utxodb.get_raw_block(cli, h);
    }

    return 0;
}
