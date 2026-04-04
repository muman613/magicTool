#pragma once

#include <QString>
#include <QSerialPort>
#include <QSerialPortInfo>

namespace debug_tool_qt5 {

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

private:
    bool SendCommand(const QString &command);
    bool ReadResponse(QString *responseOut);
    void SetErrorString(const QString &message);

    QSerialPort serialPort_;
    int timeoutMs_;
    QString lastResponse_;
    QString lastErrorString_;
};

}  // namespace debug_tool_qt5
