#pragma once

#include <QByteArray>
#include <QQueue>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QString>
#include <memory>

namespace magictool {

enum CommandCode : quint8 {
    CMD_NOP = 0x0,
    CMD_SET = 0x1,
    CMD_CLEAR = 0x2,
    CMD_TOGGLE = 0x3,
    CMD_PULSE = 0x4,
    CMD_WRITE_MASK = 0x5,
    CMD_READ_INPUTS = 0x6,
    CMD_READ_OUTPUTS = 0x7,
    CMD_ENABLE_NOTIFY = 0x8,
    CMD_DISABLE_NOTIFY = 0x9,
    CMD_GET_VERSION = 0xA,
    CMD_PING = 0xB,
    CMD_OPEN = 0xC,
    CMD_CLOSE = 0xD,
};

enum EventType : quint8 {
    EVT_INPUT_CHANGE = 0x1,
    EVT_INPUTS = 0x2,
    EVT_OUTPUTS = 0x3,
    EVT_ACK = 0xE,
    EVT_ERROR = 0xF,
};

enum ErrorCode : quint8 {
    ERR_BAD_PIN = 1,
    ERR_BAD_SELECTOR = 2,
    ERR_BAD_ARGUMENT = 3,
    ERR_QUEUE_FULL = 4,
    ERR_UNKNOWN_CMD = 5,
    ERR_LED_UNAVAILABLE = 6,
};

struct EventPacket {
    quint8 header = 0;
    quint8 arg = 0;

    EventType Type() const;
    quint8 Info() const;
};

class DebugToolDevice {
public:
    explicit DebugToolDevice(int timeoutMs = 2000);
    ~DebugToolDevice();

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
    bool Ping(quint8 value, quint8 *echoedOut = nullptr);
    bool OpenTool();
    bool CloseTool();

private:
    QSerialPort *SerialPort() const;
    QSerialPort *EnsureSerialPort();
    bool SendCommand(quint8 header, quint8 arg, EventType expectedType, quint8 expectedInfo, EventPacket *responseOut);
    bool ReadPacket(EventPacket *packetOut, int timeoutMs);
    bool WritePacket(quint8 header, quint8 arg);
    void ResetLastStatus();
    void SetErrorString(const QString &message);

    std::unique_ptr<QSerialPort> serialPort_;
    int timeoutMs_;
    QByteArray rxBuffer_;
    QQueue<EventPacket> pendingEvents_;
    EventPacket lastPacket_;
    QString lastResponse_;
    QString lastErrorString_;
};

}  // namespace magictool
