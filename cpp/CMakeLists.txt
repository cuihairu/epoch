cmake_minimum_required(VERSION 3.20)
project(epoch_cpp VERSION 0.1.0 LANGUAGES C CXX)

add_library(epoch_cpp src/epoch.cpp src/engine.cpp src/actor_id.cpp src/aeron_transport.cpp)

target_include_directories(epoch_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(epoch_cpp PRIVATE EPOCH_TESTING)

target_compile_features(epoch_cpp PUBLIC cxx_std_17)

find_package(Threads REQUIRED)
set(AERON_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/aeron)
set(AERON_CLIENT_DIR ${AERON_ROOT}/aeron-client/src/main/c)
if (EXISTS ${AERON_CLIENT_DIR}/CMakeLists.txt)
    include(CheckIncludeFile)
    file(STRINGS ${AERON_ROOT}/version.txt AERON_VERSION_TXT LIMIT_COUNT 1)
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${AERON_VERSION_TXT})
    set(AERON_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(AERON_VERSION_MINOR ${CMAKE_MATCH_2})
    set(AERON_VERSION_PATCH ${CMAKE_MATCH_3})
    execute_process(
        COMMAND git -C ${AERON_ROOT} rev-parse --short HEAD
        OUTPUT_VARIABLE AERON_VERSION_GITSHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if (AERON_VERSION_GITSHA STREQUAL "")
        set(AERON_VERSION_GITSHA "unknown")
    endif()
    add_definitions(-DAERON_VERSION_TXT="${AERON_VERSION_TXT}")
    add_definitions(-DAERON_VERSION_MAJOR=${AERON_VERSION_MAJOR})
    add_definitions(-DAERON_VERSION_MINOR=${AERON_VERSION_MINOR})
    add_definitions(-DAERON_VERSION_PATCH=${AERON_VERSION_PATCH})
    add_definitions(-DAERON_VERSION_GITSHA="${AERON_VERSION_GITSHA}")
    set(AERON_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${AERON_CLIENT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/aeron-client)
    target_link_libraries(epoch_cpp PUBLIC aeron::aeron_static)
else()
    message(FATAL_ERROR "Aeron submodule not found. Run: git submodule update --init --recursive")
endif()

enable_testing()
add_executable(epoch_cpp_test tests/epoch_vector_test.cpp)
target_link_libraries(epoch_cpp_test PRIVATE epoch_cpp)
add_test(NAME epoch_cpp_vector COMMAND epoch_cpp_test)
target_compile_definitions(epoch_cpp_test PRIVATE EPOCH_TESTING)

add_executable(epoch_cpp_core_test tests/core_test.cpp)
target_link_libraries(epoch_cpp_core_test PRIVATE epoch_cpp)
add_test(NAME epoch_cpp_core COMMAND epoch_cpp_core_test)
target_compile_definitions(epoch_cpp_core_test PRIVATE EPOCH_TESTING)

add_executable(epoch_cpp_aeron_test tests/aeron_transport_test.cpp)
target_link_libraries(epoch_cpp_aeron_test PRIVATE epoch_cpp)
add_test(NAME epoch_cpp_aeron COMMAND epoch_cpp_aeron_test)
target_compile_definitions(epoch_cpp_aeron_test PRIVATE EPOCH_TESTING)

option(EPOCH_COVERAGE "Enable coverage instrumentation" OFF)
if (EPOCH_COVERAGE)
    foreach(target epoch_cpp epoch_cpp_test epoch_cpp_core_test epoch_cpp_aeron_test)
        target_compile_options(${target} PRIVATE -O0 -g --coverage)
        target_link_options(${target} PRIVATE --coverage)
    endforeach()
endif()
