cmake_minimum_required(VERSION 2.8)
project(sdr-server)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
endif()

add_library(sdr_serverLib
		${CMAKE_CURRENT_SOURCE_DIR}/src/config.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/core.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/lpf.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/queue.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/rotator.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_server.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/xlating.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/client/tcp_client.c
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
link_directories(${PC_RTLSDR_LIBRARY_DIRS})
target_link_libraries(sdr_serverLib ${PC_RTLSDR_LIBRARIES})

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
target_link_libraries(sdr_serverLib ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(sdr_serverLib m)

add_executable(sdr_server src/main.c)
target_link_libraries(sdr_server sdr_serverLib)

add_executable(sdr_server_client
		${CMAKE_CURRENT_SOURCE_DIR}/src/client/tcp_client.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/client/tcp_client_main.c
)

install(TARGETS sdr_server DESTINATION /usr/bin/)
install(TARGETS sdr_server_client DESTINATION /usr/bin/)
install(FILES src/resources/config.conf DESTINATION /etc/sdr-server/)

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
link_directories(${PC_CHECK_LIBRARY_DIRS})

foreach(curTest ${TEST_SOURCES})
	get_filename_component(curTestName ${curTest} NAME_WE)
	add_test(NAME ${curTestName} COMMAND ${curTestName} ${curTest})
	add_executable(${curTestName} ${curTest} ${AUX_TEST_SOURCES})
	target_link_libraries(${curTestName} sdr_serverLib ${PC_CHECK_LIBRARIES})
endforeach()

foreach(curPerfTest ${PERF_SOURCES})
	get_filename_component(curPerfTestName ${curPerfTest} NAME_WE)
	add_executable(${curPerfTestName} ${curPerfTest})
	target_link_libraries(${curPerfTestName} sdr_serverLib ${PC_CHECK_LIBRARIES})
endforeach()

if(CMAKE_BUILD_TYPE MATCHES Debug)
	add_custom_target("coverage")
	add_custom_command(TARGET "coverage" COMMAND gcov ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_serverLib.dir/src/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/test_core.dir/test/*.c.o)
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_NAME "sdr-server")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libvolk2.4, librtlsdr2 (>= 0.5.4), libconfig9, zlib1g")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andrey Rodionov <dernasherbrezon@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "High performant TCP server for rtl-sdr")
set(CPACK_DEBIAN_PACKAGE_SECTION "comm")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/dernasherbrezon/sdr-server")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm")
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
set(CPACK_DEBIAN_FILE_NAME "sdr-server_${CPACK_DEBIAN_PACKAGE_VERSION}_${CUSTOM_ARCHITECTURE}.deb")
include(CPack)