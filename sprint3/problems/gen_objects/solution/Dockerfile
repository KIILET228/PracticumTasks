# syntax=docker/dockerfile:1

# ---------------------------------------------------------------------------
# Stage 1: build the game server
# ---------------------------------------------------------------------------
# NOTE: this deliberately does NOT use Conan to fetch Boost. Conan Center's
# boost recipe downloads sources from boostorg.jfrog.io, which is frequently
# broken/rate-limited (a long-standing, widely reported problem affecting
# many Boost versions, not just one) and made this build flaky/fail outright.
# The Docker image only needs the game_server binary (not the Catch2 unit
# tests, which still use Conan - see conanfile.txt/CMakeLists.txt for local
# development), so installing Boost from Ubuntu's own apt repository sidesteps
# that dependency entirely and is much more reliable in CI.
FROM ubuntu:24.04 AS build

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        libboost-json-dev \
        libboost-log-dev \
        libboost-thread-dev \
        libboost-filesystem-dev \
        libboost-date-time-dev \
    && \
    rm -rf /var/lib/apt/lists/*

COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN mkdir -p /app/build && cd /app/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --target game_server -j"$(nproc)"

# ---------------------------------------------------------------------------
# Stage 2: minimal runtime image
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS run

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        libboost-json1.83.0 \
        libboost-log1.83.0 \
        libboost-thread1.83.0 \
        libboost-filesystem1.83.0 \
        libboost-date-time1.83.0 \
    && \
    rm -rf /var/lib/apt/lists/* && \
    groupadd -r www && useradd -r -g www www

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

RUN chown -R www:www /app
USER www

EXPOSE 8080

ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/static"]
