FROM debian:stable-slim

# Install the compiler and parser-generator toolchain required by this practice.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bison \
        build-essential \
        cmake \
        flex \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the CMake project and build the semantic validator during image creation.
COPY CMakeLists.txt ./
COPY src ./src
COPY tests ./tests

RUN cmake -S . -B build \
    && cmake --build build

ENTRYPOINT ["./build/p07_validator"]
