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

find_library(RTLSDR rtlsdr)
find_path(RTLSDR_INCLUDE_DIR rtl-sdr.h)
include_directories(${RTLSDR_INCLUDE_DIR})
target_link_libraries(sdr_serverLib ${RTLSDR})

find_package(Volk REQUIRED)
include_directories(${VOLK_INCLUDE_DIRS})
target_link_libraries(sdr_serverLib ${VOLK_LIBRARIES})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries(sdr_serverLib ${ZLIB_LIBRARIES})

find_library(LIBCONFIG config)
find_path(LIBCONFIG_INCLUDE_DIR libconfig.h)
include_directories(${LIBCONFIG_INCLUDE_DIR})
target_link_libraries(sdr_serverLib ${LIBCONFIG})
target_link_libraries(sdr_serverLib pthread m)

enable_testing()

file(GLOB TEST_SOURCES test/test_*.c)
file(GLOB TEST_RESOURCES test/resources/*)
file(COPY ${TEST_RESOURCES} DESTINATION "${CMAKE_BINARY_DIR}")
file(GLOB PERF_SOURCES test/perf_*.c)

file(GLOB AUX_TEST_SOURCES test/*.c)
list(FILTER AUX_TEST_SOURCES EXCLUDE REGEX "test_.*\\.c")
list(FILTER AUX_TEST_SOURCES EXCLUDE REGEX "perf_.*\\.c")
list(FILTER AUX_TEST_SOURCES EXCLUDE REGEX "mock_.*\\.c")

find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})

find_package(Threads REQUIRED)

foreach(curTest ${TEST_SOURCES})
	get_filename_component(curTestName ${curTest} NAME_WE)
	add_test(NAME ${curTestName} COMMAND ${curTestName} ${curTest})
	add_executable(${curTestName} ${curTest} ${AUX_TEST_SOURCES})
	set_property(TARGET ${curTestName} PROPERTY C_STANDARD 99)
	target_link_libraries(${curTestName} sdr_serverLib ${CMAKE_THREAD_LIBS_INIT} ${PC_CHECK_LDFLAGS})
endforeach()

foreach(curPerfTest ${PERF_SOURCES})
	get_filename_component(curPerfTestName ${curPerfTest} NAME_WE)
	add_test(NAME ${curPerfTestName} COMMAND ${curPerfTestName} ${curPerfTest})
	add_executable(${curPerfTestName} ${curPerfTest})
	set_property(TARGET ${curPerfTestName} PROPERTY C_STANDARD 99)
	target_link_libraries(${curPerfTestName} sdr_serverLib ${CMAKE_THREAD_LIBS_INIT} ${PC_CHECK_LDFLAGS})
endforeach()

if(CMAKE_BUILD_TYPE MATCHES Debug)
	add_custom_target("coverage")
	add_custom_command(TARGET "coverage" COMMAND gcov ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_serverLib.dir/src/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/test_core.dir/test/*.c.o)
endif()