#ifndef GS_HPP
#define GS_HPP

#include <vector>
#include <filesystem>
#include <cstdint>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <mongocxx/pool.hpp>
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
    std::vector<transaction> txs,
    const int max_block_height // -1 for no max
);
std::vector<transaction> load_token_from_disk(const txhash tokenid);
std::vector<txhash> get_all_token_ids_from_mongo(const mongocxx::database & db);
std::int32_t get_current_block_height_from_mongo(
    mongocxx::pool & pool
);
void watch_mongo_for_status_update(
    mongocxx::pool & pool,
    std::int32_t & current_block_height
);

std::vector<transaction> load_token_from_mongo(
    mongocxx::pool & pool,
    const txhash tokenid
);
absl::flat_hash_map<txhash, std::vector<transaction>> load_block_from_mongo (
    mongocxx::pool & pool,
    const std::int32_t block_height
);

void signal_handler(int signal);

#endif
