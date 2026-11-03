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

    add_executable(voxel_world_test tests/voxel_world_test.cpp)
    target_link_libraries(voxel_world_test PRIVATE convergence_engine)
    add_test(NAME voxel_world_test COMMAND voxel_world_test)

    add_executable(chunk_mesher_test tests/chunk_mesher_test.cpp)
    target_link_libraries(chunk_mesher_test PRIVATE convergence_engine)
    add_test(NAME chunk_mesher_test COMMAND chunk_mesher_test)

    # The generator and manager tests exercise their app-side implementations.
    find_package(Threads REQUIRED)
    set(WORLD_GENERATOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp)
    add_executable(
        world_generator_test
        tests/world_generator_test.cpp
        ${WORLD_GENERATOR_DIR}/WorldGenerator.cpp
    )
    target_include_directories(world_generator_test PRIVATE ${WORLD_GENERATOR_DIR})
    target_link_libraries(world_generator_test PRIVATE convergence_engine)
    target_compile_features(world_generator_test PRIVATE cxx_std_17)
    add_test(NAME world_generator_test COMMAND world_generator_test)

    add_executable(
        chunk_manager_test
        tests/chunk_manager_test.cpp
        ${WORLD_GENERATOR_DIR}/WorldGenerator.cpp
        ${WORLD_GENERATOR_DIR}/ChunkManager.cpp
    )
    target_include_directories(chunk_manager_test PRIVATE ${WORLD_GENERATOR_DIR})
    target_link_libraries(chunk_manager_test PRIVATE convergence_engine Threads::Threads)
    target_compile_features(chunk_manager_test PRIVATE cxx_std_17)
    add_test(NAME chunk_manager_test COMMAND chunk_manager_test)

    add_executable(
        raycast_test
        tests/raycast_test.cpp
        ${WORLD_GENERATOR_DIR}/WorldGenerator.cpp
        ${WORLD_GENERATOR_DIR}/ChunkManager.cpp
        ${WORLD_GENERATOR_DIR}/Raycast.cpp
    )
    target_include_directories(raycast_test PRIVATE ${WORLD_GENERATOR_DIR})
    target_link_libraries(
        raycast_test PRIVATE convergence_engine glm::glm Threads::Threads)
    target_compile_features(raycast_test PRIVATE cxx_std_17)
    add_test(NAME raycast_test COMMAND raycast_test)
endif()
