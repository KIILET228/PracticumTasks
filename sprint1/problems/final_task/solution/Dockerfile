# ---------- Этап 1: сборка ----------
FROM gcc:11 AS build

RUN apt-get update && \
    apt-get install -y \
        python3-pip \
        cmake \
        make && \
    rm -rf /var/lib/apt/lists/*

# conanfile.txt использует старый формат (генератор "cmake") — это Conan 1.x стиль
RUN pip3 install "conan<2.0"

RUN conan profile new default --detect && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

WORKDIR /app

# Сначала копируем только conanfile.txt, чтобы кэшировать слой с зависимостями
COPY conanfile.txt .

RUN mkdir -p build && cd build && \
    conan install .. --build=missing

# Копируем остальные файлы проекта
COPY CMakeLists.txt .
COPY src/ ./src/

# Собираем проект (таргет в CMakeLists.txt называется game_server)
RUN cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

# ---------- Этап 2: минимальный рантайм-образ ----------
FROM ubuntu:22.04 AS run

RUN apt-get update && \
    apt-get install -y --no-install-recommends libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

RUN groupadd -r appgroup && useradd -r -g appgroup appuser

WORKDIR /app

# Копируем собранный бинарник. conan_basic_setup() переносит executable
# в build/bin/, а не оставляет его прямо в build/
COPY --from=build /app/build/bin/game_server ./game_server

# config.json лежит в подпапке data, копируем с сохранением этой структуры
COPY data/config.json ./data/config.json

USER appuser

EXPOSE 8080

ENTRYPOINT ["./game_server", "data/config.json"]
