cmake_minimum_required(VERSION 3.0.0)
project(min VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(min src/main.c src/chunk.c src/memory.c src/debug.c src/value.c)


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
