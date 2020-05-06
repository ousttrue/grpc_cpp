cmake_minimum_required(VERSION 3.0.0)
project(grpc_tutorial)

find_package(gRPC CONFIG REQUIRED)  
# find_package(modules CONFIG REQUIRED)

#===========================================================
set(TARGET_NAME server)
add_executable(${TARGET_NAME}
    route_guide_server.cc
    helper.cc
    protos/route_guide.grpc.pb.cc
    protos/route_guide.pb.cc
    )
target_compile_options(${TARGET_NAME}
PRIVATE
    /wd4251 
    /wd4996 
    /wd4005
    )
target_include_directories(${TARGET_NAME} 
PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )
target_compile_definitions(${TARGET_NAME}
PRIVATE
    "_WIN32_WINNT=0x0A00"
    )
target_link_libraries(${TARGET_NAME} 
PRIVATE 
    # gRPC::gpr 
    # gRPC::grpc 
    gRPC::grpc++ 
    # gRPC::grpc++_alts
    )

#===========================================================
set(TARGET_NAME client)
add_executable(${TARGET_NAME}
    route_guide_client.cc
    helper.cc
    protos/route_guide.grpc.pb.cc
    protos/route_guide.pb.cc
    )
target_compile_options(${TARGET_NAME}
PRIVATE
    /wd4251 
    /wd4996 
    /wd4005
    )
target_include_directories(${TARGET_NAME} 
PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )
target_compile_definitions(${TARGET_NAME}
PRIVATE
    "_WIN32_WINNT=0x0A00"
    )
target_link_libraries(${TARGET_NAME} 
PRIVATE 
    # gRPC::gpr 
    # gRPC::grpc 
    gRPC::grpc++ 
    # gRPC::grpc++_alts
    )
