# magiclib

`magiclib` in this repository refers to the host-side Qt5 library built from [`host/`](/home/michael/gitroot/pico-dev/debug_tool/host). The actual CMake target is `magictool_lib`, and the produced static library is typically `libmagictool.a`.

The main public API is [`DebugToolDevice`](/home/michael/gitroot/pico-dev/debug_tool/host/include/debug_tool_qt5/DebugToolDevice.h), which wraps the Pico firmware's 2-byte USB CDC protocol behind a small synchronous Qt interface.

## Public header

```cpp
#include <debug_tool_qt5/DebugToolDevice.h>
```

Namespace:

```cpp
debug_tool_qt5
```

## DeviceToolDevice class

The repository does not currently define a `DeviceToolDevice` class. The device wrapper is [`DebugToolDevice`](/home/michael/gitroot/pico-dev/debug_tool/host/include/debug_tool_qt5/DebugToolDevice.h), and this is the class application code should use.

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
- `Ping(quint8 value, quint8 *echoedOut = nullptr)`

Notification control:

- `EnableNotify(quint8 inputIndex)`
- `EnableAllNotify()`
- `DisableNotify(quint8 inputIndex)`
- `DisableAllNotify()`

The library validates output indexes `0..3` and input indexes `0..1` before sending a packet. Query methods decode the reply packet and copy the returned value into the optional output pointer when provided.

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

The firmware may emit asynchronous input-change notifications while the library is waiting for a command response. [`DebugToolDevice`](/home/michael/gitroot/pico-dev/debug_tool/host/include/debug_tool_qt5/DebugToolDevice.h) handles that by queueing unrelated packets in an internal pending-event queue. Use `HasPendingEvent()`, `TakePendingEvent()`, or `WaitForEvent()` to consume those notifications.

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

Relevant protocol enums are declared in [`DebugToolDevice.h`](/home/michael/gitroot/pico-dev/debug_tool/host/include/debug_tool_qt5/DebugToolDevice.h):

- `CommandCode`
- `EventType`
- `ErrorCode`

`EventPacket` provides two helpers:

```cpp
EventType Type() const;
quint8 Info() const;
```

## Minimal library example

```cpp
#include <QDebug>
#include <debug_tool_qt5/DebugToolDevice.h>

void RunExample() {
    debug_tool_qt5::DebugToolDevice device(2000);

    if (!device.Open(QStringLiteral("/dev/ttyACM0"))) {
        qWarning().noquote() << "Open failed:" << device.LastErrorString();
        return;
    }

    if (!device.Pulse(0, 5)) {
        qWarning().noquote() << "Pulse failed:" << device.LastErrorString();
        return;
    }

    quint8 inputs = 0;
    if (!device.ReadInputs(&inputs)) {
        qWarning().noquote() << "ReadInputs failed:" << device.LastErrorString();
        return;
    }

    qInfo().noquote() << "Response:" << device.LastResponse();
    qInfo().noquote() << "Inputs:" << inputs;
}
```

## `magictool` example program

The buildable example lives at [`host/examples/basic_usage.cpp`](/home/michael/gitroot/pico-dev/debug_tool/host/examples/basic_usage.cpp). It creates a `QCoreApplication`, opens the requested serial port, dispatches one command, and prints both the decoded response string and any returned value.

Build the host library and the example:

```bash
cmake -S host -B build/host -DDEBUG_TOOL_QT5_BUILD_EXAMPLES=ON
cmake --build build/host
```

The executable is produced as:

```text
build/host/magictool
```

Supported commands from the example:

- `pulse <output> <count>`
- `set <output>`
- `clear <output>`
- `toggle <output>`
- `read-inputs`
- `read-outputs`
- `ping <value>`
- `version`

Example invocations:

```bash
./build/host/magictool /dev/ttyACM0 pulse 0 5
./build/host/magictool /dev/ttyACM0 set 1
./build/host/magictool /dev/ttyACM0 read-inputs
./build/host/magictool /dev/ttyACM0 ping 42
./build/host/magictool /dev/ttyACM0 version
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
