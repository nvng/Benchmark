cmake_minimum_required(VERSION 3.25)

set(SRC_DIR
        ../Deps/libnvngLib/include/ActorFramework/Impl/DB
        )

include_directories(../Deps/libnvngLib/include/DB)

include($ENV{HOME}/git/config/cmake/project.cmake)

project(DBServer.out)

# 第二个 Proto 参数用于指定外部文件夹在输出文件夹中的位置
add_subdirectory(../Proto Proto)

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} proto)
target_precompile_headers(${PROJECT_NAME} PRIVATE ./src/PreCompile.h)

#vim: fenc=utf8:expandtab:ts=8:sw=8:fdm=marker:noma:
