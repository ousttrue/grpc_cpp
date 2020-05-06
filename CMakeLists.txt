cmake_minimum_required(VERSION 3.0.0)
project(grpc_tutorial)

find_package(gRPC CONFIG REQUIRED)  
# find_package(modules CONFIG REQUIRED)

subdirs(server)
