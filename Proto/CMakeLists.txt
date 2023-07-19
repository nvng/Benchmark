cmake_minimum_required(VERSION 3.22)

include($ENV{HOME}/git/config/cmake/common.cmake)

find_package(Protobuf REQUIRED)
FILE(GLOB protofiles "./*.proto")
# message(STATUS ${protofiles})
PROTOBUF_GENERATE_CPP(PROTOSRCS PROTOHDRS ${protofiles})
# message(STATUS ${PROTOSRCS})
add_library(proto STATIC ${PROTOSRCS} ${PROTOHDRS}) 
target_link_libraries(proto protobuf) 
target_include_directories(proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# vim: fenc=utf8:expandtab:ts=8:sw=8:fdm=marker:noma:
