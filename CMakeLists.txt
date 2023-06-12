cmake_minimum_required(VERSION 2.8)
project(sdr-server)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage ")
endif()

add_library(sdr_serverLib
		${CMAKE_CURRENT_SOURCE_DIR}/src/config.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/core.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/lpf.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/queue.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/rotator.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_server.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/xlating.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/airspy_device.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/rtlsdr_device.c
)

# at some point homebrew changed default location
# so all libraries need an explicit path
if (APPLE)
	include_directories("/opt/homebrew/include")
	link_directories("/opt/homebrew/lib")
endif ()

find_package(PkgConfig REQUIRED)

pkg_check_modules(PC_RTLSDR REQUIRED librtlsdr>=0.5.4)
include_directories(${PC_RTLSDR_INCLUDE_DIRS})

pkg_check_modules(PC_LIBAIRSPY REQUIRED libairspy)
include_directories(${PC_LIBAIRSPY_INCLUDE_DIRS})

pkg_check_modules(PC_VOLK REQUIRED volk)
include_directories(${PC_VOLK_INCLUDE_DIRS})
link_directories(${PC_VOLK_LIBRARY_DIRS})
target_link_libraries(sdr_serverLib ${PC_VOLK_LIBRARIES})

pkg_check_modules(PC_ZLIB REQUIRED zlib)
include_directories(${PC_ZLIB_INCLUDE_DIRS})
link_directories(${PC_ZLIB_LIBRARY_DIRS})
target_link_libraries(sdr_serverLib ${PC_ZLIB_LIBRARIES})

pkg_check_modules(PC_LIBCONFIG REQUIRED libconfig)
include_directories(${PC_LIBCONFIG_INCLUDE_DIRS})
link_directories(${PC_LIBCONFIG_LIBRARY_DIRS})
target_link_libraries(sdr_serverLib ${PC_LIBCONFIG_LIBRARIES})

find_package(Threads REQUIRED)
target_link_libraries(sdr_serverLib m ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

# link real libraries into final executable so that tests can use mocks
add_executable(sdr_server
		${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/rtlsdr_lib.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/airspy_lib.c
)
target_link_libraries(sdr_server sdr_serverLib)

add_executable(sdr_server_client
		${CMAKE_CURRENT_SOURCE_DIR}/src/client/tcp_client.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/client/tcp_client_main.c
)

install(TARGETS sdr_server sdr_server_client DESTINATION /usr/bin/)
install(FILES src/resources/config.conf DESTINATION /etc/sdr-server/)

enable_testing()

file(GLOB TEST_RESOURCES test/resources/*)
file(COPY ${TEST_RESOURCES} DESTINATION "${CMAKE_BINARY_DIR}")

pkg_check_modules(PC_CHECK REQUIRED check)
include_directories(${PC_CHECK_INCLUDE_DIRS})
link_directories(${PC_CHECK_LIBRARY_DIRS})

# speed up test compilation.
add_library(sdr_serverTestLib
		${CMAKE_CURRENT_SOURCE_DIR}/test/rtlsdr_lib_mock.c
		${CMAKE_CURRENT_SOURCE_DIR}/test/airspy_lib_mock.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/client/tcp_client.c
		${CMAKE_CURRENT_SOURCE_DIR}/test/utils.c
)
file(GLOB TEST_SOURCES test/test_*.c)
foreach(curTest ${TEST_SOURCES})
	get_filename_component(curTestName ${curTest} NAME_WE)
	add_executable(${curTestName} ${CMAKE_CURRENT_SOURCE_DIR}/test/${curTestName}.c)
	target_link_libraries(${curTestName} sdr_serverLib sdr_serverTestLib ${PC_CHECK_LIBRARIES})
	add_test(NAME ${curTestName} COMMAND ${curTestName})
endforeach()

file(GLOB PERF_SOURCES test/perf_*.c)
foreach(curPerfTest ${PERF_SOURCES})
	get_filename_component(curPerfTestName ${curPerfTest} NAME_WE)
	add_executable(${curPerfTestName} ${curPerfTest})
	target_link_libraries(${curPerfTestName} sdr_serverLib ${PC_CHECK_LIBRARIES})
endforeach()

if(CMAKE_BUILD_TYPE MATCHES Debug)
	add_custom_target("coverage")
	add_custom_command(TARGET "coverage" COMMAND gcov ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_serverLib.dir/src/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/test_core.dir/test/*.c.o)
endif()