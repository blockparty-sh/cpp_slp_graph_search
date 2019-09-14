#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <getopt.h>

#include <grpc++/grpc++.h>
#include <libbase64.h>
#include "helloworld.grpc.pb.h"

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
            auto x = reply.txdata();
            
            for (auto n : x) {
                // 4/3 is recommended size with a bit of a buffer
                std::string b64(n.size()*1.5, '\0');
                std::size_t b64_len = 0;
                base64_encode(n.data(), n.size(), b64.data(), &b64_len, 0);
                b64.resize(b64_len);
                std::cout << b64 << "\n";
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
    std::string grpc_bind = "0.0.0.0";
    std::string grpc_port = "50051";

    std::string usage_str = "usage: gs++-cli [--version] [--help] [--bind_address] [--port] TXID\n";

    while (true) {
        static struct option long_options[] = {
            { "help",    no_argument,       nullptr, 'h' },
            { "version", no_argument,       nullptr, 'v' },
            { "bind",    required_argument, nullptr, 'b' },
            { "port",    required_argument, nullptr, 'p' },
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "hvb:p:", long_options, &option_index);

        if (c == -1) {
            break;
        }

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
            case 'b':
                grpc_bind = optarg;
                break;
            case 'p':
                grpc_port = optarg;
                break;
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
            grpc_bind+":"+grpc_port,
            grpc::InsecureChannelCredentials(),
            ch_args
        )
    );

    graphsearch.GraphSearch(argv[argc-1]);

    return EXIT_SUCCESS;
}

