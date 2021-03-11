all: gw web/graphsearch.swagger.json

gw: gen/graphsearch.pb.go gen/graphsearch.pb.gw.go
	go build -o gw -v

gen/graphsearch.pb.go: ../../pb/graphsearch.proto ../../pb/graphsearch.yaml
	protoc \
		-I/usr/local/include \
		-I../../pb \
		-I${GOPATH}/src \
		-I${GOPATH}/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--go_out=gen --go-grpc_out=gen \
		../../pb/graphsearch.proto

web/graphsearch.swagger.json: ../../pb/graphsearch.proto ../../pb/graphsearch.yaml
	protoc \
		-I/usr/local/include \
		-I../../pb \
		-I${GOPATH}/src \
		-I${GOPATH}/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--openapiv2_out ./web --openapiv2_opt logtostderr=true \
		../../pb/graphsearch.proto

gen/graphsearch.pb.gw.go: ../../pb/graphsearch.proto ../../pb/graphsearch.yaml
	protoc \
		-I/usr/local/include \
		-I../../pb \
		-I${GOPATH}/src \
		-I${GOPATH}/src/github.com/grpc-ecosystem/grpc-gateway/third_party/googleapis \
		--grpc-gateway_out=logtostderr=true,grpc_api_configuration=../../pb/graphsearch.yaml:gen \
		../../pb/graphsearch.proto
