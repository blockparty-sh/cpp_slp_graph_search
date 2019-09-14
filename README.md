gs++ | SLP Graph Search Server
===

WIP

In-memory SLP Graph Search server. 

# Install

Have SLPDB installed and running.

Install required dependencies for grpc, protobuf, etc

run `./setup.sh`

Now go into `./build/Release` or `./build/Debug`

# Running

Just view the help for the programs. Configure them to use your SLPDB mongo database.

`./gs++ -h`

After reading help, start server:

`./gs++`

Then query it:

`./gs++-cli 508e543ff30ffe670e30ebd281ab25ebe6767071e87decbb958230a7760936ae`


# Integration

You can use any grpc client to connect to a running server. It is recommended you disable max message size.
