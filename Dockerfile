FROM debian:bookworm-slim

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        bash \
        build-essential \
        ca-certificates \
        clang \
        clang-format \
        cmake \
        coreutils \
        curl \
        gdb \
        git \
        make \
        ninja-build \
        python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
