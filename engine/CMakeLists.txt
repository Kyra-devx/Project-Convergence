cmake_minimum_required(VERSION 3.22)

project(convergence_engine LANGUAGES CXX)

add_library(convergence_engine STATIC src/voxel_world.cpp)

target_include_directories(
    convergence_engine
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(convergence_engine PUBLIC cxx_std_17)

include(CTest)
if(BUILD_TESTING)
    add_executable(voxel_world_test tests/voxel_world_test.cpp)
    target_link_libraries(voxel_world_test PRIVATE convergence_engine)
    add_test(NAME voxel_world_test COMMAND voxel_world_test)
endif()
