CMAKE_MINIMUM_REQUIRED(VERSION 3.25)

INCLUDE($ENV{HOME}/git/config/cmake/common.cmake)

FILE(GLOB protofiles "./*.proto")
PROTOBUF_GENERATE_CPP(PROTOSRCS PROTOHDRS ${protofiles})
ADD_LIBRARY(proto STATIC ${PROTOSRCS} ${PROTOHDRS})
TARGET_INCLUDE_DIRECTORIES(proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# vim: fenc=utf8:expandtab:ts=8:sw=8:fdm=marker:noma:
