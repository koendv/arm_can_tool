
## Building from Source

RT-Thread can be used with a graphical user interface (GUI) or compiled from the shell prompt (ENV). These are notes to compile on linux, from the shell prompt.

### Prerequisites & Toolchain

Prerequisites to compile the firmware on linux:

- install arm-none-eabi-gcc from [arm-none-eabi-gcc-xpack](https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/tag/v14.2.1-1.1)

- install clang from [clang-xpack](https://github.com/xpack-dev-tools/clang-xpack). Source files are formatted with `clang-format -i`

- install debian requirements for menuconfig:

  ```
  sudo apt-get install flex bison gperf python3-kconfiglib
  ```

- install the rt-thread [env](https://github.com//RT-Thread/env) environment.
  Get env install script:

  ```
  wget https://raw.githubusercontent.com/RT-Thread/env/master/install_ubuntu.sh
  ```

  Look at install_ubuntu.sh. Consider whether `apt update; apt upgrade -y` is really what you want. Comment out if not sure.

```
bash ./install_ubuntu.sh
```

Edit ~/.env/env.sh to use xpack:

```
export RTT_EXEC_PATH=/opt/xpack-arm-none-eabi-gcc-14.2.1-1.1/bin
```

In .profile, for execution at login:

```
export PATH=/opt/xpack-arm-none-eabi-gcc-14.2.1-1.1/bin:$PATH
export PATH=~/.env/tools/scripts:$PATH
export RTT_EXEC_PATH=/opt/xpack-arm-none-eabi-gcc-14.2.1-1.1/bin
source ~/.env/env.sh
```

Read ~/.env/tools/scripts/README.md . This file also contains info for Windows PowerShell.

### Compilation Steps

- set up the source directories

  ```
  git clone https://github.com/RT-Thread/rt-thread
  ```

Optional: in case future rt-thread releases break something, checkout this exact rt-thread version:

```
cd rt-thread
git checkout 1aef0dba719d9080690ca370605fdee947296dec
```

Continue.

```
cd rt-thread/bsp/at32
git clone https://github.com/koendv/arm_can_tool/
cd arm_can_tool
```

- install blackmagic as a rt-thread package. install and patch packages needed by "arm can tool". 

```
cat patches/README.md
./tools/install_pkgs.sh
```

  After patching .env, when running [menuconfig](https://github.com/RT-Thread/rt-thread/blob/master/documentation/env/env.md#bsp-configuration-menuconfig) in the arm_can_tool project directory, "blackmagic" appears as a rt-thread package. In menuconfig, `/blackmagic` will search the .config file for the blackmagic rt-thread package.

  ```
  cd ~/src/rt-thread/bsp/at32/arm_can_tool
  menuconfig
  /blackmagic
  ESC
  Q
  ```

- compile

  ```
  cd ~/src/rt-thread/bsp/at32/arm_can_tool
  scons -c
  scons
  arm-none-eabi-objcopy -O binary rtthread.elf rtthread.bin
  # uf2 family id 0xf35c900d load address 0x90000000
  ./tools/uf2conv/uf2conv.py -b 0x90000000 -f 0xf35c900d -o rtthread.uf2 rtthread.bin
  ```

This creates the uf2 binary rtthread.uf2.
To install the uf2 binary, see "install application".

