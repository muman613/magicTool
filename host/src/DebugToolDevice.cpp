#include "debug_tool_qt5/DebugToolDevice.h"

#include <QElapsedTimer>

namespace debug_tool_qt5 {
namespace {

constexpr quint8 kAllInputsSelector = 0x0F;
constexpr quint8 kOutputCount = 4;
constexpr quint8 kInputCount = 2;

quint8 HighNibble(quint8 value) {
    return static_cast<quint8>((value >> 4) & 0x0F);
}

quint8 LowNibble(quint8 value) {
    return static_cast<quint8>(value & 0x0F);
}

quint8 MakeHeader(quint8 high, quint8 low) {
    return static_cast<quint8>(((high & 0x0F) << 4) | (low & 0x0F));
}

bool IsValidOutputIndex(quint8 index) {
    return index < kOutputCount;
}

bool IsValidInputIndex(quint8 index) {
    return index < kInputCount;
}

QString CommandName(quint8 code) {
    switch (code) {
        case CMD_NOP: return QStringLiteral("NOP");
        case CMD_SET: return QStringLiteral("SET");
        case CMD_CLEAR: return QStringLiteral("CLEAR");
        case CMD_TOGGLE: return QStringLiteral("TOGGLE");
        case CMD_PULSE: return QStringLiteral("PULSE");
        case CMD_WRITE_MASK: return QStringLiteral("WRITE_MASK");
        case CMD_READ_INPUTS: return QStringLiteral("READ_INPUTS");
        case CMD_READ_OUTPUTS: return QStringLiteral("READ_OUTPUTS");
        case CMD_ENABLE_NOTIFY: return QStringLiteral("ENABLE_NOTIFY");
        case CMD_DISABLE_NOTIFY: return QStringLiteral("DISABLE_NOTIFY");
        case CMD_GET_VERSION: return QStringLiteral("GET_VERSION");
        case CMD_PING: return QStringLiteral("PING");
        default: return QStringLiteral("UNKNOWN_CMD");
    }
}

QString EventName(quint8 code) {
    switch (code) {
        case EVT_INPUT_CHANGE: return QStringLiteral("INPUT_CHANGE");
        case EVT_INPUTS: return QStringLiteral("INPUTS");
        case EVT_OUTPUTS: return QStringLiteral("OUTPUTS");
        case EVT_ACK: return QStringLiteral("ACK");
        case EVT_ERROR: return QStringLiteral("ERROR");
        default: return QStringLiteral("UNKNOWN_EVENT");
    }
}

QString ErrorName(quint8 code) {
    switch (code) {
        case ERR_BAD_PIN: return QStringLiteral("BAD_PIN");
        case ERR_BAD_SELECTOR: return QStringLiteral("BAD_SELECTOR");
        case ERR_BAD_ARGUMENT: return QStringLiteral("BAD_ARGUMENT");
        case ERR_QUEUE_FULL: return QStringLiteral("QUEUE_FULL");
        case ERR_UNKNOWN_CMD: return QStringLiteral("UNKNOWN_CMD");
        default: return QStringLiteral("UNKNOWN_ERROR");
    }
}

QString FormatPacket(const EventPacket &packet) {
    const quint8 type = packet.Type();
    const quint8 info = packet.Info();

    if (type == EVT_ACK) {
        return QStringLiteral("ACK %1 arg=%2")
            .arg(CommandName(info))
            .arg(packet.arg);
    }

    if (type == EVT_ERROR) {
        return QStringLiteral("ERROR %1 code=%2(%3)")
            .arg(CommandName(info))
            .arg(packet.arg)
            .arg(ErrorName(packet.arg));
    }

    return QStringLiteral("%1 info=%2 arg=%3")
        .arg(EventName(type))
        .arg(info)
        .arg(packet.arg);
}

QString FormatDeviceError(const EventPacket &packet) {
    return QStringLiteral("Device returned %1 for %2")
        .arg(ErrorName(packet.arg))
        .arg(CommandName(packet.Info()));
}

}  // namespace

EventType EventPacket::Type() const {
    return static_cast<EventType>(HighNibble(header));
}

quint8 EventPacket::Info() const {
    return LowNibble(header);
}

DebugToolDevice::DebugToolDevice(int timeoutMs)
    : timeoutMs_(timeoutMs) {
}

DebugToolDevice::~DebugToolDevice() {
    Close();
}

bool DebugToolDevice::Open(const QString &portName, qint32 baudRate) {
    Close();
    rxBuffer_.clear();
    pendingEvents_.clear();
    ResetLastStatus();

    QSerialPort *serialPort = EnsureSerialPort();
    serialPort->setPortName(portName);
    serialPort->setBaudRate(baudRate);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!serialPort->open(QIODevice::ReadWrite)) {
        SetErrorString(serialPort->errorString());
        return false;
    }

    serialPort->clear(QSerialPort::AllDirections);
    return true;
}

bool DebugToolDevice::Open(const QSerialPortInfo &portInfo, qint32 baudRate) {
    return Open(portInfo.portName(), baudRate);
}

void DebugToolDevice::Close() {
    if (serialPort_ && serialPort_->isOpen()) {
        serialPort_->close();
    }
}

bool DebugToolDevice::IsOpen() const {
    return serialPort_ && serialPort_->isOpen();
}

QString DebugToolDevice::PortName() const {
    return serialPort_ ? serialPort_->portName() : QString();
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

EventPacket DebugToolDevice::LastPacket() const {
    return lastPacket_;
}

bool DebugToolDevice::HasPendingEvent() const {
    return !pendingEvents_.isEmpty();
}

bool DebugToolDevice::TakePendingEvent(EventPacket *eventOut) {
    if (!eventOut || pendingEvents_.isEmpty()) {
        return false;
    }

    *eventOut = pendingEvents_.dequeue();
    return true;
}

bool DebugToolDevice::WaitForEvent(EventPacket *eventOut, int timeoutMs) {
    if (!eventOut) {
        SetErrorString(QStringLiteral("eventOut must not be null"));
        return false;
    }

    if (!pendingEvents_.isEmpty()) {
        *eventOut = pendingEvents_.dequeue();
        return true;
    }

    const int effectiveTimeout = timeoutMs >= 0 ? timeoutMs : timeoutMs_;
    return ReadPacket(eventOut, effectiveTimeout);
}

bool DebugToolDevice::Pulse(quint8 outputIndex, quint8 count) {
    if (!IsValidOutputIndex(outputIndex)) {
        SetErrorString(QStringLiteral("Invalid output index"));
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_PULSE, outputIndex), count, EVT_ACK, CMD_PULSE, &response);
}

bool DebugToolDevice::Set(quint8 outputIndex) {
    if (!IsValidOutputIndex(outputIndex)) {
        SetErrorString(QStringLiteral("Invalid output index"));
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_SET, outputIndex), 0, EVT_ACK, CMD_SET, &response);
}

bool DebugToolDevice::Clear(quint8 outputIndex) {
    if (!IsValidOutputIndex(outputIndex)) {
        SetErrorString(QStringLiteral("Invalid output index"));
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_CLEAR, outputIndex), 0, EVT_ACK, CMD_CLEAR, &response);
}

bool DebugToolDevice::Toggle(quint8 outputIndex) {
    if (!IsValidOutputIndex(outputIndex)) {
        SetErrorString(QStringLiteral("Invalid output index"));
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_TOGGLE, outputIndex), 0, EVT_ACK, CMD_TOGGLE, &response);
}

bool DebugToolDevice::WriteMask(quint8 mask) {
    EventPacket response;
    return SendCommand(MakeHeader(CMD_WRITE_MASK, 0), static_cast<quint8>(mask & 0x0F), EVT_ACK, CMD_WRITE_MASK, &response);
}

bool DebugToolDevice::ReadInputs(quint8 *bitsOut) {
    EventPacket response;
    if (!SendCommand(MakeHeader(CMD_READ_INPUTS, 0), 0, EVT_INPUTS, 0, &response)) {
        return false;
    }

    if (bitsOut) {
        *bitsOut = static_cast<quint8>(response.arg & 0x03);
    }
    return true;
}

bool DebugToolDevice::ReadOutputs(quint8 *bitsOut) {
    EventPacket response;
    if (!SendCommand(MakeHeader(CMD_READ_OUTPUTS, 0), 0, EVT_OUTPUTS, 0, &response)) {
        return false;
    }

    if (bitsOut) {
        *bitsOut = static_cast<quint8>(response.arg & 0x0F);
    }
    return true;
}

bool DebugToolDevice::EnableNotify(quint8 inputIndex) {
    if (!IsValidInputIndex(inputIndex)) {
        SetErrorString(QStringLiteral("Invalid input index"));
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_ENABLE_NOTIFY, inputIndex), 0, EVT_ACK, CMD_ENABLE_NOTIFY, &response);
}

bool DebugToolDevice::EnableAllNotify() {
    EventPacket response;
    return SendCommand(MakeHeader(CMD_ENABLE_NOTIFY, kAllInputsSelector), 0, EVT_ACK, CMD_ENABLE_NOTIFY, &response);
}

bool DebugToolDevice::DisableNotify(quint8 inputIndex) {
    if (!IsValidInputIndex(inputIndex)) {
        SetErrorString(QStringLiteral("Invalid input index"));
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_DISABLE_NOTIFY, inputIndex), 0, EVT_ACK, CMD_DISABLE_NOTIFY, &response);
}

bool DebugToolDevice::DisableAllNotify() {
    EventPacket response;
    return SendCommand(MakeHeader(CMD_DISABLE_NOTIFY, kAllInputsSelector), 0, EVT_ACK, CMD_DISABLE_NOTIFY, &response);
}

bool DebugToolDevice::GetVersion(quint8 *versionOut) {
    EventPacket response;
    if (!SendCommand(MakeHeader(CMD_GET_VERSION, 0), 0, EVT_ACK, CMD_GET_VERSION, &response)) {
        return false;
    }

    if (versionOut) {
        *versionOut = response.arg;
    }
    return true;
}

bool DebugToolDevice::Ping(quint8 value, quint8 *echoedOut) {
    EventPacket response;
    if (!SendCommand(MakeHeader(CMD_PING, 0), value, EVT_ACK, CMD_PING, &response)) {
        return false;
    }

    if (echoedOut) {
        *echoedOut = response.arg;
    }
    return true;
}

bool DebugToolDevice::SendCommand(quint8 header, quint8 arg, EventType expectedType, quint8 expectedInfo, EventPacket *responseOut) {
    ResetLastStatus();

    if (!IsOpen()) {
        SetErrorString(QStringLiteral("Serial port is not open"));
        return false;
    }

    if (!WritePacket(header, arg)) {
        return false;
    }

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs_) {
        const int remainingMs = timeoutMs_ - static_cast<int>(timer.elapsed());
        EventPacket packet;
        if (!ReadPacket(&packet, remainingMs)) {
            return false;
        }

        const quint8 type = packet.Type();
        const quint8 info = packet.Info();
        if (type == EVT_ERROR && info == HighNibble(header)) {
            lastPacket_ = packet;
            lastResponse_ = FormatPacket(packet);
            SetErrorString(FormatDeviceError(packet));
            return false;
        }

        if (type == expectedType && info == expectedInfo) {
            lastPacket_ = packet;
            lastResponse_ = FormatPacket(packet);
            if (responseOut) {
                *responseOut = packet;
            }
            return true;
        }

        pendingEvents_.enqueue(packet);
    }

    SetErrorString(QStringLiteral("Timed out waiting for firmware response"));
    return false;
}

bool DebugToolDevice::ReadPacket(EventPacket *packetOut, int timeoutMs) {
    if (!packetOut) {
        SetErrorString(QStringLiteral("packetOut must not be null"));
        return false;
    }

    QSerialPort *serialPort = SerialPort();
    if (!serialPort || !serialPort->isOpen()) {
        SetErrorString(QStringLiteral("Serial port is not open"));
        return false;
    }

    QElapsedTimer timer;
    timer.start();

    while (rxBuffer_.size() < 2) {
        const int remainingMs = timeoutMs - static_cast<int>(timer.elapsed());
        if (remainingMs <= 0) {
            SetErrorString(QStringLiteral("Timed out waiting for firmware response"));
            return false;
        }

        if (!serialPort->waitForReadyRead(remainingMs)) {
            if (serialPort->error() != QSerialPort::TimeoutError) {
                SetErrorString(serialPort->errorString());
            } else {
                SetErrorString(QStringLiteral("Timed out waiting for firmware response"));
            }
            return false;
        }

        rxBuffer_ += serialPort->readAll();
    }

    packetOut->header = static_cast<quint8>(rxBuffer_.at(0));
    packetOut->arg = static_cast<quint8>(rxBuffer_.at(1));
    rxBuffer_.remove(0, 2);
    return true;
}

bool DebugToolDevice::WritePacket(quint8 header, quint8 arg) {
    QSerialPort *serialPort = SerialPort();
    if (!serialPort || !serialPort->isOpen()) {
        SetErrorString(QStringLiteral("Serial port is not open"));
        return false;
    }

    const char packet[2] = {
        static_cast<char>(header),
        static_cast<char>(arg),
    };

    const qint64 bytesWritten = serialPort->write(packet, sizeof(packet));
    if (bytesWritten != sizeof(packet)) {
        SetErrorString(QStringLiteral("Failed to queue complete command packet"));
        return false;
    }

    if (!serialPort->waitForBytesWritten(timeoutMs_)) {
        SetErrorString(serialPort->errorString());
        return false;
    }

    return true;
}

QSerialPort *DebugToolDevice::SerialPort() const {
    return serialPort_.get();
}

QSerialPort *DebugToolDevice::EnsureSerialPort() {
    if (!serialPort_) {
        serialPort_ = std::make_unique<QSerialPort>();
    }
    return serialPort_.get();
}

void DebugToolDevice::ResetLastStatus() {
    lastPacket_ = EventPacket{};
    lastResponse_.clear();
    lastErrorString_.clear();
}

void DebugToolDevice::SetErrorString(const QString &message) {
    lastErrorString_ = message;
}

}  // namespace debug_tool_qt5
