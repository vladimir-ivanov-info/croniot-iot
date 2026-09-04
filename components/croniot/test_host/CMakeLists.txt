cmake_minimum_required(VERSION 3.14)
project(croniot_sdk_host_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Host-native tests: plain g++/clang++, zero ESP-IDF/NimBLE/FreeRTOS
# toolchain. Compiles pure-logic files (BleChannelLogic) plus MessageBus
# itself (which only depends on esp_log.h and TaskBase.h's *type*, both
# satisfied here by minimal stubs/ headers - TaskBase is never constructed).

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(CRONIOT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)

add_executable(host_tests
    ${CRONIOT_SRC}/comm/BleChannelLogic.cpp
    ${CRONIOT_SRC}/comm/MessageBus.cpp

    BleChannelLogicTest.cpp
    MessageBusRoutingTest.cpp
)

target_include_directories(host_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CRONIOT_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(host_tests PRIVATE gtest_main gmock)

include(GoogleTest)
gtest_discover_tests(host_tests)
