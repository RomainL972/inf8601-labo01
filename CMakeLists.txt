cmake_minimum_required(VERSION 3.24)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

project(tp1 LANGUAGES CXX)

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Ce projet doit être compilé avec Clang (compilateur détecté : "
        "${CMAKE_CXX_COMPILER_ID}). Essayez -DCMAKE_CXX_COMPILER=clang++.")
endif()

set(CMAKE_CXX_STANDARD 20)

include_directories(include)

enable_testing()

set(PIPELINE_SERIAL_SOURCE "${PROJECT_SOURCE_DIR}/serials/pipeline-serial.cpp")

set(PIPELINE_DATA_DIR "${PROJECT_SOURCE_DIR}/data" CACHE PATH
    "Dossier data/ (contenant downloaded/ et synthetic/) utilisé par les cibles run-* et par les tests ; remplacez avec -DPIPELINE_DATA_DIR=... si vous gardez vos images hors du projet, par exemple /home/tmp sur les postes de laboratoire")

include(FetchContent)

find_package(Threads REQUIRED)

FetchContent_Declare(
    TBB
    GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
    GIT_TAG v2021.13.0
    GIT_SHALLOW TRUE
)
set(TBB_TEST OFF CACHE BOOL "" FORCE)
set(TBB_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TBB_STRICT OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(TBB)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)
if (NOT TARGET stb::stb)
    add_library(stb::stb INTERFACE IMPORTED)
    target_include_directories(stb::stb SYSTEM INTERFACE "${stb_SOURCE_DIR}")
endif()

option(ENABLE_ASAN "Build project executables with AddressSanitizer" OFF)
if (ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

add_executable(pipeline)
target_link_libraries(pipeline PRIVATE m Threads::Threads stb::stb TBB::tbb)
target_sources(pipeline PUBLIC
    source/filter.cpp
    source/image.cpp
    source/main.cpp
    source/pipeline-pthread.cpp
    ${PIPELINE_SERIAL_SOURCE}
    source/pipeline-tbb.cpp
    source/queue.cpp
)

target_compile_options(pipeline PUBLIC -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

add_executable(pipeline-notbb)
target_link_libraries(pipeline-notbb PRIVATE m Threads::Threads stb::stb)
target_sources(pipeline-notbb PUBLIC
    source/filter.cpp
    source/image.cpp
    source/main.cpp
    source/pipeline-pthread.cpp
    ${PIPELINE_SERIAL_SOURCE}
    source/queue.cpp
)

target_compile_options(pipeline-notbb PUBLIC -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

add_executable(generate-random tests/generate_random.cpp source/image.cpp)
target_link_libraries(generate-random PRIVATE stb::stb)

add_executable(check-pipelines tests/check_pipelines.cpp)
add_test(NAME check-pipelines COMMAND check-pipelines ${PIPELINE_DATA_DIR}/synthetic)

add_custom_target(run-serial
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pipeline-notbb --directory ${PIPELINE_DATA_DIR}/downloaded --pipeline serial
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(run-serial pipeline-notbb)

add_custom_target(run-pthread
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pipeline-notbb --directory ${PIPELINE_DATA_DIR}/downloaded --pipeline pthread
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(run-pthread pipeline-notbb)

add_custom_target(run-tbb
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pipeline --directory ${PIPELINE_DATA_DIR}/downloaded --pipeline tbb
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(run-tbb pipeline)

add_custom_target(run-all
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pipeline --directory ${PIPELINE_DATA_DIR}/downloaded --pipeline all
    COMMAND ${CMAKE_COMMAND} -E echo "-- vérification --"
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/check-pipelines ${PIPELINE_DATA_DIR}/downloaded
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    USES_TERMINAL
)
add_dependencies(run-all pipeline check-pipelines)

file(MAKE_DIRECTORY "${PIPELINE_DATA_DIR}/synthetic")
add_custom_target(generate-image
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/generate-random ${PIPELINE_DATA_DIR}/synthetic/0000.png
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
add_dependencies(generate-image generate-random)

add_test(NAME generate-synthetic-image
    COMMAND generate-random "${PIPELINE_DATA_DIR}/synthetic/0000.png")
set_tests_properties(generate-synthetic-image PROPERTIES FIXTURES_SETUP synthetic_image)

add_test(NAME run-serial-synthetic
    COMMAND pipeline --directory "${PIPELINE_DATA_DIR}/synthetic" --pipeline serial)
set_tests_properties(run-serial-synthetic PROPERTIES
    FIXTURES_REQUIRED synthetic_image
    FIXTURES_SETUP run-serial-synthetic)

add_test(NAME run-pthread-synthetic
    COMMAND pipeline-notbb --directory "${PIPELINE_DATA_DIR}/synthetic" --pipeline pthread)
set_tests_properties(run-pthread-synthetic PROPERTIES
    FIXTURES_REQUIRED synthetic_image
    FIXTURES_SETUP run-pthread-synthetic)

add_test(NAME run-tbb-synthetic
    COMMAND pipeline --directory "${PIPELINE_DATA_DIR}/synthetic" --pipeline tbb)
set_tests_properties(run-tbb-synthetic PROPERTIES
    FIXTURES_REQUIRED synthetic_image
    FIXTURES_SETUP run-tbb-synthetic)

set_tests_properties(check-pipelines PROPERTIES
    FIXTURES_REQUIRED "run-serial-synthetic;run-pthread-synthetic;run-tbb-synthetic")

install(TARGETS pipeline pipeline-notbb)
