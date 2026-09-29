# ---------- Стадия сборки ----------
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        ca-certificates \
        python3 \
        python3-pip \
        python3-venv \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Conan 1.x — потому что conanfile.txt использует генератор cmake_multi
RUN pip3 install --no-cache-dir "conan==1.59.0"

WORKDIR /src

# Сначала конфиги (кэшируем слой установки зависимостей)
COPY conanfile.txt ./
COPY CMakeLists.txt ./

# Ставим зависимости в ./build
RUN mkdir -p build && cd build && \
    conan install .. \
        -s build_type=Release \
        -s compiler.libcxx=libstdc++11 \
        --build=missing

# Теперь весь исходник
COPY . .

WORKDIR /src/build
RUN cmake .. -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build . --target game_server -j"$(nproc)"

# ---------- Runtime-стадия ----------
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Бинарь по пути, который ждёт тест
COPY --from=builder /src/build/game_server /app/game_server

# Конфиг и статика
COPY --from=builder /src/config.json      /app/data/config.json
COPY --from=builder /src/static/          /app/static/

RUN chmod +x /app/game_server

# Том для state-файла монтируется снаружи как /tmp/volume
VOLUME ["/tmp/volume"]

EXPOSE 8080

ENTRYPOINT ["/app/game_server"]