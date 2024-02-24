CMAKE_MINIMUM_REQUIRED(VERSION 3.25)

PROJECT(GameMgrServer.out)

SET(SRC_DIR
        ./src/Region
        ../Deps/libnvngLib/include/ActorFramework/Impl/GameMgr
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/Benchmark
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/Robot
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/Log
        )

INCLUDE($ENV{HOME}/git/config/cmake/project.cmake)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})
TARGET_PRECOMPILE_HEADERS(${PROJECT_NAME} PRIVATE ./src/PreCompile.h)

#vim: fenc=utf8:expandtab:ts=8:sw=8:fdm=marker:noma:
