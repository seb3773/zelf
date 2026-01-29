FROM multiarch/qemu-user-static:x86_64-aarch64 AS qemu

FROM --platform=linux/arm64 debian:bookworm-slim

COPY --from=qemu /usr/bin/qemu-aarch64-static /usr/bin/

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    git \
    python3 \
    python3-pip \
    python3-venv \
    strace \
    gdb \
    binutils \
    && rm -rf /var/lib/apt/lists/*

RUN ln -s /usr/bin/python3 /usr/bin/python

WORKDIR /app
