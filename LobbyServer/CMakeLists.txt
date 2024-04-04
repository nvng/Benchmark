CMAKE_MINIMUM_REQUIRED(VERSION 3.25)

PROJECT(LobbyServer.out CXX)

SET(SRC_DIR
        ActorFramework/Impl/Lobby
        ActorFramework/Impl/Service/Log
        ActorFramework/Impl/Service/Guid
        ActorFramework/Impl/Service/DB
        ActorFramework/Impl/Service/Benchmark
        )

INCLUDE($ENV{HOME}/git/config/cmake/project.cmake)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})
TARGET_PRECOMPILE_HEADERS(${PROJECT_NAME} PRIVATE ./src/PreCompile.h)

#vim: fenc=utf8:expandtab:ts=8:sw=8:fdm=marker:noma
