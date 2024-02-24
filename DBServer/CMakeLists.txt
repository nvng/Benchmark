CMAKE_MINIMUM_REQUIRED(VERSION 3.25)

PROJECT(DBServer.out)

SET(SRC_DIR
        ../Deps/libnvngLib/include/ActorFramework/Impl/DB
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/Guid
        ../Deps/libnvngLib/include/ActorFramework/Impl/Service/DB
        )

INCLUDE_DIRECTORIES(../Deps/libnvngLib/include/DB)

INCLUDE($ENV{HOME}/git/config/cmake/project.cmake)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})
TARGET_PRECOMPILE_HEADERS(${PROJECT_NAME} PRIVATE ./src/PreCompile.h)

#vim: fenc=utf8:expandtab:ts=8:sw=8:fdm=marker:noma:
