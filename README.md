# debug_tool

`debug_tool` is a Raspberry Pi Pico 2 W helper project used to drive a GPIO pin over a simple USB serial command interface. It is intended as a standalone debug aid for other hardware and firmware work, including Argus-related debugging, but it is not part of the Argus project itself.

## Repository Layout

This repository is organized so the current Pico firmware lives in its own subdirectory and the project can grow without mixing host-side code, tests, and embedded firmware sources.

```text
debug_tool/
├── CMakeLists.txt
├── README.md
├── firmware/
│   ├── CMakeLists.txt
│   └── main.cpp
├── host/
│   ├── CMakeLists.txt
│   ├── include/
│   └── src/
└── tests/
```

- `firmware/` contains the Pico SDK based firmware build.
- `host/` contains a Qt5 compatibility library for talking to the Pico CDC serial device.
- `tests/` is reserved for unit tests and other automated validation.

## Requirements

- `cmake` 3.13 or newer
- a native build tool supported by CMake such as `make` or `ninja`
- A working Raspberry Pi Pico SDK checkout
- `PICO_SDK_PATH` exported in your shell environment
- An ARM embedded toolchain compatible with the Pico SDK
- Qt5 Core and Qt5 SerialPort development packages for the host library

Example:

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build
cmake --build build
```

If you only want the Qt5 host library and do not want to configure the firmware build:

```bash
cmake -S . -B build -DDEBUG_TOOL_BUILD_FIRMWARE=OFF
cmake --build build --target debug_tool_qt5
```

For VS Code with CMake Tools, the workspace is configured to use `host/` as the active `cmake.sourceDirectory`, with a build directory under `build/vscode-host`. That keeps editor configure/build actions on the Qt library project instead of the mixed root build.

If you want clean editor workflows for both sides of the repository, open one of these workspace files in VS Code:

- `debug_tool_host.code-workspace`
- `debug_tool_firmware.code-workspace`

## Build

Configure and build from the repository root:

```bash
cmake -S . -B build
cmake --build build
```

The root `CMakeLists.txt` supports two build modes:

- When building only one side, it adds `firmware/` or `host/` directly with `add_subdirectory()`.
- When building both firmware and host together, it uses separate sub-builds so the native Qt toolchain and Pico cross-toolchain do not conflict.

In the combined build, firmware artifacts such as `.uf2`, `.elf`, and related files are placed under `build/firmware/`, and the Qt5 compatibility library is built under `build/host/`. In host-only mode, `debug_tool_qt5` is built directly in the selected build tree.

To remove generated build output:

```bash
rm -rf build
```

## Firmware Behavior

The firmware exposes a USB serial command interface and drives GPIO 2 on the Pico 2 W.

Supported commands:

- `PULSE <n>`: output `n` pulses on GPIO 2
- `SET <0|1>`: set GPIO 2 low or high
- `CLR`: drive GPIO 2 low
- `TOGGLE`: toggle GPIO 2

Responses are written back over USB serial as plain text status messages.

## Qt5 Host Library

The host-side Qt5 library target is `debug_tool_qt5`. It wraps `QSerialPort` and exposes a small synchronous API for talking to the firmware over the CDC serial interface.

Public header:

```text
host/include/debug_tool_qt5/DebugToolDevice.h
```

Key methods:

- `Open(const QString &portName)`
- `Pulse(int count)`
- `Set(int value)`
- `Clear()`
- `Toggle()`
- `LastResponse()`
- `LastErrorString()`

Library documentation:

- `host/README.md`

Quick example:

```cpp
#include <debug_tool_qt5/DebugToolDevice.h>

debug_tool_qt5::DebugToolDevice device;
if (!device.Open("/dev/ttyACM0")) {
    qWarning() << device.LastErrorString();
    return;
}

if (!device.Pulse(5)) {
    qWarning() << device.LastErrorString();
}
```

A buildable CLI example is also provided at `host/examples/basic_usage.cpp`.

## Next Steps

The structure now supports the intended expansion path:

- move reusable serial protocol code into `host/`
- add command parsing and protocol unit tests under `tests/`
- keep device-specific firmware concerns isolated under `firmware/`
