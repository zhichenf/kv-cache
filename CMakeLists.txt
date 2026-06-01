cmake_minimum_required(VERSION 3.20)
project(kv_cache VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Threads REQUIRED)

add_library(kvstore_lib
    src/storage/kv_store.cpp
    src/common/protocol.cpp
    src/server/tcp_server.cpp
    src/client/client.cpp
)
target_include_directories(kvstore_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(kv_server src/server/server_main.cpp)
target_link_libraries(kv_server PRIVATE kvstore_lib Threads::Threads)

add_executable(kv_client src/client/client_main.cpp src/client/cli_parser.cpp)
target_link_libraries(kv_client PRIVATE kvstore_lib Threads::Threads)

# Windows socket library
if(WIN32)
    target_link_libraries(kvstore_lib PUBLIC wsock32 ws2_32)
    target_link_libraries(kv_server PRIVATE wsock32 ws2_32)
    target_link_libraries(kv_client PRIVATE wsock32 ws2_32)
endif()

# Google Test 跨平台构建
set(GTEST_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/googletest/googletest/include)
set(GMOCK_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/googletest/googlemock/include)

# 检测平台
if(WIN32)
    set(GTEST_PLATFORM_DIR ${CMAKE_SOURCE_DIR}/third_party/lib/win)
    set(GTEST_LIB_NAMES libgtest.dll libgtest_main.dll)
else()
    set(GTEST_PLATFORM_DIR ${CMAKE_SOURCE_DIR}/third_party/lib/linux)
    set(GTEST_LIB_NAMES libgtest.so libgtest_main.so)
endif()

# 检查当前平台的库文件是否存在
set(GTEST_LIB_FOUND TRUE)
foreach(lib ${GTEST_LIB_NAMES})
    if(NOT EXISTS "${GTEST_PLATFORM_DIR}/${lib}")
        set(GTEST_LIB_FOUND FALSE)
        break()
    endif()
endforeach()

if(GTEST_LIB_FOUND)
    # 直接链接已有库（不再编译源码）
    message(STATUS "Google Test pre-built libraries found in ${GTEST_PLATFORM_DIR}")

    if(WIN32)
        add_library(gtest SHARED IMPORTED)
        set_target_properties(gtest PROPERTIES
            IMPORTED_LOCATION ${GTEST_PLATFORM_DIR}/libgtest.dll
            IMPORTED_IMPLIB ${GTEST_PLATFORM_DIR}/libgtest.dll.a
        )
        add_library(gtest_main SHARED IMPORTED)
        set_target_properties(gtest_main PROPERTIES
            IMPORTED_LOCATION ${GTEST_PLATFORM_DIR}/libgtest_main.dll
            IMPORTED_IMPLIB ${GTEST_PLATFORM_DIR}/libgtest_main.dll.a
        )
    else()
        add_library(gtest SHARED IMPORTED)
        set_target_properties(gtest PROPERTIES
            IMPORTED_LOCATION ${GTEST_PLATFORM_DIR}/libgtest.so
        )
        add_library(gtest_main SHARED IMPORTED)
        set_target_properties(gtest_main PROPERTIES
            IMPORTED_LOCATION ${GTEST_PLATFORM_DIR}/libgtest_main.so
        )
    endif()
else()
    # 编译 Google Test 并输出到对应平台目录
    message(STATUS "Google Test not found, building from source to ${GTEST_PLATFORM_DIR}")

    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    # 设置输出目录
    set_target_properties(gtest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${GTEST_PLATFORM_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${GTEST_PLATFORM_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${GTEST_PLATFORM_DIR}
    )
    set_target_properties(gtest_main PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${GTEST_PLATFORM_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${GTEST_PLATFORM_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${GTEST_PLATFORM_DIR}
    )
endif()

enable_testing()

add_executable(kv_tests
    tests/test_kv_store.cpp
    tests/test_protocol.cpp
    tests/test_cli_parser.cpp
    src/client/cli_parser.cpp
)
set_target_properties(kv_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
)
target_include_directories(kv_tests PRIVATE ${GTEST_INCLUDE_DIR} ${GMOCK_INCLUDE_DIR})
target_link_libraries(kv_tests PRIVATE kvstore_lib gtest gtest_main)

if(WIN32)
    target_link_libraries(kv_tests PRIVATE wsock32 ws2_32)
endif()
