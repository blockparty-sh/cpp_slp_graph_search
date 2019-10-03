#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <getopt.h>

#include <grpc++/grpc++.h>
#include <libbase64.h>
#include <gs++/bhash.hpp>
#include "graphsearch.grpc.pb.h"

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

    bool UtxoSearchByOutpoint(const std::string& txid_human, const std::uint32_t vout)
    {
        graphsearch::UtxoSearchByOutpointsRequest request;

        gs::txid txid(txid_human);
        std::reverse(txid.v.begin(), txid.v.end());

        auto * outpoint = request.add_outpoints();
        outpoint->set_txid(txid.decompress());
        outpoint->set_vout(vout);

        graphsearch::UtxoSearchReply reply;

        grpc::ClientContext context;
        grpc::Status status = stub_->UtxoSearchByOutpoints(&context, request, &reply);

        if (status.ok()) {
            for (auto n : reply.outputs()) {
                const std::string   prev_tx_id_str = n.prev_tx_id();
                const std::uint32_t prev_out_idx   = n.prev_out_idx();
                const std::uint32_t height         = n.height();
                const std::uint64_t value          = n.value();
                const std::string   pk_script_str  = n.pk_script();

                gs::txid prev_tx_id(prev_tx_id_str);

                std::string pk_script_b64(pk_script_str.size()*1.5, '\0');
                std::size_t pk_script_b64_len = 0;
                base64_encode(
                    pk_script_str.data(),
                    pk_script_str.size(),
                    const_cast<char*>(pk_script_b64.data()),
                    &pk_script_b64_len,
                    0
                );
                pk_script_b64.resize(pk_script_b64_len);

                std::cout
                    << "prev_tx_id:   " << prev_tx_id.decompress(true) << "\n"
                    << "prev_out_idx: " << prev_out_idx                << "\n"
                    << "height:       " << height                      << "\n"
                    << "value:        " << value                       << "\n"
                    << "pk_script:    " << pk_script_b64               << "\n\n";
            }

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
                                  "[--graphsearch TXID] [--utxo TXID VOUT] [--utxo_address PK]\n";

    while (true) {
        static struct option long_options[] = {
            { "help",    no_argument,       nullptr, 'h' },
            { "version", no_argument,       nullptr, 'v' },
            { "host",    required_argument, nullptr, 'b' },
            { "port",    required_argument, nullptr, 'p' },

            { "graphsearch",  no_argument,   nullptr, 1000 },
            { "utxo",         no_argument,   nullptr, 1001 },
            { "utxo_address", no_argument,   nullptr, 1002 },
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

            case 1000: query_type = "graphsearch";  break;
            case 1001: query_type = "utxo";         break;
            case 1002: query_type = "utxo_address"; break;

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
    } else if (query_type == "utxo") {
        std::string txid(argv[argc-2]);

        std::uint32_t vout = 0;
        std::stringstream ss(argv[argc-1]);
        ss >> vout;

        graphsearch.UtxoSearchByOutpoint(txid, vout);
    } else if (query_type == "utxo_address") {
        // graphsearch.UtxoSearchByAddress
    }

    return EXIT_SUCCESS;
}

