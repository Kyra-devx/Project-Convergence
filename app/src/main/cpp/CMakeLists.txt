cmake_minimum_required(VERSION 3.22.1)

project(convergence_android LANGUAGES CXX)

add_library(convergence SHARED native-lib.cpp)
target_compile_features(convergence PRIVATE cxx_std_17)

find_library(android-lib android)
find_library(log-lib log)
find_library(gles-lib GLESv3)

target_link_libraries(
    convergence
    PRIVATE
    ${android-lib}
    ${log-lib}
    ${gles-lib}
)
