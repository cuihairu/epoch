cmake_minimum_required(VERSION 3.20)
project(epoch_cpp VERSION 0.1.0 LANGUAGES CXX)

add_library(epoch_cpp src/epoch.cpp src/engine.cpp)

target_include_directories(epoch_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(epoch_cpp PUBLIC cxx_std_17)

enable_testing()
add_executable(epoch_cpp_test tests/epoch_vector_test.cpp)
target_link_libraries(epoch_cpp_test PRIVATE epoch_cpp)
add_test(NAME epoch_cpp_vector COMMAND epoch_cpp_test)
