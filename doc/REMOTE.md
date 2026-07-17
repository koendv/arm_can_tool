# ARM CAN TOOL — Remote Use

This chapter applies to deployments where the ARM CAN Tool operates unattended at a remote location.
In this scenario, physical access to the display and multi-direction switch is not available.
The serial console, connected via a cellular modem, provides remote control.

Connect a cellular modem to the serial console on the SWD connector (pins RXD, TXD, 115200 8N1, no flow control). See [REFERENCE](REFERENCE.md#probe-swd-pinout) for pinout.

## Watchdog

In a remote deployment, a hung ARM CAN Tool cannot be power-cycled without a site visit.
Enable the watchdog to allow ARM CAN Tool to recover automatically.
The watchdog can be enabled using the display menu, or using the command line.

Using the menu:

- Startup -> next ->watchdog
- Settings -> Store

and reset.

or using the command line:

```
msh />settings set watchdog_enable 1
watchdog_enable=1
msh />settings write
msh />reboot
```

If the watchdog is enabled, and the device is unresponsive for 20 seconds, the watchdog reboots the device.

## Remote Operation Commands

The following commands replace the display and multi-direction switch when physical access is not available.

### `settings`

The `settings` command is the primary tool for remote configuration.

`settings (read|write|default|list|set <field> <value>|preset [0-15])`

Active settings are in RAM. 16 settings presets, numbered 0 to 15, are stored in EEPROM. Preset 0 is loaded at boot.

| Command | Action |
| ------- | ------ |
| `settings read` | Load active settings from preset |
| `settings write` | Write active settings to preset |
| `settings default` | Reset active settings to default values |
| `settings list` | List active settings |
| `settings set <field> <value>` | Modify active setting |
| `settings preset [0-15]` | Set preset number, default 0 |

Example — enable CAN filter and reboot to apply:

```
msh />settings list
[arm_can_tool]
version=1
language=0
mode=1
polling_interval=5
attach_enable=0
memwatch_enable=0
trigger_enable=0
(output truncated)
msh />settings set canfilter_enable 1
canfilter_enable=1
msh />settings write
msh />reboot
```

`reboot` is a standard RT-Thread command.

### `screen`

The `screen` command renders the display as a dot-matrix image.

```
msh />screen

⢰⡀⢀⡆⠀⣀⣀⠀⠀⣀⡀⡇⠀⣀⣀
⢸⠱⠎⡇⢸⠀⠀⡇⢸⠀⠈⡇⢸⠤⠤⠇
⠸⠀⠀⠇⠘⠤⠤⠃⠘⠤⠔⠇⠘⠤⠤⠂

(output truncated)
```

### `mui`

The `mui` command navigates the display menu. It has the same effect as operating the multi-direction switch.

`mui next|prev|inc|dec|select [...]`

Arguments are processed left to right. Prefix matching is used: `n`, `ne`, `nex`, `next` all match `next`. On the first unrecognised argument, a usage line is printed and processing stops. Up to 4 mui commands can be queued (MUI_MAX_MSG).

| Command | Switch action |
| ------- | ------------- |
| `mui next` | Down |
| `mui prev` | Up |
| `mui select` | Press |

Example:
```
msh />screen
msh />mui n n s
msh />screen
```

### `trst`

If the target reset is connected to AUX connector NRST:

`trst` reads the reset pin:

```
msh />trst
trst_in high
```

To toggle target reset:

```
msh />trst high
trst_out high
trst_in low
msh />trst low
trst_out low
trst_in high
```

### `tail`

To see the last lines of a log file, use `tail`:

```
msh />ls /sdcard
msh />tail -n 20 /sdcard/6a46f7d4.log
```

---

