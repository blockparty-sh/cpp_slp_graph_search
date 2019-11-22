#include <string>
#include <vector>
#include <stack>
#include <fstream>
#include <iterator>
#include <algorithm>

#include <boost/thread.hpp>
#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>
#include <spdlog/spdlog.h>

#include <gs++/transaction.hpp>
#include <gs++/graph_node.hpp>
#include <gs++/token_details.hpp>
#include <gs++/bhash.hpp>
#include <gs++/txgraph.hpp>

namespace gs {

std::pair<graph_search_status, std::vector<std::vector<std::uint8_t>>>
txgraph::graph_search__ptr(const gs::txid lookup_txid)
{
    boost::shared_lock<boost::shared_mutex> lock(lookup_mtx);

    if (txid_to_token.count(lookup_txid) == 0) {
        // txid hasn't entered our system yet
        return { graph_search_status::NOT_FOUND, {} };
    }

    token_details* token = txid_to_token[lookup_txid];
    if (token->graph.count(lookup_txid) == 0) {
        return { graph_search_status::NOT_IN_TOKENGRAPH, {} };
    }

    absl::flat_hash_set<const graph_node*> seen = { &token->graph[lookup_txid] };
    std::stack<const graph_node*> stack;
    stack.push(&token->graph[lookup_txid]);
    std::vector<std::vector<std::uint8_t>> ret = { token->graph[lookup_txid].txdata };

    do {
        const graph_node* node = stack.top();
        stack.pop();

        for (const graph_node* n : node->inputs) {
            if (! seen.count(n)) {
                seen.insert(n);
                stack.push(n);
                ret.push_back(n->txdata);
            }
        }
    } while(! stack.empty());

    return { graph_search_status::OK, ret };
}

unsigned txgraph::insert_token_data (
    const gs::tokenid & tokenid,
    const std::vector<gs::transaction> & txs
) {
    boost::lock_guard<boost::shared_mutex> lock(lookup_mtx);

    if (tokens.count(tokenid) == 0) {
        tokens.emplace(tokenid, tokenid);
    }

    token_details& token = tokens[tokenid];

    absl::flat_hash_map<gs::txid, std::vector<gs::txid>> input_map;

    unsigned ret = 0;

    // first pass to populate graph nodes
    std::vector<graph_node*> latest;
    latest.reserve(txs.size());

    for (auto & tx : txs) {
        // spdlog::info("insert_token_data: txid {}", tx.txid.decompress(true));
        if (txid_to_token.count(tx.txid)) {
            spdlog::warn("insert_token_data: already in set {}", tx.txid.decompress(true));
            continue;
        }

        token.graph.emplace(std::piecewise_construct,
            std::forward_as_tuple(tx.txid),
            std::forward_as_tuple(tx.txid, tx.serialized)
        );
        txid_to_token.emplace(tx.txid, &token);

        std::vector<gs::txid> inputs;
        inputs.reserve(tx.inputs.size());
        std::transform(
            tx.inputs.begin(),
            tx.inputs.end(),
            std::back_inserter(inputs),
            [](const gs::outpoint& outpoint) -> gs::txid {
                return outpoint.txid;
            }
        );

        input_map.emplace(tx.txid, inputs);
        latest.push_back(&token.graph[tx.txid]);
        ++ret;

        // std::cout << "txid:\t" << tx.txid.decompress(true) << "\n";
    }

    // second pass to add inputs
    for (graph_node * node : latest) {
        for (const gs::txid & input_txid : input_map[node->txid]) {
            if (! token.graph.count(input_txid)) {
                // spdlog::warn("insert_token_data: input_txid not found in tokengraph {}", input_txid.decompress(true));
                continue;
            }

            node->inputs.push_back(&token.graph[input_txid]);
        }
    }

    return ret;
}

}
