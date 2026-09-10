FROM gcc:11 AS build

RUN apt-get update -o Acquire::Check-Valid-Until=false && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        python3 \
        python3-distutils \
        cmake \
        make && \
    curl -sS https://bootstrap.pypa.io/pip/3.9/get-pip.py -o /tmp/get-pip.py && \
    python3 /tmp/get-pip.py "pip<24" && \
    rm -rf /var/lib/apt/lists/* /tmp/get-pip.py

RUN pip3 install "conan<2.0"

RUN conan profile new default --detect && \
    conan profile update settings.compiler.libcxx=libstdc++11 default

WORKDIR /app

COPY conanfile.txt .

RUN mkdir -p build && cd build && \
    conan install .. --build=missing

COPY CMakeLists.txt .
COPY src/ ./src/

RUN cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

FROM ubuntu:22.04 AS run

RUN apt-get update && \
    apt-get install -y --no-install-recommends libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

RUN groupadd -r appgroup && useradd -r -g appgroup appuser

WORKDIR /app

COPY --from=build /app/build/bin/game_server ./game_server

COPY data/config.json ./data/config.json

COPY static/ ./static/

USER appuser

EXPOSE 8080

ENTRYPOINT ["./game_server", "data/config.json", "static"]
