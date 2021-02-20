cmake_minimum_required(VERSION 2.8)
project(sdr-server)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_C_FLAGS "-std=c99")
if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
endif()
file(GLOB SOURCES src/*.c)
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")

add_executable(sdr_server src/main.c)

install(TARGETS sdr_server DESTINATION /usr/bin/)

add_library(sdr_serverLib ${SOURCES})
target_link_libraries(sdr_server sdr_serverLib)

find_package(PkgConfig REQUIRED)

pkg_check_modules(PC_RTLSDR REQUIRED librtlsdr>=0.5.4)
include_directories(${PC_RTLSDR_INCLUDE_DIRS})
target_link_libraries(sdr_serverLib ${PC_RTLSDR_LINK_LIBRARIES})

pkg_check_modules(PC_VOLK REQUIRED volk)
include_directories(${PC_VOLK_INCLUDE_DIRS})
target_link_libraries(sdr_serverLib ${PC_VOLK_LINK_LIBRARIES})

pkg_check_modules(PC_ZLIB REQUIRED zlib)
include_directories(${PC_ZLIB_INCLUDE_DIRS})
target_link_libraries(sdr_serverLib ${PC_ZLIB_LINK_LIBRARIES})

pkg_check_modules(PC_LIBCONFIG REQUIRED libconfig)
include_directories(${PC_LIBCONFIG_INCLUDE_DIRS})
target_link_libraries(sdr_serverLib ${PC_LIBCONFIG_LINK_LIBRARIES})

find_package(Threads REQUIRED)
target_link_libraries(sdr_serverLib m)

enable_testing()

file(GLOB TEST_SOURCES test/test_*.c)
file(GLOB TEST_RESOURCES test/resources/*)
file(COPY ${TEST_RESOURCES} DESTINATION "${CMAKE_BINARY_DIR}")
file(GLOB PERF_SOURCES test/perf_*.c)

file(GLOB AUX_TEST_SOURCES test/*.c)
list(FILTER AUX_TEST_SOURCES EXCLUDE REGEX "test_.*\\.c")
list(FILTER AUX_TEST_SOURCES EXCLUDE REGEX "perf_.*\\.c")
list(FILTER AUX_TEST_SOURCES EXCLUDE REGEX "mock_.*\\.c")

pkg_check_modules(PC_CHECK REQUIRED check)
include_directories(${PC_CHECK_INCLUDE_DIRS})

foreach(curTest ${TEST_SOURCES})
	get_filename_component(curTestName ${curTest} NAME_WE)
	add_test(NAME ${curTestName} COMMAND ${curTestName} ${curTest})
	add_executable(${curTestName} ${curTest} ${AUX_TEST_SOURCES})
	target_link_libraries(${curTestName} sdr_serverLib ${PC_CHECK_LINK_LIBRARIES})
endforeach()

foreach(curPerfTest ${PERF_SOURCES})
	get_filename_component(curPerfTestName ${curPerfTest} NAME_WE)
	add_test(NAME ${curPerfTestName} COMMAND ${curPerfTestName} ${curPerfTest})
	add_executable(${curPerfTestName} ${curPerfTest})
	target_link_libraries(${curPerfTestName} sdr_serverLib ${PC_CHECK_LINK_LIBRARIES})
endforeach()

if(CMAKE_BUILD_TYPE MATCHES Debug)
	add_custom_target("coverage")
	add_custom_command(TARGET "coverage" COMMAND gcov ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_serverLib.dir/src/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/test_core.dir/test/*.c.o)
endif()