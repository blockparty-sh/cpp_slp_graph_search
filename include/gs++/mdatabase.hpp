#ifndef GS_MDATABASE_HPP
#define GS_MDATABASE_HPP

#include <vector>
#include <string>
#include <atomic>
#include <cstdint>
#include <absl/container/flat_hash_map.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/string/to_string.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/instance.hpp>
#include "transaction.hpp"
#include "txhash.hpp"
#include "txgraph.hpp"

struct mdatabase
{
    mongocxx::instance inst{};
    mongocxx::pool pool{mongocxx::uri{}};
    const std::string db_name;

    mdatabase(const std::string db_name)
    : inst{}
    , pool{mongocxx::uri{}}
    , db_name(db_name)
    {}

    std::vector<txhash> get_all_token_ids();

    int get_current_block_height();

    void watch_for_status_update(
        txgraph & g,
        std::atomic<int> & current_block_height,
        const std::atomic<bool> & continue_watching_mongo
    );

    std::vector<transaction> load_token(
        const txhash tokenid,
        const int max_block_height
    );

    absl::flat_hash_map<txhash, std::vector<transaction>> load_block(
        const int block_height
    ); 
};

#endif
