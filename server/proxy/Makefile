all: gen/graphsearch.pb.go gen/graphsearch.pb.gw.go gen/graphsearch.swagger.json

gen/graphsearch.pb.go: ../../pb/graphsearch.proto ../../pb/graphsearch.yaml
	protoc \
		-I/usr/local/include \
		-I../../pb \
		-I${GOPATH}/src \
		-I${GOPATH}/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--go_out=plugins=grpc:gen \
		../../pb/graphsearch.proto

gen/graphsearch.swagger.json: ../../pb/graphsearch.proto ../../pb/graphsearch.yaml
	protoc \
		-I/usr/local/include \
		-I../../pb \
		-I${GOPATH}/src \
		-I${GOPATH}/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--swagger_out=logtostderr=true,grpc_api_configuration=../../pb/graphsearch.yaml:gen \
		../../pb/graphsearch.proto

gen/graphsearch.pb.gw.go: ../../pb/graphsearch.proto ../../pb/graphsearch.yaml
	protoc \
		-I/usr/local/include \
		-I../../pb \
		-I${GOPATH}/src \
		-I${GOPATH}/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--grpc-gateway_out=logtostderr=true,grpc_api_configuration=../../pb/graphsearch.yaml:gen \
		../../pb/graphsearch.proto
