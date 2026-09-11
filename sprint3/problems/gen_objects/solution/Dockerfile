# syntax=docker/dockerfile:1

# ---------------------------------------------------------------------------
# Stage 1: build the game server with Conan + CMake
# ---------------------------------------------------------------------------
FROM gcc:12.3.0 AS build

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        python3-pip \
        python3-setuptools \
        cmake \
    && \
    rm -rf /var/lib/apt/lists/* && \
    pip3 install --no-cache-dir --break-system-packages "conan==1.65.*"

# The conanfile.txt uses the [generators] cmake_multi, which needs one
# `conan install` per build type (it then merges them into
# conanbuildinfo_multi.cmake, picked up by CMakeLists.txt based on
# CMAKE_BUILD_TYPE).
COPY conanfile.txt /app/
RUN mkdir -p /app/build && cd /app/build && \
    conan profile new default --detect >/dev/null && \
    conan profile update settings.compiler.libcxx=libstdc++11 default && \
    conan install .. --build=missing -s build_type=Debug && \
    conan install .. --build=missing -s build_type=Release

COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --target game_server -j"$(nproc)"

# ---------------------------------------------------------------------------
# Stage 2: minimal runtime image
# ---------------------------------------------------------------------------
FROM ubuntu:22.04 AS run

RUN groupadd -r www && useradd -r -g www www

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

RUN chown -R www:www /app
USER www

EXPOSE 8080

ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/static"]
