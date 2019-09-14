#ifndef GS_GRAPHSEARCHSERVICE_HPP
#define GS_GRAPHSEARCHSERVICE_HPP

#include <grpc++/grpc++.h>
#include "helloworld.grpc.pb.h"

class GraphSearchServiceImpl final
 : public graphsearch::GraphSearchService::Service
{
    grpc::Status GraphSearch (
        grpc::ServerContext* context,
        const graphsearch::GraphSearchRequest* request,
        graphsearch::GraphSearchReply* reply
    ) override;
};

#endif
