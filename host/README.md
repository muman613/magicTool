# debug_tool_qt5

`debug_tool_qt5` is a small Qt5 library for sending synchronous serial commands to the `debug_tool` Pico firmware. It wraps `QSerialPort` with a narrow API that matches the firmware command set and keeps the most recent response and error string available to the caller.

## What it does

The library talks to the Pico over its USB CDC serial port and sends newline-terminated ASCII commands. Each command blocks until either:

- the firmware returns a newline-terminated response
- the configured timeout expires
- the serial port reports an error

The current firmware protocol supports these commands:

- `PULSE <n>`
- `SET <0|1>`
- `CLR`
- `TOGGLE`

Successful firmware responses are expected to begin with `OK`. Any other response is treated as a command failure and is copied into `LastErrorString()`.

## Public API

Public header:

```cpp
#include <debug_tool_qt5/DebugToolDevice.h>
```

Namespace:

```cpp
debug_tool_qt5
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

    bool Pulse(int count);
    bool Set(int value);
    bool Clear();
    bool Toggle();
};
```

Behavior notes:

- `Open()` resets the previous response and error state before configuring the serial port.
- `Open()` always configures `8N1` with no flow control.
- `Set(int value)` sends `SET 1` for any non-zero value and `SET 0` for zero.
- `LastResponse()` contains the raw single-line response returned by the firmware for the most recent command.
- `LastErrorString()` contains either a transport error, a timeout message, or a non-`OK` firmware response.
- The API is synchronous and not thread-safe by design.

## Build and link

The host library can be built on its own:

```bash
cmake -S host -B build/host
cmake --build build/host
```

This produces a static library:

```text
build/host/libdebug_tool_qt5.a
```

Required dependencies:

- CMake 3.16 or newer
- Qt5 Core
- Qt5 SerialPort
- A C++17 compiler

### CMake integration

If another project vendors this repository, it can add the `host/` directory directly and link the library target:

```cmake
add_subdirectory(path/to/debug_tool/host)

target_link_libraries(my_app
    PRIVATE
        debug_tool_qt5
)
```

An alias target is also provided:

```cmake
target_link_libraries(my_app
    PRIVATE
        debug_tool::qt5
)
```

This project does not currently install or export a package config, so `find_package(debug_tool_qt5)` is not available.

## Example usage

Minimal usage:

```cpp
#include <QDebug>
#include <debug_tool_qt5/DebugToolDevice.h>

debug_tool_qt5::DebugToolDevice device(2000);

if (!device.Open(QStringLiteral("/dev/ttyACM0"))) {
    qWarning() << "Open failed:" << device.LastErrorString();
    return;
}

if (!device.Pulse(5)) {
    qWarning() << "Pulse failed:" << device.LastErrorString();
    return;
}

qDebug() << "Firmware response:" << device.LastResponse();
device.Close();
```

A buildable example program is included at:

- `host/examples/basic_usage.cpp`

To build it:

```bash
cmake -S host -B build/host -DDEBUG_TOOL_QT5_BUILD_EXAMPLES=ON
cmake --build build/host --target debug_tool_qt5_basic_example
```

Example invocation:

```bash
./build/host/debug_tool_qt5_basic_example /dev/ttyACM0 pulse 5
./build/host/debug_tool_qt5_basic_example /dev/ttyACM0 set 1
./build/host/debug_tool_qt5_basic_example /dev/ttyACM0 clear
./build/host/debug_tool_qt5_basic_example /dev/ttyACM0 toggle
```

## Selecting the serial port

Typical device names:

- Linux: `/dev/ttyACM0`
- macOS: `/dev/cu.usbmodem...`
- Windows: `COM3`

If you want to enumerate candidate ports in Qt before opening one:

```cpp
for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts()) {
    qDebug() << port.portName() << port.description();
}
```

Then open either by port name or by `QSerialPortInfo`.

## Failure modes

Common reasons a call returns `false`:

- the port could not be opened
- the device is not connected
- the firmware did not answer before the timeout
- the firmware returned an error line or a response not starting with `OK`

For all of these cases, inspect `LastErrorString()` first.
