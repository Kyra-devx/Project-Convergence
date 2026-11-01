cmake_minimum_required(VERSION 3.22.1)

project(convergence_android LANGUAGES CXX)

# Build the portable engine as a static library without desktop test targets.
set(BUILD_TESTING OFF CACHE BOOL "Disable engine tests in Android build" FORCE)
add_subdirectory(
    ${CMAKE_CURRENT_LIST_DIR}/../../../../engine
    ${CMAKE_CURRENT_BINARY_DIR}/engine
)

add_library(convergence SHARED native-lib.cpp)
target_compile_features(convergence PRIVATE cxx_std_17)

find_library(android-lib android)
find_library(log-lib log)
find_library(gles-lib GLESv3)

target_link_libraries(
    convergence
    PRIVATE
    convergence_engine
    ${android-lib}
    ${log-lib}
    ${gles-lib}
)
