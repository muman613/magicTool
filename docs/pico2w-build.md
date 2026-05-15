# Pico 2 W Build Targets

These commands build the Raspberry Pi Pico 2 W target. Firmware and all-target builds require `PICO_SDK_PATH`. Host application builds do not use `PICO_SDK_PATH`, FreeRTOS, LVGL, or `MAGICTOOL_HW_VERSION`.

Use the `debug` presets shown below for Debug builds. For Release builds, replace `debug` with `release` in both the preset name and build preset.

## Dependency Modes

| Mode | Configure Inputs |
| --- | --- |
| Download dependencies as needed | Leave `FREERTOS_KERNEL_PATH` unset. For hardware version 2, leave `LVGL_PATH` unset and keep `MAGICTOOL_FETCH_LVGL=ON`. |
| Use local dependency checkouts | Set `FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel`. For hardware version 2, also set `LVGL_PATH=/path/to/lvgl`. |

FreeRTOS must be the Raspberry Pi FreeRTOS-Kernel fork, because this firmware uses its RP2350 ARM non-secure port.

## Build Matrix

| Target | Configure Step | Build Step |
| --- | --- | --- |
| Pico 2 W HW1, all targets, download FreeRTOS if needed | `cmake --preset all-pico2w-debug -DMAGICTOOL_HW_VERSION=1` | `cmake --build --preset all-pico2w-debug` |
| Pico 2 W HW1, firmware only, download FreeRTOS if needed | `cmake --preset firmware-pico2w-debug -DMAGICTOOL_HW_VERSION=1` | `cmake --build --preset firmware-pico2w-debug` |
| Pico 2 W HW1, host applications only | `cmake --preset host-debug` | `cmake --build --preset host-debug` |
| Pico 2 W HW1, all targets, local FreeRTOS | `cmake --preset all-pico2w-debug -DMAGICTOOL_HW_VERSION=1 -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel` | `cmake --build --preset all-pico2w-debug` |
| Pico 2 W HW1, firmware only, local FreeRTOS | `cmake --preset firmware-pico2w-debug -DMAGICTOOL_HW_VERSION=1 -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel` | `cmake --build --preset firmware-pico2w-debug` |
| Pico 2 W HW2, all targets, download FreeRTOS and LVGL if needed | `cmake --preset all-pico2w-debug -DMAGICTOOL_HW_VERSION=2 -DMAGICTOOL_FETCH_LVGL=ON` | `cmake --build --preset all-pico2w-debug` |
| Pico 2 W HW2, firmware only, download FreeRTOS and LVGL if needed | `cmake --preset firmware-pico2w-debug -DMAGICTOOL_HW_VERSION=2 -DMAGICTOOL_FETCH_LVGL=ON` | `cmake --build --preset firmware-pico2w-debug` |
| Pico 2 W HW2, host applications only | `cmake --preset host-debug` | `cmake --build --preset host-debug` |
| Pico 2 W HW2, all targets, local FreeRTOS and LVGL | `cmake --preset all-pico2w-debug -DMAGICTOOL_HW_VERSION=2 -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel -DLVGL_PATH=/path/to/lvgl` | `cmake --build --preset all-pico2w-debug` |
| Pico 2 W HW2, firmware only, local FreeRTOS and LVGL | `cmake --preset firmware-pico2w-debug -DMAGICTOOL_HW_VERSION=2 -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel -DLVGL_PATH=/path/to/lvgl` | `cmake --build --preset firmware-pico2w-debug` |

## Outputs

| Target | Output |
| --- | --- |
| Pico 2 W firmware Debug | `build/firmware-pico2w-debug/firmware/magictool_fw_pico2_w.uf2` |
| Pico 2 W all-target Debug firmware | `build/all-pico2w-debug/firmware/magictool_fw_pico2_w.uf2` |
| Host applications Debug | `build/host-debug/host/magictool`, `build/host-debug/host/magictool_native`, `build/host-debug/host/magicUI` |
| All-target Debug host applications | `build/all-pico2w-debug/host/magictool`, `build/all-pico2w-debug/host/magictool_native`, `build/all-pico2w-debug/host/magicUI` |

## Install Host Applications and Libraries

Install is only meaningful for host builds and all-target builds. Firmware-only builds produce a `.uf2` file and do not install firmware to the host system.

The default install prefix is `/opt/magictool`. The installed files are:

| File | Purpose |
| --- | --- |
| `<prefix>/bin/magictool` | Qt5 command-line host example |
| `<prefix>/bin/magictool_native` | Native POSIX command-line host example |
| `<prefix>/bin/magicUI` | Qt Widgets host application |
| `<prefix>/lib/libmagictool_native.a` | Native POSIX static library |
| `<prefix>/lib/libmagictool_qt5.a` | Qt5 static library |
| `<prefix>/lib/pkgconfig/magictool_native.pc` | `pkg-config` metadata for the native library |
| `<prefix>/lib/pkgconfig/magictool_qt5.pc` | `pkg-config` metadata for the Qt5 library |
| `<prefix>/inc/magictool/magicdebug.h` | Qt5 public header |
| `<prefix>/inc/magictool/native/magicdebug.h` | Native public header |

| Install Target | Configure Step | Build Step | Install Step |
| --- | --- | --- | --- |
| Host applications and libraries to default `/opt/magictool` | `cmake --preset host-release` | `cmake --build --preset host-release` | `sudo cmake --install build/host-release` |
| Pico 2 W HW1 all-target host artifacts to default `/opt/magictool` | `cmake --preset all-pico2w-release -DMAGICTOOL_HW_VERSION=1` | `cmake --build --preset all-pico2w-release` | `sudo cmake --install build/all-pico2w-release` |
| Pico 2 W HW2 all-target host artifacts to default `/opt/magictool` | `cmake --preset all-pico2w-release -DMAGICTOOL_HW_VERSION=2 -DMAGICTOOL_FETCH_LVGL=ON` | `cmake --build --preset all-pico2w-release` | `sudo cmake --install build/all-pico2w-release` |
| Host applications and libraries to `/usr/local` | `cmake --preset host-release -DCMAKE_INSTALL_PREFIX=/usr/local -DMAGICTOOL_INSTALL_INCLUDEDIR=include` | `cmake --build --preset host-release` | `sudo cmake --install build/host-release` |
| Host applications and libraries to a user prefix | `cmake --preset host-release -DCMAKE_INSTALL_PREFIX="$HOME/.local/magictool"` | `cmake --build --preset host-release` | `cmake --install build/host-release` |

For a native-only install without Qt:

```bash
cmake -S host -B build/host-native-only \
  -DDEBUG_TOOL_BUILD_QT5=OFF \
  -DDEBUG_TOOL_BUILD_NATIVE=ON \
  -DDEBUG_TOOL_NATIVE_BUILD_EXAMPLES=ON \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DMAGICTOOL_INSTALL_INCLUDEDIR=include
cmake --build build/host-native-only
sudo cmake --install build/host-native-only
```

## pkg-config

For `/usr/local`, most Linux systems already search `/usr/local/lib/pkgconfig`. If `pkg-config` cannot find the package, or if you install into `/opt/magictool` or a user prefix, set `PKG_CONFIG_PATH`:

```bash
export PKG_CONFIG_PATH=/opt/magictool/lib/pkgconfig:$PKG_CONFIG_PATH
export PKG_CONFIG_PATH="$HOME/.local/magictool/lib/pkgconfig:$PKG_CONFIG_PATH"
```

Verify the installed metadata:

```bash
pkg-config --modversion magictool_native
pkg-config --cflags --libs magictool_native
pkg-config --modversion magictool_qt5
pkg-config --cflags --libs magictool_qt5
```

Compile against the installed native library:

```bash
c++ -std=c++17 native_example.cpp -o native_example \
  $(pkg-config --cflags --libs magictool_native)
```

Compile against the installed Qt5 library:

```bash
c++ -std=c++17 qt_example.cpp -o qt_example \
  $(pkg-config --cflags --libs magictool_qt5)
```
