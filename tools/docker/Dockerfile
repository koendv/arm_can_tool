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

FROM ubuntu:24.04 AS tools

# pinned versions of rt-thread and the RT-Thread packages registry
ENV RTTHREAD_SHA=f19da943aa905d9be49451e8cbad1a3fae0248b8
ENV PACKAGES_REGISTRY_SHA=16ed99eba0a7897311972254bafe0667afb145ee

ARG DEST_DIR=/root
WORKDIR ${DEST_DIR}

RUN apt-get update && apt-get upgrade -y

RUN apt-get install -y \
      vim git wget zip jq uuid-runtime build-essential xz-utils \
      python3 python-is-python3 python3-pip libncurses5-dev \
      scons python3-requests python3-psutil python3-kconfiglib python3-tqdm \
      python3-yaml pandoc

# ARM toolchain
RUN wget -O /tmp/toolchain.tgz -q "https://github.com/RT-Thread/toolchains-ci/releases/download/v1.8/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.xz" \
    && mkdir -p /opt \
    && tar -xf /tmp/toolchain.tgz -C /opt \
    && rm -f /tmp/toolchain.tgz

ENV RTT_EXEC_PATH=/opt/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi/bin
ENV RTT_CC=gcc

# RT-Thread env/pkgs tooling and package registry
RUN mkdir -p /root/.env/tools/scripts /root/.env/packages/packages \
    && echo 'source "$PKGS_DIR/packages/Kconfig"' > /root/.env/packages/Kconfig \
    && git clone https://github.com/RT-Thread/env.git /root/.env/tools/scripts/ \
    && git clone https://github.com/RT-Thread/packages.git /root/.env/packages/packages/ \
    && git -C /root/.env/packages/packages checkout $PACKAGES_REGISTRY_SHA

ENV PATH="/root/.env/tools/scripts:$RTT_EXEC_PATH:$PATH"

# ---------------------------------------------------------------------------
# "build" stage: takes tools, adds sources, builds firmware, bootloader, and manual.

FROM tools AS build

RUN git clone https://github.com/RT-Thread/rt-thread /root/rt-thread \
    && git -C /root/rt-thread checkout $RTTHREAD_SHA

ENV RTT_ROOT=/root/rt-thread

RUN git clone https://github.com/koendv/arm_can_tool /root/rt-thread/bsp/at32/arm_can_tool

WORKDIR /root/rt-thread/bsp/at32/arm_can_tool

RUN ./tools/install_pkgs.sh

RUN ./tools/manifest.sh ../../.. /root/.env/packages/packages > MANIFEST.txt

RUN scons

RUN ./tools/uf2conv/uf2conv.py -c -b 0x90000000 -f 0xf35c900d -o rtthread.uf2 rtthread.bin

RUN ./tools/mkepub.sh

RUN git clone https://github.com/koendv/at32f405-uf2boot /root/at32f405-uf2boot \
    && ./tools/manifest.sh /root/at32f405-uf2boot >> MANIFEST.txt

RUN cd /root/at32f405-uf2boot/ports/ArteryTek/at32f402_405 && make BOARD=arm_can_tool all

RUN mkdir -p /output \
    && cp MANIFEST.txt /output/ \
    && cp rtthread.uf2 rtthread.elf /output/ \
    && cp doc/arm_can_tool.epub /output/ \
    && cp /root/at32f405-uf2boot/ports/ArteryTek/at32f402_405/.build/arm_can_tool/cherryuf2_arm_can_tool.bin /output/ \
    && cp /root/at32f405-uf2boot/ports/ArteryTek/at32f402_405/.build/arm_can_tool/cherryuf2_arm_can_tool.elf /output/

# not truncated
