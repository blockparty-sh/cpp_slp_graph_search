#ifndef GS_UTXODB_HPP
#define GS_UTXODB_HPP

#include <string>
#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <gs++/output.hpp>

namespace gs {

struct utxodb
{
    absl::node_hash_map<gs::outpoint, gs::output> outpoint_map;
    absl::flat_hash_map<gs::pk_script, absl::flat_hash_set<gs::output*>> pk_script_to_output;

    bool load_from_bchd_checkpoint(const std::string & path);

    std::vector<std::uint8_t> get_raw_block(
        httplib::Client & cli,
        const std::size_t height
    );
};

}

#endif
