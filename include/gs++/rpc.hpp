#ifndef GS_RPC_HPP
#define GS_RPC_HPP

#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <iostream>
#include <httplib/httplib.h>
#include <nlohmann/json.hpp>
#include <gs++/util.hpp>

namespace gs {

struct rpc
{
    std::string     rpc_user;
    std::string     rpc_pass;
    httplib::Client cli;

    rpc(
        const std::string   rpc_addr,
        const std::uint16_t rpc_port,
        const std::string   rpc_user,
        const std::string   rpc_pass
    )
    : rpc_user(rpc_user)
    , rpc_pass(rpc_pass)
    , cli(httplib::Client(rpc_addr.c_str(), rpc_port))
    {}

    std::shared_ptr<httplib::Response> query(
        const std::string & method,
        const nlohmann::json & params
    );

    std::pair<bool, std::vector<std::uint8_t>> get_raw_block(
        const std::size_t height
    );

    std::pair<bool, std::uint32_t> get_best_block_height();
};

}


#endif
