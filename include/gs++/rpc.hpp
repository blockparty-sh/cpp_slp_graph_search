#ifndef GS_RPC_HPP
#define GS_RPC_HPP

#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <httplib/httplib.h>
#include <nlohmann/json.hpp>

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

std::vector<std::uint8_t> get_raw_block(
    const std::size_t height
) {
    std::string block_hash;
    {
        std::shared_ptr<httplib::Response> res = query("getblockhash", nlohmann::json::array({ height }));
        if (res->status == 200) {
            auto jbody = nlohmann::json::parse(res->body);
            // std::cout << jbody << std::endl;

            if (jbody.size() > 0) {
                if (! jbody[0]["error"].is_null()) {
                    std::cerr << jbody[0]["error"] << "\n";
                }

                block_hash = jbody[0]["result"].get<std::string>();
            }
        }
    }

    std::string block_data_str;
    {
        std::shared_ptr<httplib::Response> res = query("getblock", nlohmann::json::array({ block_hash, 0 }));
        if (res->status == 200) {
            auto jbody = nlohmann::json::parse(res->body);

            if (jbody.size() > 0) {
                if (! jbody[0]["error"].is_null()) {
                    std::cerr << jbody[0]["error"] << "\n";
                }

                block_data_str = jbody[0]["result"].get<std::string>();
            }
        }
    }


    std::vector<std::uint8_t> block_data;
    block_data.reserve(block_data_str.size() / 2);
    for (unsigned i=0; i<block_data_str.size() / 2; ++i) {
        const std::uint8_t p1 = block_data_str[(i<<1)+0];
        const std::uint8_t p2 = block_data_str[(i<<1)+1];

        assert((p1 >= '0' && p1 <= '9') || (p1 >= 'a' && p1 <= 'f'));
        assert((p2 >= '0' && p2 <= '9') || (p2 >= 'a' && p2 <= 'f'));

        block_data[i] = ((p1 >= '0' && p1 <= '9' ? p1 - '0' : p1 - 'a' + 10) << 4)
                      +  (p2 >= '0' && p2 <= '9' ? p2 - '0' : p2 - 'a' + 10);
    }

    return block_data;
}

};

}


#endif
