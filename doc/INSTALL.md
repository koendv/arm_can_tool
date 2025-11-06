# firmware for arm can tool.
Instructions to install bootloader and application.

## bootloader
To install the bootloader using dfu-util:

- connect the board to usb
- push reset and boot0 buttons
- wait one second
- release the reset button
- wait one second
- release the boot0 button
- write the bootloader using dfu-util

```
dfu-util -a 0 -d 2e3c:df11 --dfuse-address 0x08000000 -D cherryuf2_arm_can_tool.bin
```

The bootloader only needs to be installed once.

## application

To install the application using the bootloader:

- connect the board to usb
- push reset button and multi-direction switch
- wait one second
- release the reset button
- wait one second
- release the multi-direction switch
- a mass storage device appears
- copy the file `rt-thread.uf2` to the file `CURRENT.UF2`
```
cp rtthread.uf2 /media/$USER/CherryUF2/CURRENT.UF2
```

## see also

For complete installation instructions, and more recent firmware, see [documentation](https://github.com/koendv/arm_can_toolt?tab=readme-ov-file#firmware)

