CMAKE_MINIMUM_REQUIRED(VERSION 3.25)

PROJECT(GameServer.out CXX)

SET(SRC_DIR
        ActorFramework/Impl/Game
        )

INCLUDE($ENV{HOME}/git/config/cmake/project.cmake)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})
TARGET_PRECOMPILE_HEADERS(${PROJECT_NAME} PRIVATE ./src/PreCompile.h)

#vim: fenc=utf8:expandtab:ts=8:sw=8:fdm=marker:noma:
