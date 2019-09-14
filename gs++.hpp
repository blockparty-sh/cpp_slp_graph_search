#ifndef GS_HPP
#define GS_HPP

#include <vector>
#include <filesystem>
#include <cstdint>
#include <absl/container/flat_hash_set.h>
#include <mongocxx/client.hpp>
#include "graph_node.hpp"
#include "transaction.hpp"
#include "txhash.hpp"

void recursive_walk__ptr (
    const graph_node* node,
    absl::flat_hash_set<const graph_node*> & seen
);

std::vector<std::string> graph_search__ptr(const txhash lookup_txid);
std::filesystem::path get_tokendir(const txhash tokenid);

// TODO save writes into buffer to prevent many tiny writes
// should improve performance
bool save_token_to_disk(const txhash tokenid);

std::size_t insert_token_data (
    const txhash tokenid,
    std::vector<transaction> txs
);
std::vector<transaction> load_token_from_disk(const txhash tokenid);
std::vector<txhash> get_all_token_ids(mongocxx::database & db);

std::vector<transaction> load_token_from_mongo (
    mongocxx::database & db,
    const txhash tokenid
);

void signal_handler(int signal);

#endif
