#include <iostream>
#include <memory>
#include <chrono>
#include <string>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <getopt.h>

#include <grpc++/grpc++.h>
#include <libbase64.h>
#include "graphsearch.grpc.pb.h"
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/numeric/int128.h>

#include <gs++/transaction.hpp>
#include <gs++/bhash.hpp>
#include <gs++/scriptpubkey.hpp>

#include <gs++/cslp.h>


class GraphSearchServiceClient
{
public:
    GraphSearchServiceClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(graphsearch::GraphSearchService::NewStub(channel))
    {}

    bool GraphSearch(const std::string& txid)
    {
        graphsearch::GraphSearchRequest request;
        request.set_txid(txid);

        graphsearch::GraphSearchReply reply;

        grpc::ClientContext context;
        grpc::Status status = stub_->GraphSearch(&context, request, &reply);

        if (status.ok()) {
            for (auto n : reply.txdata()) {
                // 4/3 is recommended size with a bit of a buffer
                std::string b64(n.size()*1.5, '\0');
                std::size_t b64_len = 0;
                base64_encode(n.data(), n.size(), const_cast<char*>(b64.data()), &b64_len, 0);
                b64.resize(b64_len);
                std::cout << b64 << "\n";
            }

            return true;
        } else {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return false;
        }
    }

    bool GraphSearchValidate(const std::string& txid_str)
    {
        graphsearch::GraphSearchRequest request;
        request.set_txid(txid_str);

        graphsearch::GraphSearchReply reply;

        grpc::ClientContext context;
        grpc::Status status = stub_->GraphSearch(&context, request, &reply);


        if (status.ok()) {
            cslp_validator validator = cslp_validator_init();
            {
                auto start = std::chrono::high_resolution_clock::now();
                for (auto & n : reply.txdata()) {
                    cslp_validator_add_tx(validator, n.data());
                }
                auto end = std::chrono::high_resolution_clock::now();
                std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms\n";
            }

            gs::txid txid(txid_str);
            std::reverse(txid.v.begin(), txid.v.end());

            {
                auto start = std::chrono::high_resolution_clock::now();
                const bool valid = cslp_validator_validate(validator, reinterpret_cast<const char*>(txid.v.data()));
                std::cout << txid.decompress(true) << ": " << ((valid) ? "valid" : "invalid") << "\n";
                auto end = std::chrono::high_resolution_clock::now();
                std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms\n";
            }

            cslp_validator_destroy(validator);
            return true;
        } else {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return false;
        }
    }

    bool UtxoSearchByOutpoints(
        const std::vector<std::pair<std::string, std::uint32_t>> outpoints
    ) {
        graphsearch::UtxoSearchByOutpointsRequest request;

        for (auto o : outpoints) {
            auto * outpoint = request.add_outpoints();
            outpoint->set_txid(o.first);
            outpoint->set_vout(o.second);
        }

        graphsearch::UtxoSearchReply reply;

        grpc::ClientContext context;
        grpc::Status status = stub_->UtxoSearchByOutpoints(&context, request, &reply);

        if (status.ok()) {
            for (auto n : reply.outputs()) {
                const std::string   prev_tx_id_str = n.prev_tx_id();
                const std::uint32_t prev_out_idx   = n.prev_out_idx();
                const std::uint32_t height         = n.height();
                const std::uint64_t value          = n.value();
                const std::string   scriptpubkey_str  = n.scriptpubkey();

                gs::txid prev_tx_id(prev_tx_id_str);

                std::string scriptpubkey_b64(scriptpubkey_str.size()*1.5, '\0');
                std::size_t scriptpubkey_b64_len = 0;
                base64_encode(
                    scriptpubkey_str.data(),
                    scriptpubkey_str.size(),
                    const_cast<char*>(scriptpubkey_b64.data()),
                    &scriptpubkey_b64_len,
                    0
                );
                scriptpubkey_b64.resize(scriptpubkey_b64_len);

                std::cout
                    << prev_tx_id.decompress(true) << ":" << prev_out_idx << "\n"
                    << "\theight:       " << height                       << "\n"
                    << "\tvalue:        " << value                        << "\n"
                    << "\tscriptpubkey:    " << scriptpubkey_b64                << "\n";
            }

            return true;
        } else {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return false;
        }
    }

    bool UtxoSearchByScriptPubKey(const gs::scriptpubkey scriptpubkey)
    {
        graphsearch::UtxoSearchByScriptPubKeyRequest request;
        request.set_scriptpubkey(std::string(scriptpubkey.v.begin(), scriptpubkey.v.end()));

        graphsearch::UtxoSearchReply reply;

        grpc::ClientContext context;
        grpc::Status status = stub_->UtxoSearchByScriptPubKey(&context, request, &reply);

        if (status.ok()) {
            for (auto n : reply.outputs()) {
                const std::string   prev_tx_id_str = n.prev_tx_id();
                const std::uint32_t prev_out_idx   = n.prev_out_idx();
                const std::uint32_t height         = n.height();
                const std::uint64_t value          = n.value();
                const std::string   scriptpubkey_str  = n.scriptpubkey();

                gs::txid prev_tx_id(prev_tx_id_str);

                std::string scriptpubkey_b64(scriptpubkey_str.size()*1.5, '\0');
                std::size_t scriptpubkey_b64_len = 0;
                base64_encode(
                    scriptpubkey_str.data(),
                    scriptpubkey_str.size(),
                    const_cast<char*>(scriptpubkey_b64.data()),
                    &scriptpubkey_b64_len,
                    0
                );
                scriptpubkey_b64.resize(scriptpubkey_b64_len);

                std::cout
                    << prev_tx_id.decompress(true) << ":" << prev_out_idx << "\n"
                    << "\theight:       " << height                       << "\n"
                    << "\tvalue:        " << value                        << "\n";
            }

            return true;
        } else {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return false;
        }
    }

    bool BalanceByScriptPubKey(const gs::scriptpubkey scriptpubkey)
    {
        graphsearch::BalanceByScriptPubKeyRequest request;
        request.set_scriptpubkey(std::string(scriptpubkey.v.begin(), scriptpubkey.v.end()));

        graphsearch::BalanceByScriptPubKeyReply reply;

        grpc::ClientContext context;
        grpc::Status status = stub_->BalanceByScriptPubKey(&context, request, &reply);

        if (status.ok()) {
            const std::uint64_t balance = reply.balance();
            std::cout << balance << "\n";
            return true;
        } else {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return false;
        }
    }

private:
    std::unique_ptr<graphsearch::GraphSearchService::Stub> stub_;
};

int main(int argc, char* argv[])
{
    std::string grpc_host = "0.0.0.0";
    std::string grpc_port = "50051";
    std::string query_type = "graphsearch";

    const std::string usage_str = "usage: gs++-cli [--version] [--help] [--host host_address] [--port port]\n"
                                  "[--graphsearch TXID] [--utxo TXID:VOUT] [--utxo_scriptpubkey PK]\n"
                                  "[--balance_scriptpubkey PK] [--validate TXID]\n";

    while (true) {
        static struct option long_options[] = {
            { "help",    no_argument,       nullptr, 'h' },
            { "version", no_argument,       nullptr, 'v' },
            { "host",    required_argument, nullptr, 'b' },
            { "port",    required_argument, nullptr, 'p' },

            { "graphsearch",          no_argument, nullptr, 1000 },
            { "utxo",                 no_argument, nullptr, 1001 },
            { "utxo_scriptpubkey",    no_argument, nullptr, 1002 },
            { "balance_scriptpubkey", no_argument, nullptr, 1003 },
            { "validate",             no_argument, nullptr, 1004 },
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "hvb:p:", long_options, &option_index);

        if (c == -1) {
            break;
        }

        std::stringstream ss(optarg != nullptr ? optarg : "");
        switch (c) {
            case 0:
                if (long_options[option_index].flag != 0) {
                    break;
                }

                break;
            case 'h':
                std::cout << usage_str;
                return EXIT_SUCCESS;
            case 'v':
                std::cout <<
                    "gs++-cli v" << GS_VERSION << std::endl;
                return EXIT_SUCCESS;
            case 'b': ss >> grpc_host; break;
            case 'p': ss >> grpc_port; break;

            case 1000: query_type = "graphsearch";          break;
            case 1001: query_type = "utxo";                 break;
            case 1002: query_type = "utxo_scriptpubkey";    break;
            case 1003: query_type = "balance_scriptpubkey"; break;
            case 1004: query_type = "validate";             break;

            case '?':
                return EXIT_FAILURE;
            default:
                return EXIT_FAILURE;
        }
    }
    if (argc < 2) {
        std::cout << usage_str;
        return EXIT_FAILURE;
    }

    grpc::ChannelArguments ch_args;
    ch_args.SetMaxReceiveMessageSize(-1);

    GraphSearchServiceClient graphsearch(
        grpc::CreateCustomChannel(
            grpc_host+":"+grpc_port,
            grpc::InsecureChannelCredentials(),
            ch_args
        )
    );

    if (query_type == "graphsearch") {
        graphsearch.GraphSearch(argv[argc-1]);
    } else if (query_type == "validate") {
        /*
        if (optind >= argc) {
            std::cerr << "validate requires TXID argument\n";
            return EXIT_FAILURE;
        }*/
        graphsearch.GraphSearchValidate(argv[argc-1]);
    } else if (query_type == "utxo") {
        std::vector<std::pair<std::string, std::uint32_t>> outpoints;
        for (int optidx=optind; optidx < argc; ++optidx) {
            std::vector<std::string> seglist;
            {
                std::stringstream ss(argv[optidx]);
                std::string segment;

                while (std::getline(ss, segment, ':')) {
                    seglist.push_back(segment);
                }
            }

            if (seglist.size() != 2) {
                std::cerr << "bad format: expected TXID:VOUT\n";
                return EXIT_FAILURE;
            }

            gs::txid txid(seglist[0]);
            std::reverse(txid.v.begin(), txid.v.end());
            std::uint32_t vout = 0;
            {
                std::stringstream ss(seglist[1]);
                ss >> vout;
            }

            outpoints.push_back({ txid.decompress(), vout });
        }

        graphsearch.UtxoSearchByOutpoints(outpoints);
    } else if (query_type == "utxo_scriptpubkey") {
        const std::string scriptpubkey_b64 = argv[optind];

        std::size_t scriptpubkey_len = 0;
        std::string decoded(scriptpubkey_b64.size(), '\0');
        base64_decode(
            scriptpubkey_b64.data(),
            scriptpubkey_b64.size(),
            reinterpret_cast<char*>(decoded.data()),
            &scriptpubkey_len,
            0
        );
        decoded.resize(scriptpubkey_len);

        graphsearch.UtxoSearchByScriptPubKey(gs::scriptpubkey(decoded));
    } else if (query_type == "balance_scriptpubkey") {
        const std::string scriptpubkey_b64 = argv[optind];

        std::size_t scriptpubkey_len = 0;
        std::string decoded(scriptpubkey_b64.size(), '\0');
        base64_decode(
            scriptpubkey_b64.data(),
            scriptpubkey_b64.size(),
            reinterpret_cast<char*>(decoded.data()),
            &scriptpubkey_len,
            0
        );
        decoded.resize(scriptpubkey_len);

        graphsearch.BalanceByScriptPubKey(gs::scriptpubkey(decoded));
    }

    return EXIT_SUCCESS;
}

