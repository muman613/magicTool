#include "debug_tool_qt5/DebugToolDevice.h"

#include <QByteArray>
#include <QElapsedTimer>

namespace debug_tool_qt5 {

DebugToolDevice::DebugToolDevice(int timeoutMs)
    : timeoutMs_(timeoutMs) {
}

bool DebugToolDevice::Open(const QString &portName, qint32 baudRate) {
    serialPort_.close();
    lastResponse_.clear();
    lastErrorString_.clear();

    serialPort_.setPortName(portName);
    serialPort_.setBaudRate(baudRate);
    serialPort_.setDataBits(QSerialPort::Data8);
    serialPort_.setParity(QSerialPort::NoParity);
    serialPort_.setStopBits(QSerialPort::OneStop);
    serialPort_.setFlowControl(QSerialPort::NoFlowControl);

    if (!serialPort_.open(QIODevice::ReadWrite)) {
        SetErrorString(serialPort_.errorString());
        return false;
    }

    serialPort_.clear(QSerialPort::AllDirections);
    return true;
}

bool DebugToolDevice::Open(const QSerialPortInfo &portInfo, qint32 baudRate) {
    serialPort_.setPort(portInfo);
    return Open(portInfo.portName(), baudRate);
}

void DebugToolDevice::Close() {
    serialPort_.close();
}

bool DebugToolDevice::IsOpen() const {
    return serialPort_.isOpen();
}

QString DebugToolDevice::PortName() const {
    return serialPort_.portName();
}

void DebugToolDevice::SetTimeoutMs(int timeoutMs) {
    timeoutMs_ = timeoutMs;
}

int DebugToolDevice::TimeoutMs() const {
    return timeoutMs_;
}

QString DebugToolDevice::LastResponse() const {
    return lastResponse_;
}

QString DebugToolDevice::LastErrorString() const {
    return lastErrorString_;
}

bool DebugToolDevice::Pulse(int count) {
    return SendCommand(QStringLiteral("PULSE %1").arg(count));
}

bool DebugToolDevice::Set(int value) {
    return SendCommand(QStringLiteral("SET %1").arg(value ? 1 : 0));
}

bool DebugToolDevice::Clear() {
    return SendCommand(QStringLiteral("CLR"));
}

bool DebugToolDevice::Toggle() {
    return SendCommand(QStringLiteral("TOGGLE"));
}

bool DebugToolDevice::SendCommand(const QString &command) {
    lastResponse_.clear();
    lastErrorString_.clear();

    if (!serialPort_.isOpen()) {
        SetErrorString(QStringLiteral("Serial port is not open"));
        return false;
    }

    serialPort_.clear(QSerialPort::AllDirections);

    const QByteArray payload = command.toUtf8() + '\n';
    const qint64 bytesWritten = serialPort_.write(payload);
    if (bytesWritten != payload.size()) {
        SetErrorString(QStringLiteral("Failed to queue complete command"));
        return false;
    }

    if (!serialPort_.waitForBytesWritten(timeoutMs_)) {
        SetErrorString(serialPort_.errorString());
        return false;
    }

    QString response;
    if (!ReadResponse(&response)) {
        return false;
    }

    lastResponse_ = response;
    if (!response.startsWith(QStringLiteral("OK"))) {
        SetErrorString(response);
        return false;
    }

    return true;
}

bool DebugToolDevice::ReadResponse(QString *responseOut) {
    QByteArray responseBuffer;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs_) {
        const int remainingMs = timeoutMs_ - static_cast<int>(timer.elapsed());
        if (!serialPort_.waitForReadyRead(remainingMs)) {
            if (serialPort_.error() != QSerialPort::TimeoutError) {
                SetErrorString(serialPort_.errorString());
            } else {
                SetErrorString(QStringLiteral("Timed out waiting for firmware response"));
            }
            return false;
        }

        responseBuffer += serialPort_.readAll();
        const int newlineIndex = responseBuffer.indexOf('\n');
        if (newlineIndex >= 0) {
            const QByteArray line = responseBuffer.left(newlineIndex + 1).trimmed();
            *responseOut = QString::fromUtf8(line);
            return true;
        }
    }

    SetErrorString(QStringLiteral("Timed out waiting for firmware response"));
    return false;
}

void DebugToolDevice::SetErrorString(const QString &message) {
    lastErrorString_ = message;
}

}  // namespace debug_tool_qt5
