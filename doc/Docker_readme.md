# ELFZ - ARM64 Development Environment
(ARM64 development environment on an x86_64 host using Docker and QEMU emulation.)

## 1. Prerequisites (Host Setup)
Before running the container after a system reboot, enable multi-architecture support:

sudo docker run --privileged --rm tonistiigi/binfmt --install all


## 2. Image Management
Build the Image
Use BuildKit to ensure proper cross-platform support. This command builds the ARM64 image even on an Intel host:
DOCKER_BUILDKIT=1 docker build --platform linux/arm64 -t zelf_arm64_build .


Clean Up:
To stop and remove the container:
docker stop elfz_dev && docker rm elfz_dev

## 3. Workflow Commands
Start the Environment
Launch the container in the background with debugging privileges enabled:
docker run -it -d --name elfz_dev --platform linux/arm64 --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -v "$(pwd)":/app zelf_arm64_build bash

Access the Container
Open an interactive terminal inside the running container:
docker exec -it elfz_dev bash

## 4. Installed Toolchain
Compiler: gcc (GNU C Compiler)
Assembler: as (GNU Assembler for ARM64)
Debugger: gdb & strace
Analysis: objdump, readelf, nm (from binutils)
Scripting: python3 (aliased to python)


## 5. Compiling for ARM64
Since NASM is not available for ARM, use GAS (GNU Assembler).

To assemble and link a file named main.s:

# Assembling
as -o main.o main.s
# Linking
ld -o main main.o
# Or via GCC
gcc -o main main.s


## 6. Reset Environment
To completely remove the container and the image to start from scratch:

# Stop and remove the container
docker stop elfz_dev && docker rm elfz_dev

# Remove the image
docker rmi zelf_arm64_build

# Optional: Clean up Docker build cache
docker builder prune -f

## strace
strace is not working well in a docker container, but we can use qemu integrated trace:
activate trace: export QEMU_STRACE=1
desactivate strace: unset QEMU_STRACE


## Docker file (./Dockerfile) :

FROM --platform=linux/arm64 debian:bookworm-slim
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    nasm \
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
