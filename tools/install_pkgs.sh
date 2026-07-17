#!/bin/bash -x
#
# install rtthread packages and patch them
#
pwd
# add blackmagic package to rtthread
if ! grep -q blackmagic ~/.env/packages/packages/tools/Kconfig; then
    echo 'source "$PKGS_DIR/packages/tools/blackmagic/Kconfig"' >> ~/.env/packages/packages/tools/Kconfig
    mkdir ~/.env/packages/packages/tools/blackmagic
    cp patches/packages/Kconfig ~/.env/packages/packages/tools/blackmagic
    cp patches/packages/package.json ~/.env/packages/packages/tools/blackmagic
fi
# patch -p1 -d ~/.env/packages/packages < patches/packages/packages.patch
# install arm_can_tool rtthread packages
pkgs --update
# patch rtthread and packages for arm_can_tool
patch -p1 -d ../../../ <  patches/01_dma_config.patch
#patch -p1 -d ../../../ <  patches/02_dev_can.patch # now using HAL
patch -p1 -d ../../../ <  patches/03_dev_rtc.patch
patch -p1 -d ../../../ <  patches/07_dev_soft_i2c.patch # less logging
patch -p1 -d ../../../ <  patches/09-rt_usbd_serial.patch # needed
patch -p1 -d ../../../ <  patches/12-tail.patch # faster
#not truncated
