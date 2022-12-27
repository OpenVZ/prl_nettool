cmake_minimum_required(VERSION 3.0)

project (rcconf_test C)

add_definitions(-DTEST_COVERAGE=1)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

file(GLOB SOURCES "../rcconf.c" "../rcconf_list.c" "../rcconf_sublist.c" "test.c")

add_executable(${PROJECT_NAME} ${SOURCES})
