CMAKE_MINIMUM_REQUIRED(VERSION 3.25)

PROJECT(GameMgrServer.out CXX)

SET(SRC_DIR
        ActorFramework/Impl/GameMgr
        ActorFramework/Impl/Service/Benchmark
        ActorFramework/Impl/Service/Robot
        ActorFramework/Impl/Service/Log
        )

INCLUDE($ENV{HOME}/git/config/cmake/project.cmake)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})
TARGET_PRECOMPILE_HEADERS(${PROJECT_NAME} PRIVATE ./src/PreCompile.h)

#vim: fenc=utf8:expandtab:ts=8:sw=8:fdm=marker:noma:
