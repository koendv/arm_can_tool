# MicroRL with Raw Callback Mode for Lua Integration

This is a modified version of MicroRL (Micro Read Line) that adds a **raw callback mode** for seamless integration with Lua or other scripting languages.

## Features

- **Raw Callback Mode**: Execution callback receives raw command line buffer (no tokenization)
- **Completion Callback**: Receives raw command line buffer, handles all completion UI
- **Backward Compatible**: Traditional tokenized mode remains available
- **Less RAM**: No token pointer arrays

## Configuration

Enable raw callback mode by setting in `microrl_config.h`:

```c
#define MICRORL_CFG_USE_RAW_CALLBACKS 1
```
