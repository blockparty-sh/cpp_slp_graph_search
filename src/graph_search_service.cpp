#include <sstream>
#include <chrono>
#include <vector>
#include <string>
#include <iostream>
#include "graph_search_service.hpp"
#include "gs++.hpp"
#include "txhash.hpp"

grpc::Status GraphSearchServiceImpl::GraphSearch (
    grpc::ServerContext* context,
    const graphsearch::GraphSearchRequest* request,
    graphsearch::GraphSearchReply* reply
) {
    const txhash lookup_txid = request->txid();

    std::stringstream ss;
    ss << "lookup: " << lookup_txid;
    reply->add_txdata();

    const auto start = std::chrono::steady_clock::now();
    std::vector<std::string> result = graph_search__ptr(lookup_txid);
    for (auto i : result) {
        reply->add_txdata(i);
    }
    const auto end = std::chrono::steady_clock::now();
    const auto diff = end - start;

    ss  << "\t" << std::chrono::duration <double, std::milli> (diff).count() << " ms "
        << "(" << result.size() << ")"
        << std::endl;

    std::cout << ss.str();

    return grpc::Status::OK;
}
