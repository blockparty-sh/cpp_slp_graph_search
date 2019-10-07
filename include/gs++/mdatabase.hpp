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
#include "gs_tx.hpp"
#include "bhash.hpp"
#include "txgraph.hpp"

namespace gs {

struct mdatabase
{
    mongocxx::instance inst{};
    mongocxx::pool pool{mongocxx::uri{}};
    const std::string db_name;

    mdatabase(
        const std::string& db_name,
        const std::string& uri
    )
    : inst{}
    , pool{uri.empty() ? mongocxx::uri{} : mongocxx::uri{uri}}
    , db_name(db_name)
    {}

    std::vector<gs::tokenid> get_all_token_ids();

    int get_current_block_height(bool & running);

    void watch_for_status_update(
        txgraph & g,
        std::atomic<int> & current_block_height,
        const std::atomic<bool> & continue_watching_mongo
    );

    std::vector<gs_tx> load_token(
        const gs::tokenid tokenid,
        const int max_block_height
    );

    absl::flat_hash_map<gs::tokenid, std::vector<gs_tx>> load_block(
        const int block_height,
        bool & success
    ); 
};

}

#endif
