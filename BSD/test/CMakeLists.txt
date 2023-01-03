cmake_minimum_required(VERSION 3.0)

project (rcconf_test C)

add_definitions(-DTEST_COVERAGE=1)
add_definitions(-DCMAKE_BUILD_TYPE=Debug)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
set(COVERAGE_DIR "coverage")
set(TMP_DIR "tmp")

file(GLOB SOURCES "../rcconf.c" "../rcconf_list.c" "../rcconf_sublist.c" "../resolvconf.c" "test.c")

add_executable(${PROJECT_NAME} ${SOURCES})

add_custom_target(test
	DEPENDS ${PROJECT_NAME}
	COMMAND rm -rf ${COVERAGE_DIR}
	COMMAND mkdir ${COVERAGE_DIR}
	COMMAND rm -rf ${TMP_DIR}
	COMMAND mkdir ${TMP_DIR}
	COMMAND ./rcconf_test
	COMMAND lcov --capture --directory . --output-file ${COVERAGE_DIR}/coverage.info
	COMMAND genhtml ${COVERAGE_DIR}/coverage.info --output-directory ${COVERAGE_DIR}/html
)
