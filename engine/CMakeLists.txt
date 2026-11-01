cmake_minimum_required(VERSION 3.22)

project(convergence_engine LANGUAGES CXX)

add_library(
    convergence_engine
    STATIC
    src/voxel_world.cpp
    src/chunk.cpp
)
set_target_properties(convergence_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

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

    add_executable(chunk_mesher_test tests/chunk_mesher_test.cpp)
    target_link_libraries(chunk_mesher_test PRIVATE convergence_engine)
    add_test(NAME chunk_mesher_test COMMAND chunk_mesher_test)
endif()
