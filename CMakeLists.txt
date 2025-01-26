cmake_minimum_required(VERSION 2.8)
project(sdr-server)
set(CMAKE_C_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ffast-math")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} --coverage")

if(NO_MANUAL_SIMD)
	add_definitions(-DNO_MANUAL_SIMD)
else()
	remove_definitions(-DNO_MANUAL_SIMD)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
	set(BUILD_COMPILATION_FLAGS "${CMAKE_C_FLAGS_DEBUG}")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
	set(BUILD_COMPILATION_FLAGS "${CMAKE_C_FLAGS_RELEASE}")
endif()
add_definitions(-DCMAKE_C_FLAGS="${CMAKE_C_FLAGS} ${BUILD_COMPILATION_FLAGS}")

add_library(sdr_serverLib
		${CMAKE_CURRENT_SOURCE_DIR}/src/config.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr_device.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/dsp_worker.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/lpf.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/queue.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/tcp_server.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/xlating.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/client/tcp_client.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/airspy_device.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/rtlsdr_device.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/hackrf_device.c
)

# at some point homebrew changed default location
# so all libraries need an explicit path
if (APPLE)
	include_directories("/opt/homebrew/include")
	include_directories("/usr/local/include")
	link_directories("/opt/homebrew/lib")
	link_directories("/usr/local/lib")
endif ()


find_package(PkgConfig REQUIRED)

pkg_check_modules(PC_RTLSDR REQUIRED librtlsdr>=0.5.4)
include_directories(${PC_RTLSDR_INCLUDE_DIRS})

pkg_check_modules(PC_LIBAIRSPY REQUIRED libairspy)
include_directories(${PC_LIBAIRSPY_INCLUDE_DIRS})

pkg_check_modules(PC_LIBHACKRF REQUIRED libhackrf)
include_directories(${PC_LIBHACKRF_INCLUDE_DIRS})

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
		${CMAKE_CURRENT_SOURCE_DIR}/src/sdr/hackrf_lib.c
)
target_link_libraries(sdr_server sdr_serverLib)

add_executable(sdr_server_client
		${CMAKE_CURRENT_SOURCE_DIR}/src/client/tcp_client.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/client/tcp_client_main.c
)

install(TARGETS sdr_server DESTINATION /usr/bin/)
install(TARGETS sdr_server_client DESTINATION /usr/bin/)
install(FILES src/resources/config.conf DESTINATION /etc/sdr-server/)
install(FILES debian/sdr-server.service DESTINATION /lib/systemd/system/)

enable_testing()

file(GLOB TEST_RESOURCES test/resources/*)
file(COPY ${TEST_RESOURCES} DESTINATION "${CMAKE_BINARY_DIR}")
file(GLOB PERF_SOURCES test/perf_*.c)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/test/unity-2.5.2/src/)

# speed up test compilation.
add_library(sdr_serverTestLib
		${CMAKE_CURRENT_SOURCE_DIR}/test/rtlsdr_lib_mock.c
		${CMAKE_CURRENT_SOURCE_DIR}/test/airspy_lib_mock.c
		${CMAKE_CURRENT_SOURCE_DIR}/test/hackrf_lib_mock.c
		${CMAKE_CURRENT_SOURCE_DIR}/src/client/tcp_client.c
		${CMAKE_CURRENT_SOURCE_DIR}/test/utils.c
		${CMAKE_CURRENT_SOURCE_DIR}/test/unity-2.5.2/src/unity.c
)
file(GLOB TEST_SOURCES test/test_*.c)
foreach(curTest ${TEST_SOURCES})
	get_filename_component(curTestName ${curTest} NAME_WE)
	add_test(NAME ${curTestName} COMMAND ${curTestName} ${curTest})
	add_executable(${curTestName} ${curTest})
	target_link_libraries(${curTestName} sdr_serverLib sdr_serverTestLib)
endforeach()

add_executable(perf_xlating
		${CMAKE_CURRENT_SOURCE_DIR}/test/perf_xlating.c
)
target_link_libraries(perf_xlating sdr_serverLib)

if(CMAKE_BUILD_TYPE MATCHES Debug)
	add_custom_target("coverage")
	add_custom_command(TARGET "coverage" COMMAND gcov ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_serverLib.dir/src/*.c.o ${CMAKE_BINARY_DIR}/CMakeFiles/sdr_serverLib.dir/src/sdr/*.c.o)
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_NAME "sdr-server")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "librtlsdr2 (>= 2.0.1), libconfig9, zlib1g")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andrey Rodionov <dernasherbrezon@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "High performant TCP server for rtl-sdr")
set(CPACK_DEBIAN_PACKAGE_SECTION "comm")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/dernasherbrezon/sdr-server")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm;${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm")
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
set(CPACK_DEBIAN_FILE_NAME "sdr-server_${CPACK_DEBIAN_PACKAGE_VERSION}_${CUSTOM_ARCHITECTURE}.deb")
include(CPack)
