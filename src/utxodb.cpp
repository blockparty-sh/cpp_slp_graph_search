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
#include <zmq.hpp>

#include <gs++/bhash.hpp>
#include <gs++/output.hpp>
#include <gs++/pk_script.hpp>
#include <gs++/utxodb.hpp>
#include <gs++/util.hpp>
#include <gs++/rpc.hpp>
#include <gs++/transaction.hpp>

#include <3rdparty/picosha2.h>


namespace gs {

utxodb::utxodb()
: current_block_height(0)
, current_block_hash("0000000000000000000000000000000000000000000000000000000000000000")
{}

bool utxodb::load_from_bchd_checkpoint (
    const std::string & path,
    const std::uint32_t block_height,
    const std::string block_hash
) {
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

    current_block_height = block_height;
    current_block_hash   = block_hash;

    return true;
}


void utxodb::process_block(
    gs::rpc & rpc,
    const std::uint32_t height,
    const bool save_rollback
) {
    std::vector<std::uint8_t> block_data = rpc.get_raw_block(600000);

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
        gs::transaction tx(it, height);

        for (auto & m : tx.inputs) {
            blk_inputs.emplace_back(m);
        }

        for (auto & m : tx.outputs) {
            blk_outputs.emplace_back(m);
        }
    }

    // only used if save_rollback enabled
    std::vector<gs::outpoint> this_block_added;
    std::vector<gs::output>   this_block_removed;

    for (auto & m : blk_outputs) {
        const gs::outpoint outpoint(m.prev_tx_id, m.prev_out_idx);
        gs::output* const oid = &(*outpoint_map.insert({ outpoint, m }).first).second;

		if (! pk_script_to_output.count(m.pk_script)) {
			pk_script_to_output.insert({ m.pk_script, { oid } });
		} else {
			pk_script_to_output[m.pk_script].emplace(oid);
		}

        if (save_rollback) {
            this_block_added.push_back(outpoint);
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
        if (save_rollback) {
            this_block_removed.push_back(o);
        }
        if (addr_map.erase(&o)) {
            // std::cout << height << "\tremoved: " << m.txid.decompress() << ":" << m.vout << "\n";
        }
        if (addr_map.empty()) {
            pk_script_to_output.erase(pk_script);
        }
    }

    if (save_rollback) {
        last_block_added.push_back(this_block_added);
        last_block_removed.push_back(this_block_removed);

        if (last_block_added.size() > rollback_depth) {
            last_block_added.pop_front();
        }
        if (last_block_removed.size() > rollback_depth) {
            last_block_removed.pop_front();
        }
    }
}

void utxodb::rollback()
{
    if (last_block_removed.empty() || last_block_added.empty()) {
        return;
    }

    const std::vector<gs::output> rb_removed = last_block_removed.back();
    const std::vector<gs::outpoint> rb_added = last_block_added.back();
    last_block_removed.pop_back();
    last_block_added.pop_back();

    for (auto m : rb_removed) {
        gs::output* const oid = &(*outpoint_map.insert({
            gs::outpoint(m.prev_tx_id, m.prev_out_idx),
			m
		}).first).second;

		if (! pk_script_to_output.count(m.pk_script)) {
			pk_script_to_output.insert({ m.pk_script, { oid } });
		} else {
			pk_script_to_output[m.pk_script].emplace(oid);
		}

        // std::cout << "\tadded: " << m.prev_tx_id.decompress() << ":" << m.prev_out_idx << "\n";
    }

    for (auto m : rb_added) {
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
            // std::cout << "\tremoved: " << m.txid.decompress() << ":" << m.vout << "\n";
        }
        if (addr_map.empty()) {
            pk_script_to_output.erase(pk_script);
        }
    }
}

void utxodb::process_mempool_tx(const std::vector<std::uint8_t>& msg_data)
{
    gs::transaction tx(msg_data.begin(), 0);
    std::cout << "txid: " << tx.txid.decompress() << std::endl;
    std::cout << "\tversion: " << tx.version << std::endl;
    std::cout << "\tlock_time: " << tx.lock_time << std::endl;

    for (auto & m : tx.outputs) {
        std::cout << "\toutput txid: " << m.prev_tx_id.decompress() << "\t" << m.prev_out_idx << std::endl; 


        const gs::outpoint outpoint(m.prev_tx_id, m.prev_out_idx);
        gs::output* const oid = &(*mempool_outpoint_map.insert({ outpoint, m }).first).second;

        if (! pk_script_to_output.count(m.pk_script)) {
            mempool_pk_script_to_output.insert({ m.pk_script, { oid } });
        } else {
            mempool_pk_script_to_output[m.pk_script].emplace(oid);
        }
    }

    for (auto & m : tx.inputs) {
        std::cout << "\tinput txid: " << m.txid.decompress() << ":" << m.vout << "\n";

        if (mempool_outpoint_map.count(m) == 0) {
            mempool_spent_confirmed_outpoints.insert(m);
            continue;
        }

        const gs::output& o           = mempool_outpoint_map.at(m);
        const gs::pk_script pk_script = o.pk_script;

        if (! mempool_pk_script_to_output.count(pk_script)) {
            continue;
        }
        absl::flat_hash_set<gs::output*> & addr_map = mempool_pk_script_to_output.at(pk_script);
        if (addr_map.erase(&o)) {
            // std::cout << height << "\tremoved: " << m.txid.decompress() << ":" << m.vout << "\n";
        }
        if (addr_map.empty()) {
            mempool_pk_script_to_output.erase(pk_script);
        }
    }
}

}


int main()
{
    gs::utxodb utxodb;
    
    zmq::context_t context(1);
    zmq::socket_t sock(context, ZMQ_SUB);
    sock.connect("tcp://127.0.0.1:28332");
    sock.setsockopt(ZMQ_SUBSCRIBE, "rawtx", strlen("rawtx"));

    while(true)
    {
        zmq::message_t env;
        sock.recv(&env);
        std::string env_str = std::string(static_cast<char*>(env.data()), env.size());

        if (env_str == "rawtx") {
            std::cout << "Received envelope '" << env_str << "'" << std::endl;

            zmq::message_t msg;
            sock.recv(&msg);

            std::vector<std::uint8_t> msg_data;
            msg_data.reserve(msg.size());

            std::copy(
                static_cast<std::uint8_t*>(msg.data()),
                static_cast<std::uint8_t*>(msg.data())+msg.size(),
                std::back_inserter(msg_data)
            );

            utxodb.process_mempool_tx(msg_data);
        }
    }




    gs::rpc rpc("0.0.0.0", 8332, "user", "password919191828282777wq");
    // utxodb.process_block(rpc, 600000);

    /*
    utxodb.load_from_bchd_checkpoint(
		"../utxo-checkpoints/QmXkBQJrMKkCKNbwv4m5xtnqwU9Sq7kucPigvZW8mWxcrv",
		582680, "0000000000000000000000000000000000000000000000000000000000000000"
	);*/

    // for (std::uint32_t h=582680; h<602365; ++h) {
    for (std::uint32_t h=582680; h<582780; ++h) {
        std::cout << h << std::endl;
        utxodb.process_block(rpc, h, h>582760);
    }

    std::cout << "s1: " << utxodb.outpoint_map.size() << std::endl;
    utxodb.rollback();
    utxodb.rollback();
    utxodb.rollback();
    utxodb.process_block(rpc, 582757, true);
    utxodb.process_block(rpc, 582758, true);
    utxodb.process_block(rpc, 582759, true);
    std::cout << "s2: " << utxodb.outpoint_map.size() << std::endl;

    return 0;
}
