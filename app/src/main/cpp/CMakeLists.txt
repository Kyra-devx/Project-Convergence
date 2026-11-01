cmake_minimum_required(VERSION 3.22.1)

project(convergence_android LANGUAGES CXX)

# GLM is header-only. Pin the exact upstream revision for reproducible builds.
include(FetchContent)
set(GLM_BUILD_TESTS OFF CACHE BOOL "Disable GLM tests" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "Use GLM header-only target" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "Disable GLM install rules" FORCE)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0af55ccecd98d4e5a8d1fad7de25ba429d60e863
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(glm)

# Build the portable engine as a static library without desktop test targets.
set(BUILD_TESTING OFF CACHE BOOL "Disable engine tests in Android build" FORCE)
add_subdirectory(
    ${CMAKE_CURRENT_LIST_DIR}/../../../../engine
    ${CMAKE_CURRENT_BINARY_DIR}/engine
)

add_library(
    convergence
    SHARED
    native-lib.cpp
    Camera.cpp
    WorldGenerator.cpp
)
target_compile_features(convergence PRIVATE cxx_std_17)
target_include_directories(convergence PRIVATE ${CMAKE_CURRENT_LIST_DIR})

find_library(android-lib android)
find_library(log-lib log)
find_library(gles-lib GLESv3)

target_link_libraries(
    convergence
    PRIVATE
    convergence_engine
    glm::glm
    ${android-lib}
    ${log-lib}
    ${gles-lib}
)
