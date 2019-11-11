#include <string>
#include <vector>
#include <fstream>
#include <boost/thread.hpp>
#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>
#include <spdlog/spdlog.h>
#include <gs++/graph_node.hpp>
#include <gs++/token_details.hpp>
#include <gs++/bhash.hpp>
#include <gs++/gs_tx.hpp>
#include <gs++/txgraph.hpp>

namespace gs {

void txgraph::recursive_walk__ptr (
    const graph_node* node,
    absl::flat_hash_set<const graph_node*> & seen
) const {
    for (const graph_node* n : node->inputs) {
        if (! seen.count(n)) {
            seen.insert(n);
            recursive_walk__ptr(n, seen);
        }
    }
}

std::pair<graph_search_status, std::vector<std::string>>
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
    recursive_walk__ptr(&token->graph[lookup_txid], seen);

    std::vector<std::string> ret;
    ret.reserve(seen.size());

    for (auto it = std::begin(seen); it != std::end(seen); ) {
        ret.emplace_back(std::move(seen.extract(it++).value())->txdata);
    }

    return { graph_search_status::OK, ret };
}

/* TODO
void txgraph::clear_token_data (const gs::tokenid tokenid)
{
    if (tokens.count(tokenid)) {
        tokens.erase(tokenid);
    }
}
*/

unsigned txgraph::insert_token_data (
    const gs::tokenid & tokenid,
    const std::vector<gs::gs_tx> & txs
) {
    boost::lock_guard<boost::shared_mutex> lock(lookup_mtx);

    if (! tokens.count(tokenid)) {
        tokens.insert({ tokenid, token_details(tokenid) });
    }

    token_details& token = tokens[tokenid];

    absl::flat_hash_map<gs::txid, std::vector<gs::txid>> input_map;

    unsigned ret = 0;

    // first pass to populate graph nodes
    std::vector<graph_node*> latest;
    latest.reserve(txs.size());

    for (auto & tx : txs) {
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
        for (const gs::txid & input_txid : input_map[node->txid]) {
            if (! token.graph.count(input_txid)) {
                spdlog::warn("insert_token_data: input_txid not found in tokengraph {}", input_txid.decompress());
                continue;
            }

            node->inputs.emplace_back(&token.graph[input_txid]);
        }
    }

    return ret;
}

}
