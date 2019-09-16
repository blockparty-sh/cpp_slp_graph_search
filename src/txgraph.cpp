#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>
#include "gs++.hpp"
#include "graph_node.hpp"
#include "token_details.hpp"
#include "txhash.hpp"
#include "transaction.hpp"
#include "txgraph.hpp"

void txgraph::recursive_walk__ptr (
    const graph_node* node,
    absl::flat_hash_set<const graph_node*> & seen
) {
    for (const graph_node* n : node->inputs) {
        if (! seen.count(n)) {
            seen.insert(n);
            recursive_walk__ptr(n, seen);
        }
    }
}

std::vector<std::string> txgraph::graph_search__ptr(const txhash lookup_txid)
{
    std::shared_lock lock(lookup_mtx);

    if (txid_to_token.count(lookup_txid) == 0) {
        // txid hasn't entered our system yet
        return {};
    }

    token_details* token = txid_to_token[lookup_txid];

    absl::flat_hash_set<const graph_node*> seen;

    if (token->graph.count(lookup_txid) == 0) {
        std::stringstream ss;
        ss << "graph_search__ptr: txid not found in tokengraph " << lookup_txid
           << "\n";
        std::cerr << ss.str();
        return {};
    }
    recursive_walk__ptr(&token->graph[lookup_txid], seen);

    std::vector<std::string> ret;
    ret.reserve(seen.size());

    for (auto it = std::begin(seen); it != std::end(seen); ) {
        ret.emplace_back(std::move(seen.extract(it++).value())->txdata);
    }

    return ret;
}

/* TODO
void txgraph::clear_token_data (const txhash tokenid)
{
    if (tokens.count(tokenid)) {
        tokens.erase(tokenid);
    }
}
*/

unsigned txgraph::insert_token_data (
    const txhash tokenid,
    std::vector<transaction> txs
) {
    std::lock_guard lock(lookup_mtx);

    token_details& token = tokens[tokenid];

    if (! tokens.count(tokenid)) {
        tokens.insert({ tokenid, token_details(tokenid) });
    }

    absl::flat_hash_map<txhash, std::vector<txhash>> input_map;

    unsigned ret = 0;

    // first pass to populate graph nodes
    std::vector<graph_node*> latest;
    latest.reserve(txs.size());
    for (auto tx : txs) {
        if (! txid_to_token.count(tx.txid)) {
            token.graph.insert({ tx.txid, graph_node(tx.txid, tx.txdata) });
            txid_to_token.insert({ tx.txid, &token });
            input_map.insert({ tx.txid, tx.inputs });
            latest.emplace_back(&token.graph[tx.txid]);
            ++ret;
        }
    }

    // second pass to add inputs
    for (graph_node * node : latest) {
        for (const txhash input_txid : input_map[node->txid]) {
            if (! token.graph.count(input_txid)) {
                /*
                std::stringstream ss;
                ss << "insert_token_data: input_txid not found in tokengraph " << input_txid
                   << "\n";
                std::cerr << ss.str();
                */
                continue;
            }

            node->inputs.emplace_back(&token.graph[input_txid]);
        }
    }

    return ret;
}

bool txgraph::save_token_to_disk(const txhash tokenid)
{
    std::shared_lock lock(lookup_mtx);
    std::cout << "saving token to disk" << tokenid;

    const std::filesystem::path tokendir = get_tokendir(tokenid);
    std::filesystem::create_directories(tokendir);

    const std::filesystem::path tokenpath(tokendir / tokenid);
    std::ofstream outf(tokenpath, std::ofstream::binary);

    for (auto it : tokens[tokenid].graph) {
        auto node = it.second;
        outf.write(node.txid.data(), node.txid.size());

        const std::size_t txdata_size = node.txdata.size();
        outf.write(reinterpret_cast<const char *>(&txdata_size), sizeof(std::size_t));

        outf.write(node.txdata.data(), node.txdata.size());

        const std::size_t inputs_size = node.inputs.size();
        outf.write(reinterpret_cast<const char *>(&inputs_size), sizeof(std::size_t));

        for (graph_node* input : node.inputs) {
            outf.write(input->txid.data(), input->txid.size());
        }
    }

    std::cout << "\t done" << tokenpath << std::endl;

    return true;
}

std::vector<transaction> txgraph::load_token_from_disk(const txhash tokenid)
{
    std::shared_lock lock(lookup_mtx);
    constexpr std::size_t txid_size = 64;

    std::filesystem::path tokenpath = get_tokendir(tokenid) / tokenid;
    std::ifstream file(tokenpath, std::ios::binary);
    std::cout << "loading token from disk: " << tokenpath << std::endl;
    std::vector<std::uint8_t> fbuf(std::istreambuf_iterator<char>(file), {});
    std::vector<transaction> ret;


    auto it = std::begin(fbuf);
    while (it != std::end(fbuf)) {
        txhash txid(txid_size, '\0');
        std::copy(it, it+txid_size, std::begin(txid));
        it += txid_size;

        std::size_t txdata_size;
        std::copy(it, it+sizeof(std::size_t), reinterpret_cast<char*>(&txdata_size));
        it += sizeof(std::size_t);

        std::string txdata(txdata_size, '\0');
        std::copy(it, it+txdata_size, std::begin(txdata));
        it += txdata_size;

        std::size_t inputs_size;
        std::copy(it, it+sizeof(inputs_size), reinterpret_cast<char*>(&inputs_size));
        it += sizeof(inputs_size);

        std::vector<txhash> inputs;
        inputs.reserve(inputs_size);
        for (std::size_t i=0; i<inputs_size; ++i) {
            txhash input(txid_size, '\0');
            std::copy(it, it+txid_size, std::begin(input));
            it += txid_size;
            inputs.emplace_back(input);
        }

        ret.emplace_back(transaction(txid, txdata, inputs));
    }
    file.close();

    return ret;
}
