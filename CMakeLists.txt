cmake_minimum_required(VERSION 3.20)
project(kv_cache VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Threads REQUIRED)

# 引入 abseil（禁用测试）
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/abseil EXCLUDE_FROM_ALL)

add_library(kvstore_lib
    src/storage/kv_store.cpp
    src/storage/persistent_kv_store.cpp
    src/common/protocol.cpp
    src/common/crc32.cpp
    src/common/wal.cpp
    src/common/snapshot.cpp
    src/server/tcp_server.cpp
    src/server/config.cpp
    src/server/kv_server.cpp
    src/client/client.cpp
)
target_include_directories(kvstore_lib PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
)
target_compile_definitions(kvstore_lib PUBLIC SPDLOG_FMT_BUNDLED)
target_link_libraries(kvstore_lib PUBLIC Threads::Threads absl::flat_hash_map)

add_executable(kv_server src/server/server_main.cpp)
target_link_libraries(kv_server PRIVATE kvstore_lib Threads::Threads)
add_custom_command(TARGET kv_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/config.txt
        $<TARGET_FILE_DIR:kv_server>/../config.txt
    COMMENT "Copying config.txt to build directory"
)

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

# 统一测试入口（初始化 Logger）
set(TEST_MAIN tests/test_main.cpp)

# 通用测试目标函数
function(add_kv_test TEST_NAME)
    set(TEST_FILE "tests/${TEST_NAME}.cpp")
    set(TEST_SOURCES ${TEST_MAIN} ${TEST_FILE})
    foreach(ARG ${ARGN})
        list(APPEND TEST_SOURCES ${ARG})
    endforeach()

    add_executable(${TEST_NAME} ${TEST_SOURCES})
    set_target_properties(${TEST_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test)
    target_include_directories(${TEST_NAME} PRIVATE ${GTEST_INCLUDE_DIR} ${GMOCK_INCLUDE_DIR})
    target_link_libraries(${TEST_NAME} PRIVATE kvstore_lib gtest)
    if(WIN32)
        target_link_libraries(${TEST_NAME} PRIVATE wsock32 ws2_32)
    endif()
endfunction()

# 单元测试
add_kv_test(test_kv_store)
add_kv_test(test_protocol)
add_kv_test(test_cli_parser src/client/cli_parser.cpp)
add_kv_test(test_crc32)
add_kv_test(test_wal)
add_kv_test(test_snapshot)
add_kv_test(test_persistent_kv_store)

# Google Benchmark 跨平台配置
set(BENCHMARK_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/benchmark/include)

if(WIN32)
    set(BENCHMARK_PLATFORM_DIR ${CMAKE_SOURCE_DIR}/third_party/lib/win)
    set(BENCHMARK_LIB_NAMES libbenchmark.dll libbenchmark_main.dll)
else()
    set(BENCHMARK_PLATFORM_DIR ${CMAKE_SOURCE_DIR}/third_party/lib/linux)
    set(BENCHMARK_LIB_NAMES libbenchmark.so libbenchmark_main.so)
endif()

set(BENCHMARK_LIB_FOUND TRUE)
foreach(lib ${BENCHMARK_LIB_NAMES})
    if(NOT EXISTS "${BENCHMARK_PLATFORM_DIR}/${lib}")
        set(BENCHMARK_LIB_FOUND FALSE)
        break()
    endif()
endforeach()

if(BENCHMARK_LIB_FOUND)
    message(STATUS "Google Benchmark pre-built libraries found in ${BENCHMARK_PLATFORM_DIR}")

    if(WIN32)
        add_library(benchmark SHARED IMPORTED)
        set_target_properties(benchmark PROPERTIES
            IMPORTED_LOCATION ${BENCHMARK_PLATFORM_DIR}/libbenchmark.dll
            IMPORTED_IMPLIB ${BENCHMARK_PLATFORM_DIR}/libbenchmark.dll.a
        )
        add_library(benchmark_main SHARED IMPORTED)
        set_target_properties(benchmark_main PROPERTIES
            IMPORTED_LOCATION ${BENCHMARK_PLATFORM_DIR}/libbenchmark_main.dll
            IMPORTED_IMPLIB ${BENCHMARK_PLATFORM_DIR}/libbenchmark_main.dll.a
        )
    else()
        add_library(benchmark SHARED IMPORTED)
        set_target_properties(benchmark PROPERTIES
            IMPORTED_LOCATION ${BENCHMARK_PLATFORM_DIR}/libbenchmark.so
        )
        add_library(benchmark_main SHARED IMPORTED)
        set_target_properties(benchmark_main PROPERTIES
            IMPORTED_LOCATION ${BENCHMARK_PLATFORM_DIR}/libbenchmark_main.so
        )
    endif()
else()
    # 编译 Google Benchmark 并输出到对应平台目录
    message(STATUS "Google Benchmark not found, building from source to ${BENCHMARK_PLATFORM_DIR}")

    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/benchmark EXCLUDE_FROM_ALL)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    # 设置输出目录
    set_target_properties(benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BENCHMARK_PLATFORM_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${BENCHMARK_PLATFORM_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${BENCHMARK_PLATFORM_DIR}
    )
    set_target_properties(benchmark_main PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BENCHMARK_PLATFORM_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${BENCHMARK_PLATFORM_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${BENCHMARK_PLATFORM_DIR}
    )
endif()

# Benchmark 可执行文件
add_executable(bench_wal benchmarks/bench_wal.cpp)
set_target_properties(bench_wal PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bench)
target_include_directories(bench_wal PRIVATE ${BENCHMARK_INCLUDE_DIR})
target_link_libraries(bench_wal PRIVATE kvstore_lib benchmark benchmark_main Threads::Threads)

if(WIN32)
    target_link_libraries(bench_wal PRIVATE wsock32 ws2_32)
endif()

# D2D 集成测试
function(add_d2d_client CLIENT_NAME)
    set(CLIENT_SRC "d2dtest/clients/${CLIENT_NAME}.cpp")
    add_executable(${CLIENT_NAME} ${CLIENT_SRC})
    set_target_properties(${CLIENT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/d2dtest)
    target_link_libraries(${CLIENT_NAME} PRIVATE kvstore_lib Threads::Threads)
    if(WIN32)
        target_link_libraries(${CLIENT_NAME} PRIVATE wsock32 ws2_32)
    endif()
endfunction()

add_d2d_client(writer_client)
add_d2d_client(burst_client)
add_d2d_client(steady_client)
add_d2d_client(delete_client)
add_d2d_client(modify_client)
add_d2d_client(reader_client)
