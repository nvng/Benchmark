cmake_minimum_required(VERSION 3.25)

project(LobbyServer.out)

set(SRC_DIR
        ./src/Player
        ./src/Region
        ../Deps/libnvngLib/include/ActorFramework/Impl/Lobby
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/Benchmark
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/Log
        )

include($ENV{HOME}/git/config/cmake/project.cmake)

# 第二个 Proto 参数用于指定外部文件夹在输出文件夹中的位置
add_subdirectory(../Proto Proto)

add_executable(${PROJECT_NAME} ${SRC_LIST} ${libproto.a})
target_link_libraries(${PROJECT_NAME} proto)
target_precompile_headers(${PROJECT_NAME} PRIVATE ./src/PreCompile.h)
# target_link_options(${PROJECT_NAME} PUBLIC -stdlib=libc++)

#vim: fenc=utf8:expandtab:ts=8:sw=8:fdm=marker:noma
