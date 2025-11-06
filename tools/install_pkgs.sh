#!/bin/bash -x
#
# install rtthread packages and patch them
#
pwd
# add blackmagic package to rtthread
patch -p1 -d ~/.env/packages/packages < patches/packages/packages.patch
# install arm_can_tool rtthread packages
pkgs --update
# patch rtthread and packages for arm_can_tool
patch -p1 -d ../../../ <  patches/01_dma_config.patch
patch -p1 -d ../../../ <  patches/02_dev_can.patch
patch -p1 -d ../../../ <  patches/03_dev_rtc.patch
patch -p1 -d packages/Lua-latest/ <  patches/04_lua.patch
patch -p1 -d packages/CmBacktrace-latest/ <  patches/05_cm_backtrace.patch
#patch -p1 -d ../../../ <  patches/06_drv_usart.patch
patch -p1 -d ../../../ <  patches/07_dev_soft_i2c.patch
#not truncated
