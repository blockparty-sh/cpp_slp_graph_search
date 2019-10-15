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
    ) {
        nlohmann::json robj {{
            { "jsonrpc", "2.0" },
            { "method", method },
            { "params", params },
            { "id", 0 }
        }};

        return cli.Post("/", {
                httplib::make_basic_authentication_header(rpc_user, "password919191828282777wq")
            },
            robj.dump(),
            "text/plain"
        );
    }

    std::pair<bool, std::vector<std::uint8_t>> get_raw_block(
        const std::size_t height
    ) {
        std::string block_hash;
        {
            std::shared_ptr<httplib::Response> res = query("getblockhash", nlohmann::json::array({ height }));
            if (! res) {
                return { false, {} };
            }

            if (res->status != 200) {
                return { false, {} };
            }

            auto jbody = nlohmann::json::parse(res->body);
            // std::cout << jbody << std::endl;

            if (jbody.size() == 0) {
                return { false, {} };
            }

            if (! jbody[0]["error"].is_null()) {
                std::cerr << jbody[0]["error"] << "\n";
                return { false, {} };
            }

            block_hash = jbody[0]["result"].get<std::string>();
        }

        std::string block_data_str;
        {
            std::shared_ptr<httplib::Response> res = query("getblock", nlohmann::json::array({ block_hash, 0 }));
            if (! res) {
                return { false, {} };
            }

            if (res->status != 200) {
                return { false, {} };
            }

            auto jbody = nlohmann::json::parse(res->body);
            // std::cout << jbody << std::endl;

            if (jbody.size() == 0) {
                return { false, {} };
            }
            if (! jbody[0]["error"].is_null()) {
                std::cerr << jbody[0]["error"] << "\n";
                return { false, {} };
            }

            block_data_str = jbody[0]["result"].get<std::string>();
        }

        return { true, gs::util::compress_hex(block_data_str) };
    }

    std::pair<bool, std::uint32_t> get_best_block_height()
    {
        std::shared_ptr<httplib::Response> res = query("getblockchaininfo", {});

        if (! res) {
            return { false, {} };
        }

        if (res->status != 200) {
            return { false, {} };
        }

        auto jbody = nlohmann::json::parse(res->body);

        if (jbody.size() == 0) {
            return { false, {} };
        }
        if (! jbody[0]["error"].is_null()) {
            std::cerr << jbody[0]["error"] << "\n";
            return { false, {} };
        }

        // std::cout << jbody << std::endl;
        return { true, jbody[0]["result"]["blocks"].get<std::uint32_t>() };
    }
};

}


#endif
