# In directory with Dockerfile:
# docker build $PWD -t arm_can_tool
# docker run --interactive --tty  arm_can_tool:latest bash
# to compile arm_can_tool, at the shell prompt type:
# cd /root/rt-thread/bsp/at32/arm_can_tool
# scons
#
FROM ubuntu:22.04

ARG DEST_DIR=/root

WORKDIR ${DEST_DIR}

#system
RUN apt-get update

RUN apt-get upgrade -y

RUN apt-get install -y vim git wget python3 python-is-python3 python3-pip libncurses5-dev

RUN apt-get install -y pandoc weasyprint ghostscript fonts-linuxlibertine fonts-noto-cjk fonts-noto-mono

# toolchain
RUN wget -O /tmp/toolchain.tgz -q "https://github.com/RT-Thread/toolchains-ci/releases/download/v1.8/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.xz"

ENV RTT_EXEC_PATH=/opt/arm-gnu-toolchain-13.2.Rel1-x86_64-arm-none-eabi/bin

RUN sudo mkdir -p /opt

RUN sudo tar -xf "/tmp/toolchain.tgz" -C /opt

RUN rm -f /tmp/toolchain.tgz

#env
RUN mkdir /root/.env

RUN mkdir /root/.env/tools

RUN mkdir /root/.env/packages

RUN mkdir /root/.env/packages/packages

RUN mkdir /root/.env/tools/scripts

RUN touch /root/.env/packages/Kconfig

RUN echo 'source "$PKGS_DIR/packages/Kconfig"' > /root/.env/packages/Kconfig

RUN git clone https://github.com/RT-Thread/env.git /root/.env/tools/scripts/

RUN git clone https://github.com/RT-Thread/packages.git /root/.env/packages/packages/

ENV PATH="/root/.env/tools/scripts:$RTT_EXEC_PATH:$PATH"

RUN apt-get install -y scons python3-requests python3-psutil python3-kconfiglib python3-tqdm

ENV RTT_CC=gcc

RUN git clone https://github.com/RT-Thread/rt-thread /root/rt-thread

RUN git clone https://github.com/koendv/arm_can_tool /root/rt-thread/bsp/at32/arm_can_tool

WORKDIR /root/rt-thread/bsp/at32/arm_can_tool

RUN ./tools/install_pkgs.sh

RUN scons

#not truncated
