# debug_tool

`debug_tool` is a Raspberry Pi Pico 2 / Pico 2 W helper project used to drive a GPIO pin over a simple USB serial command interface. It is intended as a standalone debug aid for other hardware and firmware work, including Argus-related debugging, but it is not part of the Argus project itself.

## Repository Layout

This repository is organized so the current Pico firmware lives in its own subdirectory and the project can grow without mixing host-side code, tests, and embedded firmware sources.

```text
debug_tool/
├── CMakeLists.txt
├── docs/
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
- `host/` contains native POSIX and Qt5 libraries for talking to the Pico CDC serial device.
- `docs/` contains project documentation for the host library and usage examples.
- `tests/` is reserved for unit tests and other automated validation.

## Requirements

- `cmake` 3.16 or newer
- a native build tool supported by CMake such as `make` or `ninja`
- A working Raspberry Pi Pico SDK checkout
- `PICO_SDK_PATH` exported in your shell environment
- An ARM embedded toolchain compatible with the Pico SDK
- Qt5 Core and Qt5 SerialPort development packages for the Qt5 host library
- Qt5 Widgets development packages for the `magicUI` host application

The firmware presets require `PICO_SDK_PATH`:

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
```

The host-only presets do not require the Pico SDK.

## Build

Prefer the CMake presets below. They keep host and firmware outputs in separate
build directories and avoid mixing the host toolchain with the Pico cross-toolchain.

### Host Applications

The host build produces:

- `magictool`, the command-line example from `host/examples/basic_usage.cpp`
- `magictool_native`, the native command-line example from `host/examples/basic_usage_native.cpp`
- `magicUI`, the Qt Widgets application
- `libmagictool_native.a`, the native POSIX host library
- `libmagictool_qt5.a`, the Qt5 host library

Build host Debug:

```bash
cmake --preset host-debug
cmake --build --preset host-debug
```

Build host Release:

```bash
cmake --preset host-release
cmake --build --preset host-release
```

Build the native POSIX host library and `magictool_native` without Qt:

```bash
cmake -S host -B build/host-native-only \
  -DDEBUG_TOOL_BUILD_QT5=OFF \
  -DDEBUG_TOOL_BUILD_NATIVE=ON \
  -DDEBUG_TOOL_NATIVE_BUILD_EXAMPLES=ON
cmake --build build/host-native-only
```

Host output paths:

```text
build/host-debug/host/magictool
build/host-debug/host/magictool_native
build/host-debug/host/magicUI
build/host-debug/host/libmagictool_native.a
build/host-debug/host/libmagictool_qt5.a
build/host-release/host/magictool
build/host-release/host/magictool_native
build/host-release/host/magicUI
build/host-release/host/libmagictool_native.a
build/host-release/host/libmagictool_qt5.a
```

### Firmware Only

Build Pico 2 Debug:

```bash
cmake --preset firmware-pico2-debug
cmake --build --preset firmware-pico2-debug
```

Build Pico 2 Release:

```bash
cmake --preset firmware-pico2-release
cmake --build --preset firmware-pico2-release
```

Build Pico 2 W Debug:

```bash
cmake --preset firmware-pico2w-debug
cmake --build --preset firmware-pico2w-debug
```

Build Pico 2 W Release:

```bash
cmake --preset firmware-pico2w-release
cmake --build --preset firmware-pico2w-release
```

Firmware UF2 output paths:

```text
build/firmware-pico2-debug/firmware/magictool_fw_pico2.uf2
build/firmware-pico2-release/firmware/magictool_fw_pico2.uf2
build/firmware-pico2w-debug/firmware/magictool_fw_pico2_w.uf2
build/firmware-pico2w-release/firmware/magictool_fw_pico2_w.uf2
```

### Host And Firmware Together

The combined presets build the selected firmware target and the host
applications in separate sub-builds under one top-level build directory.

Build all targets for Pico 2 Debug:

```bash
cmake --preset all-pico2-debug
cmake --build --preset all-pico2-debug
```

Build all targets for Pico 2 Release:

```bash
cmake --preset all-pico2-release
cmake --build --preset all-pico2-release
```

Build all targets for Pico 2 W Debug:

```bash
cmake --preset all-pico2w-debug
cmake --build --preset all-pico2w-debug
```

Build all targets for Pico 2 W Release:

```bash
cmake --preset all-pico2w-release
cmake --build --preset all-pico2w-release
```

Combined build output paths use nested sub-build directories:

```text
build/all-pico2-release/firmware/magictool_fw_pico2.uf2
build/all-pico2-release/host/magictool
build/all-pico2-release/host/magictool_native
build/all-pico2-release/host/magicUI
build/all-pico2w-release/firmware/magictool_fw_pico2_w.uf2
build/all-pico2w-release/host/magictool
build/all-pico2w-release/host/magictool_native
build/all-pico2w-release/host/magicUI
```

### Install Host Applications

The default install prefix is `/opt/magictool`. Install a host build with:

```bash
cmake --install build/host-release
```

If the prefix requires elevated permissions:

```bash
sudo cmake --install build/host-release
```

To install into a user-writable prefix, set the prefix when configuring:

```bash
cmake --preset host-release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build --preset host-release
cmake --install build/host-release
```

Installed host files:

```text
<prefix>/bin/magictool
<prefix>/bin/magictool_native
<prefix>/bin/magicUI
<prefix>/lib/libmagictool_native.a
<prefix>/lib/libmagictool_qt5.a
<prefix>/lib/pkgconfig/magictool_native.pc
<prefix>/lib/pkgconfig/magictool_qt5.pc
<prefix>/inc/magictool/magicdebug.h
<prefix>/inc/magictool/native/magicdebug.h
```

Combined builds install the host sub-build:

```bash
cmake --install build/all-pico2-release
```

## CI And Releases

GitHub Actions builds the host applications and Pico firmware on pull requests
and pushes to `main`.

Release builds run when a tag matching `v*` is pushed, or when the release
workflow is started manually from GitHub Actions. Tag releases publish these
artifacts to the GitHub release:

- `magictool-linux-x86_64.tar.gz`, containing the installed Linux host tools,
  static libraries, pkg-config files, and public headers
- `magictool_fw_pico2.uf2`
- `magictool_fw_pico2_w.uf2`

Create a release by tagging the commit and pushing the tag:

```bash
git tag v0.1.0
git push origin v0.1.0
```

### Manual CMake Options

Presets are wrappers around these root CMake options:

```text
DEBUG_TOOL_BUILD_HOST=ON|OFF
DEBUG_TOOL_BUILD_FIRMWARE=ON|OFF
DEBUG_TOOL_QT5_BUILD_EXAMPLES=ON|OFF
DEBUG_TOOL_NATIVE_BUILD_EXAMPLES=ON|OFF
PICO_2_W=ON|OFF
CMAKE_BUILD_TYPE=Debug|Release
```

For example, a manual host Release build is:

```bash
cmake -S . -B build/manual-host-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DDEBUG_TOOL_BUILD_FIRMWARE=OFF \
  -DDEBUG_TOOL_BUILD_HOST=ON \
  -DDEBUG_TOOL_QT5_BUILD_EXAMPLES=ON \
  -DDEBUG_TOOL_NATIVE_BUILD_EXAMPLES=ON
cmake --build build/manual-host-release
```

The direct `host/` build also accepts these host-specific options:

```text
DEBUG_TOOL_BUILD_QT5=ON|OFF
DEBUG_TOOL_BUILD_NATIVE=ON|OFF
```

A manual Pico 2 W firmware Release build is:

```bash
cmake -S . -B build/manual-firmware-pico2w-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DDEBUG_TOOL_BUILD_FIRMWARE=ON \
  -DDEBUG_TOOL_BUILD_HOST=OFF \
  -DPICO_2_W=ON
cmake --build build/manual-firmware-pico2w-release
```

To remove generated build output:

```bash
rm -rf build
```

For VS Code with CMake Tools, open one of these workspace files:

- `debug_tool_host.code-workspace`
- `debug_tool_firmware.code-workspace`

## Firmware Behavior

The firmware exposes a USB CDC interface with a compact 2-byte binary protocol.

- Outputs `0..3` are mapped to GPIO `2, 3, 4, 5`
- Inputs `0..1` are mapped to GPIO `6, 7`
- Host command packets are 2 bytes: upper nibble = command, lower nibble = selector, second byte = argument
- Firmware replies are 2-byte event packets and may also include asynchronous input-change notifications

The current firmware supports output control, input/output bitmap reads, notification enable/disable, firmware version query, hardware version query, and ping.

## Host Libraries

The host-side native CMake target is `magictool_native_lib`, and the built library file is `libmagictool_native.a`. It uses the C++ standard library plus POSIX serial calls.

The host-side Qt5 CMake target is `magictool_qt5_lib`, and the built library file is `libmagictool_qt5.a`. It wraps `QSerialPort` and exposes a small synchronous API for talking to the firmware over the CDC serial interface.

Qt5 public header:

```text
host/include/magictool/magicdebug.h
```

Native public header:

```text
host/include/magictool/native/magicdebug.h
```

Key methods:

- `Open(const QString &portName)`
- `Pulse(quint8 outputIndex, quint8 count = 1)`
- `Set(quint8 outputIndex)`
- `Clear(quint8 outputIndex)`
- `Toggle(quint8 outputIndex)`
- `ReadInputs(quint8 *bitsOut = nullptr)`
- `ReadOutputs(quint8 *bitsOut = nullptr)`
- `GetVersion(quint8 *versionOut = nullptr)`
- `GetHardwareVersion(quint8 *hardwareVersionOut = nullptr)`
- `Ping(quint8 value, quint8 *echoedOut = nullptr)`
- `OpenTool()`
- `CloseTool()`
- `LastResponse()`
- `LastErrorString()`

Library documentation:

- `host/README.md`
- `docs/magiclib.md`

Quick example:

```cpp
#include <magictool/magicdebug.h>

magictool::DebugToolDevice device;
if (!device.Open("/dev/ttyACM0")) {
    qWarning() << device.LastErrorString();
    return;
}

if (!device.Pulse(0, 5)) {
    qWarning() << device.LastErrorString();
}
```

A buildable CLI example is also provided at `host/examples/basic_usage.cpp`.

## Next Steps

The structure now supports the intended expansion path:

- move reusable serial protocol code into `host/`
- add command parsing and protocol unit tests under `tests/`
- keep device-specific firmware concerns isolated under `firmware/`
