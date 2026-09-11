cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Game server executable
# ---------------------------------------------------------------------------
add_executable(game_server
    src/main.cpp
    src/sdk.h
    src/http_server.h
    src/http_server.cpp
    src/logger.h
    src/logger.cpp
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/model.h
    src/model.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/extra_data.h
    src/tagged.h
    src/app.h
    src/app.cpp
    src/players.h
    src/players.cpp
    src/api_handler.h
    src/api_handler.cpp
    src/request_handler.h
    src/request_handler.cpp
)

target_include_directories(game_server PRIVATE src)
target_link_libraries(game_server PRIVATE Threads::Threads CONAN_PKG::boost)

# ---------------------------------------------------------------------------
# Unit tests (Catch2)
# ---------------------------------------------------------------------------
add_executable(game_server_tests
    src/model.h
    src/model.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/tagged.h
    tests/loot_generator_tests.cpp
    tests/model-tests.cpp
)

target_include_directories(game_server_tests PRIVATE src)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 CONAN_PKG::boost Threads::Threads)
