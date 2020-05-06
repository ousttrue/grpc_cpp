# grpc_cpp for Windows10

## setup grpc library using vcpkg

* install vcpkg

```
$ vcpkg install grpc
```

* set environment variable VCPKG_DIR to vcpkg folder.

## build tutorial

[gRPC Basics - C++](https://grpc.io/docs/tutorials/basic/cpp/)

### copy sources

* protos/route_guide.proto
* route_guide_client.cc
* route_guide_server.cc
* helper.cc
* helper.h

### generate protobuf

protoc.exe and grpc_cpp_plugin.exe is found in $env:VCPKG_DIR/packages

* $env:VCPKG_DIR\packages\protobuf_x64-windows\tools\protobuf\protoc.exe --cpp_out=. ../../protos/route_guide.proto
* $env:VCPKG_DIR\packages\protobuf_x64-windows\tools\protobuf\protoc.exe --grpc_out=. --plugin=protoc-gen-grpc=$env:VCPKG_DIR\packages\grpc_x64-windows\tools\grpc\grpc_cpp_plugin.exe ../../protos/route_guide.proto
 
### build

### `Please compile grpc with _WIN32_WINNT of at least 0x600 (aka Windows Vista)`

define `_WIN32_WINNT=0x0A00` like.

### Could not find a package configuration file provided by "gRPC"

```json
    "cmake.configureArgs": [
        "-DCMAKE_PREFIX_PATH=${env:VCPKG_DIR}/installed/x64-windows"
    ],
```
