# magiclib

`magiclib` in this repository refers to the host-side Qt5 library built from [`host/`](/home/michael/gitroot/pico-dev/debug_tool/host). The actual CMake target is `magictool_lib`, and the installed library artifact is `libmagictool.a`.

The main public API is [`DebugToolDevice`](/home/michael/gitroot/pico-dev/debug_tool/host/include/magictool/magicdebug.h), which wraps the Pico firmware's 2-byte USB CDC protocol behind a small synchronous Qt interface.

## Public header

```cpp
#include <magictool/magicdebug.h>
```

Namespace:

```cpp
magictool
```

## Installation

Build and install the host library directly from [`host/`](/home/michael/gitroot/pico-dev/debug_tool/host):

```bash
cmake -S host -B build/host
cmake --build build/host
cmake --install build/host
```

Default requirements:

- CMake 3.16 or newer
- C++17 compiler
- Qt5 Core
- Qt5 SerialPort
- Qt5 Widgets only if you also build the `magicUI` example

The host project installs to `/opt/magictool` by default. Override that with standard CMake cache variables:

```bash
cmake -S host -B build/host \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DMAGICTOOL_INSTALL_BINDIR=bin \
  -DMAGICTOOL_INSTALL_LIBDIR=lib \
  -DMAGICTOOL_INSTALL_INCLUDEDIR=include
cmake --build build/host
cmake --install build/host
```

To install the example programs as well:

```bash
cmake -S host -B build/host -DDEBUG_TOOL_QT5_BUILD_EXAMPLES=ON
cmake --build build/host
cmake --install build/host
```

Default install layout:

```text
/opt/magictool/lib/libmagictool.a
/opt/magictool/lib/pkgconfig/magictool.pc
/opt/magictool/inc/magictool/magicdebug.h
```

With `DEBUG_TOOL_QT5_BUILD_EXAMPLES=ON`, installation also adds:

```text
/opt/magictool/bin/magictool
/opt/magictool/bin/magicUI
```

## Linking

### CMake

Inside the repository, link against the build target directly:

```cmake
target_link_libraries(your_app PRIVATE magictool_lib)
```

If you consume the library source as a subdirectory, the alias target is also available:

```cmake
target_link_libraries(your_app PRIVATE debug_tool::qt5)
```

### pkg-config

Installation writes a `pkg-config` file to `lib/pkgconfig/magictool.pc`. If you install into a non-standard prefix, point `pkg-config` at it first:

```bash
export PKG_CONFIG_PATH=/opt/magictool/lib/pkgconfig:$PKG_CONFIG_PATH
```

Inspect the published flags:

```bash
pkg-config --cflags --libs magictool
```

Compile and link an application with `pkg-config`:

```bash
c++ -std=c++17 example.cpp -o example \
  $(pkg-config --cflags --libs magictool)
```

`magictool.pc` exposes:

- include flags for `magictool/magicdebug.h`
- `-lmagictool`
- private Qt5 dependencies on `Qt5Core` and `Qt5SerialPort`

## `DebugToolDevice`

The repository does not define a `DeviceToolDevice` class. The device wrapper is [`DebugToolDevice`](/home/michael/gitroot/pico-dev/debug_tool/host/include/magictool/magicdebug.h), and this is the class application code should use.

### Construction and connection

```cpp
explicit DebugToolDevice(int timeoutMs = 2000);

bool Open(const QString &portName, qint32 baudRate = QSerialPort::Baud115200);
bool Open(const QSerialPortInfo &portInfo, qint32 baudRate = QSerialPort::Baud115200);
void Close();

bool IsOpen() const;
QString PortName() const;

void SetTimeoutMs(int timeoutMs);
int TimeoutMs() const;
```

`DebugToolDevice` owns a `QSerialPort` and configures it for `115200 8N1` with no flow control. `Open()` clears any previous buffered state, opens the device, and prepares the object for synchronous request/response calls.

### Command methods

Output control:

- `Pulse(quint8 outputIndex, quint8 count = 1)`
- `Set(quint8 outputIndex)`
- `Clear(quint8 outputIndex)`
- `Toggle(quint8 outputIndex)`
- `WriteMask(quint8 mask)`

State queries:

- `ReadInputs(quint8 *bitsOut = nullptr)`
- `ReadOutputs(quint8 *bitsOut = nullptr)`
- `GetVersion(quint8 *versionOut = nullptr)`
- `GetHardwareVersion(quint8 *hardwareVersionOut = nullptr)`
- `Ping(quint8 value, quint8 *echoedOut = nullptr)`

Device actions:

- `OpenTool()`
- `CloseTool()`

Notification control:

- `EnableNotify(quint8 inputIndex)`
- `EnableAllNotify()`
- `DisableNotify(quint8 inputIndex)`
- `DisableAllNotify()`

The library validates output indexes `0..3` and input indexes `0..1` before sending a packet. Query methods decode the reply packet and copy the returned value into the optional output pointer when provided. `GetHardwareVersion()` returns one packed byte: high nibble hardware type (`0` unknown, `1` pico2, `2` pico2_w), low nibble hardware revision (`0` unknown, `1` v1, `2` v2, etc.). `OpenTool()` and `CloseTool()` send the firmware `OPEN` and `CLOSE` commands, which control the onboard indicator LED.

### Status and events

```cpp
QString LastResponse() const;
QString LastErrorString() const;
EventPacket LastPacket() const;

bool HasPendingEvent() const;
bool TakePendingEvent(EventPacket *eventOut);
bool WaitForEvent(EventPacket *eventOut, int timeoutMs = -1);
```

`LastResponse()` stores a human-readable summary of the most recent successful reply. `LastErrorString()` stores the most recent transport, timeout, or firmware-reported error.

The firmware may emit asynchronous input-change notifications while the library is waiting for a command response. [`DebugToolDevice`](/home/michael/gitroot/pico-dev/debug_tool/host/include/magictool/magicdebug.h) handles that by queueing unrelated packets in an internal pending-event queue. Use `HasPendingEvent()`, `TakePendingEvent()`, or `WaitForEvent()` to consume those notifications.

## Example usage

Minimal Qt application code:

```cpp
#include <QCoreApplication>
#include <QDebug>
#include <magictool/magicdebug.h>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    magictool::DebugToolDevice device(2000);

    if (!device.Open(QStringLiteral("/dev/ttyACM0"))) {
        qWarning().noquote() << "Open failed:" << device.LastErrorString();
        return 1;
    }

    if (!device.Pulse(0, 5)) {
        qWarning().noquote() << "Pulse failed:" << device.LastErrorString();
        return 1;
    }

    quint8 inputs = 0;
    if (!device.ReadInputs(&inputs)) {
        qWarning().noquote() << "ReadInputs failed:" << device.LastErrorString();
        return 1;
    }

    qInfo().noquote() << "Response:" << device.LastResponse();
    qInfo().noquote() << "Inputs:" << inputs;
    return 0;
}
```

Buildable CLI example:

- Source: [`host/examples/basic_usage.cpp`](/home/michael/gitroot/pico-dev/debug_tool/host/examples/basic_usage.cpp)
- Output binary: `build/host/magictool`

Build it with:

```bash
cmake -S host -B build/host -DDEBUG_TOOL_QT5_BUILD_EXAMPLES=ON
cmake --build build/host
```

Supported commands:

- `pulse <output> <count>`
- `set <output>`
- `clear <output>`
- `toggle <output>`
- `read-inputs`
- `read-outputs`
- `ping <value>`
- `version`
- `hardware`
- `open`
- `close`

Example invocations:

```bash
./build/host/magictool /dev/ttyACM0 pulse 0 5
./build/host/magictool /dev/ttyACM0 set 1
./build/host/magictool /dev/ttyACM0 read-inputs
./build/host/magictool /dev/ttyACM0 ping 42
./build/host/magictool /dev/ttyACM0 version
./build/host/magictool /dev/ttyACM0 hardware
./build/host/magictool /dev/ttyACM0 open
./build/host/magictool /dev/ttyACM0 close
```

Expected output shape:

```text
Response: ACK PULSE arg=5
Response: INPUTS info=0 arg=1
Value: 1
Response: ACK PING arg=42
Value: 42
```

If a command fails, the example prints `Command failed:` followed by `LastErrorString()`. Common causes are an unopened port, an invalid input/output index, a firmware timeout, or an `EVT_ERROR` reply from the device.

## Protocol model

The firmware protocol uses fixed-width 2-byte packets.

Host to device:

- byte 0: upper nibble = command
- byte 0: lower nibble = selector
- byte 1: command argument

Device to host:

- byte 0: upper nibble = event type
- byte 0: lower nibble = info
- byte 1: payload

Relevant protocol enums are declared in [`magicdebug.h`](/home/michael/gitroot/pico-dev/debug_tool/host/include/magictool/magicdebug.h):

- `CommandCode`
- `EventType`
- `ErrorCode`

`EventPacket` provides two helpers:

```cpp
EventType Type() const;
quint8 Info() const;
```
