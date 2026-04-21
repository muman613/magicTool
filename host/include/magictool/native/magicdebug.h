#pragma once

#include <cstdint>
#include <deque>
#include <string>

namespace magictool {
namespace native {

enum CommandCode : std::uint8_t {
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
    CMD_GET_HARDWARE_VERSION = 0xE,
};

enum EventType : std::uint8_t {
    EVT_INPUT_CHANGE = 0x1,
    EVT_INPUTS = 0x2,
    EVT_OUTPUTS = 0x3,
    EVT_ACK = 0xE,
    EVT_ERROR = 0xF,
};

enum ErrorCode : std::uint8_t {
    ERR_BAD_PIN = 1,
    ERR_BAD_SELECTOR = 2,
    ERR_BAD_ARGUMENT = 3,
    ERR_QUEUE_FULL = 4,
    ERR_UNKNOWN_CMD = 5,
    ERR_LED_UNAVAILABLE = 6,
};

struct Version {
    std::uint8_t major = 0;
    std::uint8_t minor = 0;
    std::uint8_t revision = 0;
};

std::string FormatVersion(const Version &version);

struct EventPacket {
    std::uint8_t header = 0;
    std::uint8_t arg = 0;

    EventType Type() const;
    std::uint8_t Info() const;
};

class DebugToolDevice {
public:
    explicit DebugToolDevice(int timeoutMs = 2000);
    ~DebugToolDevice();

    static Version LibraryVersion();

    DebugToolDevice(const DebugToolDevice &) = delete;
    DebugToolDevice &operator=(const DebugToolDevice &) = delete;

    DebugToolDevice(DebugToolDevice &&other) noexcept;
    DebugToolDevice &operator=(DebugToolDevice &&other) noexcept;

    bool Open(const std::string &portName, int baudRate = 115200);
    void Close();

    bool IsOpen() const;
    const std::string &PortName() const;

    void SetTimeoutMs(int timeoutMs);
    int TimeoutMs() const;

    const std::string &LastResponse() const;
    const std::string &LastErrorString() const;
    EventPacket LastPacket() const;

    bool HasPendingEvent() const;
    bool TakePendingEvent(EventPacket *eventOut);
    bool WaitForEvent(EventPacket *eventOut, int timeoutMs = -1);

    bool Pulse(std::uint8_t outputIndex, std::uint8_t count = 1);
    bool Set(std::uint8_t outputIndex);
    bool Clear(std::uint8_t outputIndex);
    bool Toggle(std::uint8_t outputIndex);
    bool WriteMask(std::uint8_t mask);

    bool ReadInputs(std::uint8_t *bitsOut = nullptr);
    bool ReadOutputs(std::uint8_t *bitsOut = nullptr);

    bool EnableNotify(std::uint8_t inputIndex);
    bool EnableAllNotify();
    bool DisableNotify(std::uint8_t inputIndex);
    bool DisableAllNotify();

    bool GetVersion(std::uint8_t *versionOut = nullptr);
    bool GetFirmwareVersion(Version *versionOut = nullptr);
    bool GetHardwareVersion(std::uint8_t *hardwareVersionOut = nullptr);
    bool Ping(std::uint8_t value, std::uint8_t *echoedOut = nullptr);
    bool OpenTool();
    bool CloseTool();

private:
    bool SendCommand(std::uint8_t header,
                     std::uint8_t arg,
                     EventType expectedType,
                     std::uint8_t expectedInfo,
                     EventPacket *responseOut);
    bool ReadPacket(EventPacket *packetOut, int timeoutMs);
    bool WritePacket(std::uint8_t header, std::uint8_t arg);
    void ResetLastStatus();
    void SetErrorString(const std::string &message);
    void SetErrnoError(const std::string &prefix);

    int fd_;
    int timeoutMs_;
    std::string portName_;
    std::deque<std::uint8_t> rxBuffer_;
    std::deque<EventPacket> pendingEvents_;
    EventPacket lastPacket_;
    std::string lastResponse_;
    std::string lastErrorString_;
};

}  // namespace native
}  // namespace magictool
