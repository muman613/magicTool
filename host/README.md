# magictool

`magictool` is a Qt5 library for talking to the `debug_tool` Pico firmware over its USB CDC interface. It wraps `QSerialPort`, sends the firmware's 2-byte binary protocol, and provides a synchronous API for output control, state queries, and notification management.

## Protocol model

The firmware protocol uses fixed-width 2-byte packets.

Host to Pico:

- byte 0: upper nibble = command, lower nibble = selector
- byte 1: command argument

Pico to host:

- byte 0: upper nibble = event type, lower nibble = info
- byte 1: payload

The firmware can emit asynchronous `EVT_INPUT_CHANGE` packets at any time. The host library handles that by queueing unexpected packets while it waits for the specific reply to the command it just sent.

## Public API

Public header:

```cpp
#include <magictool/magicdebug.h>
```

Key types:

```cpp
enum CommandCode : quint8;
enum EventType : quint8;
enum ErrorCode : quint8;

struct EventPacket {
    quint8 header;
    quint8 arg;

    EventType Type() const;
    quint8 Info() const;
};
```

Class overview:

```cpp
class DebugToolDevice {
public:
    explicit DebugToolDevice(int timeoutMs = 2000);

    bool Open(const QString &portName, qint32 baudRate = QSerialPort::Baud115200);
    bool Open(const QSerialPortInfo &portInfo, qint32 baudRate = QSerialPort::Baud115200);
    void Close();

    bool IsOpen() const;
    QString PortName() const;

    void SetTimeoutMs(int timeoutMs);
    int TimeoutMs() const;

    QString LastResponse() const;
    QString LastErrorString() const;
    EventPacket LastPacket() const;

    bool HasPendingEvent() const;
    bool TakePendingEvent(EventPacket *eventOut);
    bool WaitForEvent(EventPacket *eventOut, int timeoutMs = -1);

    bool Pulse(quint8 outputIndex, quint8 count = 1);
    bool Set(quint8 outputIndex);
    bool Clear(quint8 outputIndex);
    bool Toggle(quint8 outputIndex);
    bool WriteMask(quint8 mask);

    bool ReadInputs(quint8 *bitsOut = nullptr);
    bool ReadOutputs(quint8 *bitsOut = nullptr);

    bool EnableNotify(quint8 inputIndex);
    bool EnableAllNotify();
    bool DisableNotify(quint8 inputIndex);
    bool DisableAllNotify();

    bool GetVersion(quint8 *versionOut = nullptr);
    bool GetHardwareVersion(quint8 *hardwareVersionOut = nullptr);
    bool Ping(quint8 value, quint8 *echoedOut = nullptr);
    bool OpenTool();
    bool CloseTool();
};
```

Behavior notes:

- `Pulse()`, `Set()`, `Clear()`, and `Toggle()` address firmware outputs `0..3`.
- `ReadInputs()` returns the current 2-bit input bitmap.
- `ReadOutputs()` returns the current 4-bit output bitmap.
- `GetHardwareVersion()` returns one packed byte: high nibble hardware type (`0` unknown, `1` pico2, `2` pico2_w), low nibble hardware revision (`0` unknown, `1` v1, `2` v2, etc.).
- `EnableNotify()` and `DisableNotify()` address inputs `0..1`.
- `OpenTool()` and `CloseTool()` send the firmware `OPEN` and `CLOSE` commands, which control the onboard indicator LED.
- `LastResponse()` is a human-readable summary of the most recent reply packet.
- `LastErrorString()` contains transport errors, timeout errors, or decoded firmware error packets.

## Build and link

Build the host library on its own:

```bash
cmake -S host -B build/host
cmake --build build/host
```

This produces:

```text
build/host/libmagictool.a
```

Dependencies:

- CMake 3.16 or newer
- Qt5 Core
- Qt5 SerialPort
- Qt5 Widgets for the `magicUI` example
- A C++17 compiler

Install the library and examples:

```bash
cmake -S host -B build/host -DDEBUG_TOOL_QT5_BUILD_EXAMPLES=ON
cmake --build build/host
cmake --install build/host
```

Default install layout:

```text
/opt/magictool/bin/magictool
/opt/magictool/bin/magicUI
/opt/magictool/lib/libmagictool.a
/opt/magictool/lib/pkgconfig/magictool.pc
/opt/magictool/inc/magictool/magicdebug.h
```

Override the prefix or install subdirectories with standard CMake cache variables:

```bash
cmake -S host -B build/host \
  -DCMAKE_INSTALL_PREFIX=/some/prefix \
  -DMAGICTOOL_INSTALL_BINDIR=bin \
  -DMAGICTOOL_INSTALL_LIBDIR=lib \
  -DMAGICTOOL_INSTALL_INCLUDEDIR=inc
```

## Example usage

```cpp
#include <QDebug>
#include <magictool/magicdebug.h>

magictool::DebugToolDevice device(2000);

if (!device.Open(QStringLiteral("/dev/ttyACM0"))) {
    qWarning() << "Open failed:" << device.LastErrorString();
    return;
}

if (!device.Pulse(0, 5)) {
    qWarning() << "Pulse failed:" << device.LastErrorString();
    return;
}

quint8 inputs = 0;
if (!device.ReadInputs(&inputs)) {
    qWarning() << "ReadInputs failed:" << device.LastErrorString();
    return;
}

qDebug() << "Last response:" << device.LastResponse();
qDebug() << "Input bitmap:" << inputs;
```

A buildable CLI example is included at `host/examples/basic_usage.cpp` and builds as `magictool`.

When `DEBUG_TOOL_QT5_BUILD_EXAMPLES=ON`, the host build also produces `magicUI`, a Qt5 Widgets example application that:

- opens a selected serial port
- fetches and logs the firmware and hardware versions immediately after connect
- provides buttons for all output operations (`set`, `clear`, `toggle`, `pulse`, `write mask`)
- exposes query and protocol actions (`read-inputs`, `read-outputs`, `version`, `hardware`, `ping`, `open`, `close`, notify enable/disable)
- shows live input transitions using firmware notifications

Example invocations:

```bash
./build/host/magictool /dev/ttyACM0 pulse 0 5
./build/host/magictool /dev/ttyACM0 set 1
./build/host/magictool /dev/ttyACM0 clear 1
./build/host/magictool /dev/ttyACM0 toggle 2
./build/host/magictool /dev/ttyACM0 read-inputs
./build/host/magictool /dev/ttyACM0 read-outputs
./build/host/magictool /dev/ttyACM0 ping 42
./build/host/magictool /dev/ttyACM0 version
./build/host/magictool /dev/ttyACM0 hardware
./build/host/magictool /dev/ttyACM0 open
./build/host/magictool /dev/ttyACM0 close
```

## Failure modes

Common reasons a call returns `false`:

- the port could not be opened
- the device is not connected
- the firmware did not answer before the timeout
- the firmware returned an `EVT_ERROR` packet
- the requested input or output selector is invalid

Inspect `LastErrorString()` first when a call fails.
