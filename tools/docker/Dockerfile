# Build arm_can_tool firmware, bootloader, and manual.
#
# Used in:
# - github workflows
# - devcontainer (tools only)
# - linux build
#
# Full build:
#   docker build --target build -t arm_can_tool .
#   id=$(docker create arm_can_tool)
#   docker cp "$id":/output ./arm_can_tool_firmware
#   docker rm "$id"
#
# Tested:
# - amd64/ubuntu
# - arm64/raspberry pi os (use ubuntu:latest)
#

FROM ubuntu:24.04 AS tools

# pinned versions of rt-thread and the RT-Thread packages registry
ENV RTTHREAD_SHA=f19da943aa905d9be49451e8cbad1a3fae0248b8
ENV PACKAGES_REGISTRY_SHA=16ed99eba0a7897311972254bafe0667afb145ee

ARG DEST_DIR=/root
WORKDIR ${DEST_DIR}

RUN apt-get update && apt-get upgrade -y

RUN apt-get install -y apt-utils

RUN apt-get install -y \
      vim git wget zip jq uuid-runtime build-essential xz-utils \
      python3 python-is-python3 python3-pip libncurses5-dev \
      scons python3-requests python3-psutil python3-kconfiglib python3-tqdm \
      python3-yaml pandoc

# npm for xpm
COPY --from=node:22-bookworm-slim /usr/local /usr/local

RUN npm install --global xpm@latest \
    && xpm --version

# build runs as non-root user
ARG USER_UID=1000
ARG USER_GID=1000
RUN if [ "$USER_UID" != "1000" ] || [ "$USER_GID" != "1000" ]; then \
        groupadd -g $USER_GID devuser \
        && useradd -u $USER_UID -g $USER_GID -m -s /bin/bash devuser; \
    else \
        groupmod -n devuser ubuntu \
        && usermod -l devuser -d /home/devuser -m ubuntu; \
    fi

# /output directory for build results
RUN mkdir -p /output && chown $USER_UID:$USER_GID /output

USER devuser
WORKDIR /home/devuser

# ARM toolchain from xpack
RUN xpm init \
    && xpm install @xpack-dev-tools/arm-none-eabi-gcc@13.2.1-1.1.1 \
    && xpacks/.bin/arm-none-eabi-gcc --version

ENV PATH=/home/devuser/xpacks/.bin:${PATH}
ENV RTT_EXEC_PATH=/home/devuser/xpacks/.bin
ENV RTT_CC=gcc

# rt-thread env/pkgs
RUN mkdir -p /home/devuser/.env/tools/scripts /home/devuser/.env/packages/packages \
    && echo 'source "$PKGS_DIR/packages/Kconfig"' > /home/devuser/.env/packages/Kconfig \
    && git clone https://github.com/RT-Thread/env.git /home/devuser/.env/tools/scripts/ \
    && git clone https://github.com/RT-Thread/packages.git /home/devuser/.env/packages/packages/ \
    && git -C /home/devuser/.env/packages/packages checkout $PACKAGES_REGISTRY_SHA

ENV PATH="/home/devuser/.env/tools/scripts:$RTT_EXEC_PATH:$PATH"

# ---------------------------------------------------------------------------
# "build" stage: takes tools and sources, builds firmware, bootloader, and manual.

FROM tools AS build

RUN git clone https://github.com/RT-Thread/rt-thread /home/devuser/rt-thread \
    && git -C /home/devuser/rt-thread checkout $RTTHREAD_SHA

ENV RTT_ROOT=/home/devuser/rt-thread

RUN git clone https://github.com/koendv/arm_can_tool /home/devuser/rt-thread/bsp/at32/arm_can_tool

WORKDIR /home/devuser/rt-thread/bsp/at32/arm_can_tool

RUN ./tools/install_pkgs.sh

RUN ./tools/manifest.sh ../../.. /home/devuser/.env/packages/packages > MANIFEST.txt

RUN scons

RUN ./tools/uf2conv/uf2conv.py -c -b 0x90000000 -f 0xf35c900d -o rtthread.uf2 rtthread.bin

RUN ./tools/mkepub.sh

RUN git clone https://github.com/koendv/at32f405-uf2boot /home/devuser/at32f405-uf2boot \
    && ./tools/manifest.sh /home/devuser/at32f405-uf2boot >> MANIFEST.txt

RUN cd /home/devuser/at32f405-uf2boot/ports/ArteryTek/at32f402_405 && make BOARD=arm_can_tool all

RUN cp MANIFEST.txt /output/ \
    && cp rtthread.uf2 rtthread.elf /output/ \
    && cp doc/arm_can_tool.epub /output/ \
    && cp /home/devuser/at32f405-uf2boot/ports/ArteryTek/at32f402_405/.build/arm_can_tool/cherryuf2_arm_can_tool.bin /output/ \
    && cp /home/devuser/at32f405-uf2boot/ports/ArteryTek/at32f402_405/.build/arm_can_tool/cherryuf2_arm_can_tool.elf /output/

# not truncated
