cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)

add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/model.h
	src/model.cpp
	src/tagged.h
	src/geom.h
	src/collision_detector.h
	src/collision_detector.cpp
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/extra_data.h
	src/loot_generator.h
	src/loot_generator.cpp
	src/players.h
	src/players.cpp
	src/app.h
	src/app.cpp
	src/api_handler.h
	src/api_handler.cpp
	src/request_handler.cpp
	src/request_handler.h
	src/ticker.h
	src/logger.h
	src/logger.cpp
)
target_link_libraries(game_server PRIVATE Threads::Threads CONAN_PKG::boost)

add_executable(game_server_tests
	src/model.h
	src/model.cpp
	src/tagged.h
	src/geom.h
	src/collision_detector.h
	src/collision_detector.cpp
	src/loot_generator.h
	src/loot_generator.cpp
	tests/loot_generator_tests.cpp
	tests/model-tests.cpp
)
target_include_directories(game_server_tests PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 CONAN_PKG::boost Threads::Threads)
