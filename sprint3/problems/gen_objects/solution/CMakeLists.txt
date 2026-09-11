cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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
target_link_libraries(game_server PRIVATE Threads::Threads)

# ---------------------------------------------------------------------------
# Boost: two supported ways to get it, picked automatically.
#
# 1. Conan (conanbuildinfo_multi.cmake present, i.e. `conan install` was run
#    for this build dir) - used for local development, and required to build
#    the Catch2 unit tests below. This is what conanfile.txt is for.
# 2. A Boost already findable by CMake (e.g. installed via the system package
#    manager, as in the Docker image) - no Conan involved at all. This exists
#    because Conan Center's Boost recipe downloads sources from a mirror
#    (boostorg.jfrog.io) that is frequently broken/rate-limited, which made
#    Docker builds flaky; apt's Boost package doesn't have that problem and
#    is all the runtime image needs (it never builds the unit tests).
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake")
    include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    conan_basic_setup(TARGETS)

    target_link_libraries(game_server PRIVATE CONAN_PKG::boost)

    # -----------------------------------------------------------------------
    # Unit tests (Catch2) - only built when Conan is available, since that's
    # the only place catch2 comes from.
    # -----------------------------------------------------------------------
    add_executable(game_server_tests
        src/model.h
        src/model.cpp
        src/loot_generator.h
        src/loot_generator.cpp
        src/tagged.h
        tests/loot_generator_tests.cpp
        tests/model-tests.cpp
    )

    target_include_directories(game_server_tests PRIVATE src CONAN_PKG::boost)
    target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 CONAN_PKG::boost Threads::Threads)
else()
    find_package(Boost REQUIRED COMPONENTS json log log_setup thread filesystem date_time)
    target_link_libraries(game_server PRIVATE
        Boost::json
        Boost::log
        Boost::log_setup
        Boost::thread
        Boost::filesystem
        Boost::date_time
    )
    # Needed because the system Boost package ships shared (.so) libraries;
    # without this, boost::log's headers assume static linkage and the
    # binary fails to link.
    target_compile_definitions(game_server PRIVATE BOOST_LOG_DYN_LINK)
endif()
