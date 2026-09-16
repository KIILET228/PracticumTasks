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

# Статическая библиотека с игровой моделью и всеми её зависимостями.
# В ней нет ни строчки сетевого кода - только то, что реально покрыто
# юнит-тестами Catch2. Благодаря этому она не тянет за собой Boost
# и линкуется одинаково и в game_server, и в game_server_tests,
# без дублирования компиляции.
add_library(game_model STATIC
	src/tagged.h
	src/loot_generator.h
	src/loot_generator.cpp
	src/model.h
	src/model.cpp
)
target_include_directories(game_model PUBLIC src)

add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/extra_data.h
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
target_link_libraries(game_server PRIVATE game_model Threads::Threads CONAN_PKG::boost)

add_executable(game_server_tests
	tests/loot_generator_tests.cpp
	tests/model-tests.cpp
)
target_link_libraries(game_server_tests PRIVATE game_model CONAN_PKG::catch2 Threads::Threads)
