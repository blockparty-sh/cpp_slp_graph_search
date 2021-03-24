package main

import (
	"flag"
	"net/http"
	"mime"
	"path/filepath"
	"os"
	"log"

	"github.com/golang/glog"
	"github.com/grpc-ecosystem/grpc-gateway/v2/runtime"
	"golang.org/x/net/context"
	"google.golang.org/grpc"

	gw "main/gen"
)

var (
	// command-line options:
	// gRPC server endpoint
	grpcServerEndpoint = flag.String("gs-grpc-endpoint", "localhost:50051", "gs++ gRPC server endpoint")
	proxyPort          = flag.String("port", "8082", "port for the proxy server")
)

func run() error {
	ctx := context.Background()
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()

	mux := http.NewServeMux()

	// Register gRPC server endpoint
	// Note: Make sure the gRPC server is running properly and accessible
	gwmux := runtime.NewServeMux()
	opts := []grpc.DialOption{grpc.WithInsecure(), grpc.WithMaxMsgSize(4294967295)}
	err := gw.RegisterGraphSearchServiceHandlerFromEndpoint(ctx, gwmux, *grpcServerEndpoint, opts)
	if err != nil {
		return err
	}

	mux.Handle("/v1/", gwmux)
	serveSwagger(mux)

	// Start HTTP server (and proxy calls to gRPC server endpoint)
	return http.ListenAndServe(":"+*proxyPort, mux)
}

func serveSwagger(mux *http.ServeMux) {
	mime.AddExtensionType(".svg", "image/svg+xml")

	dir, err := filepath.Abs(filepath.Dir(os.Args[0]))
	if err != nil {
					log.Fatal(err)
	}
	dir = filepath.Join(dir, "web")

	fileServer := http.FileServer(http.Dir(dir))
	prefix := "/"
	mux.Handle(prefix, http.StripPrefix(prefix, fileServer))
}

func main() {
	flag.Parse()
	defer glog.Flush()

	if err := run(); err != nil {
		glog.Fatal(err)
	}
}
