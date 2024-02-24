CMAKE_MINIMUM_REQUIRED(VERSION 3.25)

PROJECT(LobbyServer.out)

SET(SRC_DIR
        ./src/Player
        ./src/Region
        ../Deps/libnvngLib/include/ActorFramework/Impl/Lobby
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/Log
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/Guid
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/DB
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/Benchmark
        )

INCLUDE($ENV{HOME}/git/config/cmake/project.cmake)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})
TARGET_PRECOMPILE_HEADERS(${PROJECT_NAME} PRIVATE ./src/PreCompile.h)
# target_link_options(${PROJECT_NAME} PUBLIC -stdlib=libc++)

#vim: fenc=utf8:expandtab:ts=8:sw=8:fdm=marker:noma
