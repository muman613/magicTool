#include "magictool/native/magicdebug.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <utility>

namespace magictool {
namespace native {
namespace {

constexpr std::uint8_t kAllInputsSelector = 0x0F;
constexpr std::uint8_t kOutputCount = 4;
constexpr std::uint8_t kInputCount = 2;
constexpr std::uint8_t kVersionMajorSelector = 0;
constexpr std::uint8_t kVersionMinorSelector = 1;
constexpr std::uint8_t kVersionRevisionSelector = 2;
constexpr int kInvalidFd = -1;

#ifndef MAGICTOOL_LIBRARY_VERSION_MAJOR
#define MAGICTOOL_LIBRARY_VERSION_MAJOR 0
#endif

#ifndef MAGICTOOL_LIBRARY_VERSION_MINOR
#define MAGICTOOL_LIBRARY_VERSION_MINOR 1
#endif

#ifndef MAGICTOOL_LIBRARY_VERSION_REVISION
#define MAGICTOOL_LIBRARY_VERSION_REVISION 0
#endif

std::uint8_t HighNibble(std::uint8_t value) {
    return static_cast<std::uint8_t>((value >> 4) & 0x0F);
}

std::uint8_t LowNibble(std::uint8_t value) {
    return static_cast<std::uint8_t>(value & 0x0F);
}

std::uint8_t MakeHeader(std::uint8_t high, std::uint8_t low) {
    return static_cast<std::uint8_t>(((high & 0x0F) << 4) | (low & 0x0F));
}

bool IsValidOutputIndex(std::uint8_t index) {
    return index < kOutputCount;
}

bool IsValidInputIndex(std::uint8_t index) {
    return index < kInputCount;
}

const char *CommandName(std::uint8_t code) {
    switch (code) {
        case CMD_NOP: return "NOP";
        case CMD_SET: return "SET";
        case CMD_CLEAR: return "CLEAR";
        case CMD_TOGGLE: return "TOGGLE";
        case CMD_PULSE: return "PULSE";
        case CMD_WRITE_MASK: return "WRITE_MASK";
        case CMD_READ_INPUTS: return "READ_INPUTS";
        case CMD_READ_OUTPUTS: return "READ_OUTPUTS";
        case CMD_ENABLE_NOTIFY: return "ENABLE_NOTIFY";
        case CMD_DISABLE_NOTIFY: return "DISABLE_NOTIFY";
        case CMD_GET_VERSION: return "GET_VERSION";
        case CMD_PING: return "PING";
        case CMD_OPEN: return "OPEN";
        case CMD_CLOSE: return "CLOSE";
        case CMD_GET_HARDWARE_VERSION: return "GET_HARDWARE_VERSION";
        default: return "UNKNOWN_CMD";
    }
}

const char *EventName(std::uint8_t code) {
    switch (code) {
        case EVT_INPUT_CHANGE: return "INPUT_CHANGE";
        case EVT_INPUTS: return "INPUTS";
        case EVT_OUTPUTS: return "OUTPUTS";
        case EVT_ACK: return "ACK";
        case EVT_ERROR: return "ERROR";
        default: return "UNKNOWN_EVENT";
    }
}

const char *ErrorName(std::uint8_t code) {
    switch (code) {
        case ERR_BAD_PIN: return "BAD_PIN";
        case ERR_BAD_SELECTOR: return "BAD_SELECTOR";
        case ERR_BAD_ARGUMENT: return "BAD_ARGUMENT";
        case ERR_QUEUE_FULL: return "QUEUE_FULL";
        case ERR_UNKNOWN_CMD: return "UNKNOWN_CMD";
        case ERR_LED_UNAVAILABLE: return "LED_UNAVAILABLE";
        default: return "UNKNOWN_ERROR";
    }
}

std::string FormatPacket(const EventPacket &packet) {
    const std::uint8_t type = packet.Type();
    const std::uint8_t info = packet.Info();
    std::ostringstream stream;

    if (type == EVT_ACK) {
        stream << "ACK " << CommandName(info) << " arg=" << static_cast<unsigned>(packet.arg);
        return stream.str();
    }

    if (type == EVT_ERROR) {
        stream << "ERROR " << CommandName(info)
               << " code=" << static_cast<unsigned>(packet.arg)
               << "(" << ErrorName(packet.arg) << ")";
        return stream.str();
    }

    stream << EventName(type)
           << " info=" << static_cast<unsigned>(info)
           << " arg=" << static_cast<unsigned>(packet.arg);
    return stream.str();
}

std::string FormatDeviceError(const EventPacket &packet) {
    std::ostringstream stream;
    stream << "Device returned " << ErrorName(packet.arg) << " for " << CommandName(packet.Info());
    return stream.str();
}

bool BaudRateToSpeed(int baudRate, speed_t *speedOut) {
    if (!speedOut) {
        return false;
    }

    switch (baudRate) {
        case 9600: *speedOut = B9600; return true;
        case 19200: *speedOut = B19200; return true;
        case 38400: *speedOut = B38400; return true;
        case 57600: *speedOut = B57600; return true;
        case 115200: *speedOut = B115200; return true;
        case 230400: *speedOut = B230400; return true;
        default: return false;
    }
}

std::string ErrnoMessage(const std::string &prefix) {
    std::ostringstream stream;
    stream << prefix << ": " << std::strerror(errno);
    return stream.str();
}

int ElapsedMs(std::chrono::steady_clock::time_point start) {
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

}  // namespace

std::string FormatVersion(const Version &version) {
    std::ostringstream stream;
    stream << static_cast<unsigned>(version.major)
           << '.' << static_cast<unsigned>(version.minor)
           << '.' << static_cast<unsigned>(version.revision);
    return stream.str();
}

EventType EventPacket::Type() const {
    return static_cast<EventType>(HighNibble(header));
}

std::uint8_t EventPacket::Info() const {
    return LowNibble(header);
}

DebugToolDevice::DebugToolDevice(int timeoutMs)
    : fd_(kInvalidFd),
      timeoutMs_(timeoutMs) {
}

DebugToolDevice::~DebugToolDevice() {
    Close();
}

Version DebugToolDevice::LibraryVersion() {
    return Version{
        static_cast<std::uint8_t>(MAGICTOOL_LIBRARY_VERSION_MAJOR),
        static_cast<std::uint8_t>(MAGICTOOL_LIBRARY_VERSION_MINOR),
        static_cast<std::uint8_t>(MAGICTOOL_LIBRARY_VERSION_REVISION),
    };
}

DebugToolDevice::DebugToolDevice(DebugToolDevice &&other) noexcept
    : fd_(other.fd_),
      timeoutMs_(other.timeoutMs_),
      portName_(std::move(other.portName_)),
      rxBuffer_(std::move(other.rxBuffer_)),
      pendingEvents_(std::move(other.pendingEvents_)),
      lastPacket_(other.lastPacket_),
      lastResponse_(std::move(other.lastResponse_)),
      lastErrorString_(std::move(other.lastErrorString_)) {
    other.fd_ = kInvalidFd;
}

DebugToolDevice &DebugToolDevice::operator=(DebugToolDevice &&other) noexcept {
    if (this != &other) {
        Close();
        fd_ = other.fd_;
        timeoutMs_ = other.timeoutMs_;
        portName_ = std::move(other.portName_);
        rxBuffer_ = std::move(other.rxBuffer_);
        pendingEvents_ = std::move(other.pendingEvents_);
        lastPacket_ = other.lastPacket_;
        lastResponse_ = std::move(other.lastResponse_);
        lastErrorString_ = std::move(other.lastErrorString_);
        other.fd_ = kInvalidFd;
    }
    return *this;
}

bool DebugToolDevice::Open(const std::string &portName, int baudRate) {
    Close();
    rxBuffer_.clear();
    pendingEvents_.clear();
    ResetLastStatus();

    speed_t speed = B115200;
    if (!BaudRateToSpeed(baudRate, &speed)) {
        SetErrorString("Unsupported baud rate");
        return false;
    }

    const int fd = open(portName.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0) {
        SetErrnoError("Failed to open " + portName);
        return false;
    }

    if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {
        const std::string message = ErrnoMessage("fcntl failed");
        close(fd);
        SetErrorString(message);
        return false;
    }

    termios options{};
    if (tcgetattr(fd, &options) != 0) {
        const std::string message = ErrnoMessage("tcgetattr failed");
        close(fd);
        SetErrorString(message);
        return false;
    }

    options.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
    options.c_oflag &= static_cast<tcflag_t>(~OPOST);
    options.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN | ISIG));
    options.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    options.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    options.c_cflag |= CS8;
    options.c_cflag &= static_cast<tcflag_t>(~PARENB);
    options.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
#ifdef CRTSCTS
    options.c_cflag &= static_cast<tcflag_t>(~CRTSCTS);
#endif
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;

    if (cfsetispeed(&options, speed) != 0 || cfsetospeed(&options, speed) != 0) {
        const std::string message = ErrnoMessage("Failed to set baud rate");
        close(fd);
        SetErrorString(message);
        return false;
    }

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        const std::string message = ErrnoMessage("tcsetattr failed");
        close(fd);
        SetErrorString(message);
        return false;
    }

    tcflush(fd, TCIOFLUSH);
    fd_ = fd;
    portName_ = portName;

    Version firmwareVersion;
    if (!GetFirmwareVersion(&firmwareVersion)) {
        const std::string message = lastErrorString_;
        Close();
        SetErrorString(message);
        return false;
    }

    const Version libraryVersion = LibraryVersion();
    if (firmwareVersion.major != libraryVersion.major ||
        firmwareVersion.minor != libraryVersion.minor) {
        SetErrorString("Firmware version " + FormatVersion(firmwareVersion) +
                       " is incompatible with library version " + FormatVersion(libraryVersion));
        Close();
        return false;
    }

    return true;
}

void DebugToolDevice::Close() {
    if (fd_ != kInvalidFd) {
        close(fd_);
        fd_ = kInvalidFd;
    }
}

bool DebugToolDevice::IsOpen() const {
    return fd_ != kInvalidFd;
}

const std::string &DebugToolDevice::PortName() const {
    return portName_;
}

void DebugToolDevice::SetTimeoutMs(int timeoutMs) {
    timeoutMs_ = timeoutMs;
}

int DebugToolDevice::TimeoutMs() const {
    return timeoutMs_;
}

const std::string &DebugToolDevice::LastResponse() const {
    return lastResponse_;
}

const std::string &DebugToolDevice::LastErrorString() const {
    return lastErrorString_;
}

EventPacket DebugToolDevice::LastPacket() const {
    return lastPacket_;
}

bool DebugToolDevice::HasPendingEvent() const {
    return !pendingEvents_.empty();
}

bool DebugToolDevice::TakePendingEvent(EventPacket *eventOut) {
    if (!eventOut || pendingEvents_.empty()) {
        return false;
    }

    *eventOut = pendingEvents_.front();
    pendingEvents_.pop_front();
    return true;
}

bool DebugToolDevice::WaitForEvent(EventPacket *eventOut, int timeoutMs) {
    if (!eventOut) {
        SetErrorString("eventOut must not be null");
        return false;
    }

    if (!pendingEvents_.empty()) {
        *eventOut = pendingEvents_.front();
        pendingEvents_.pop_front();
        return true;
    }

    const int effectiveTimeout = timeoutMs >= 0 ? timeoutMs : timeoutMs_;
    return ReadPacket(eventOut, effectiveTimeout);
}

bool DebugToolDevice::Pulse(std::uint8_t outputIndex, std::uint8_t count) {
    if (!IsValidOutputIndex(outputIndex)) {
        SetErrorString("Invalid output index");
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_PULSE, outputIndex), count, EVT_ACK, CMD_PULSE, &response);
}

bool DebugToolDevice::Set(std::uint8_t outputIndex) {
    if (!IsValidOutputIndex(outputIndex)) {
        SetErrorString("Invalid output index");
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_SET, outputIndex), 0, EVT_ACK, CMD_SET, &response);
}

bool DebugToolDevice::Clear(std::uint8_t outputIndex) {
    if (!IsValidOutputIndex(outputIndex)) {
        SetErrorString("Invalid output index");
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_CLEAR, outputIndex), 0, EVT_ACK, CMD_CLEAR, &response);
}

bool DebugToolDevice::Toggle(std::uint8_t outputIndex) {
    if (!IsValidOutputIndex(outputIndex)) {
        SetErrorString("Invalid output index");
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_TOGGLE, outputIndex), 0, EVT_ACK, CMD_TOGGLE, &response);
}

bool DebugToolDevice::WriteMask(std::uint8_t mask) {
    EventPacket response;
    return SendCommand(MakeHeader(CMD_WRITE_MASK, 0),
                       static_cast<std::uint8_t>(mask & 0x0F),
                       EVT_ACK,
                       CMD_WRITE_MASK,
                       &response);
}

bool DebugToolDevice::ReadInputs(std::uint8_t *bitsOut) {
    EventPacket response;
    if (!SendCommand(MakeHeader(CMD_READ_INPUTS, 0), 0, EVT_INPUTS, 0, &response)) {
        return false;
    }

    if (bitsOut) {
        *bitsOut = static_cast<std::uint8_t>(response.arg & 0x03);
    }
    return true;
}

bool DebugToolDevice::ReadOutputs(std::uint8_t *bitsOut) {
    EventPacket response;
    if (!SendCommand(MakeHeader(CMD_READ_OUTPUTS, 0), 0, EVT_OUTPUTS, 0, &response)) {
        return false;
    }

    if (bitsOut) {
        *bitsOut = static_cast<std::uint8_t>(response.arg & 0x0F);
    }
    return true;
}

bool DebugToolDevice::EnableNotify(std::uint8_t inputIndex) {
    if (!IsValidInputIndex(inputIndex)) {
        SetErrorString("Invalid input index");
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_ENABLE_NOTIFY, inputIndex), 0, EVT_ACK, CMD_ENABLE_NOTIFY, &response);
}

bool DebugToolDevice::EnableAllNotify() {
    EventPacket response;
    return SendCommand(MakeHeader(CMD_ENABLE_NOTIFY, kAllInputsSelector), 0, EVT_ACK, CMD_ENABLE_NOTIFY, &response);
}

bool DebugToolDevice::DisableNotify(std::uint8_t inputIndex) {
    if (!IsValidInputIndex(inputIndex)) {
        SetErrorString("Invalid input index");
        return false;
    }

    EventPacket response;
    return SendCommand(MakeHeader(CMD_DISABLE_NOTIFY, inputIndex), 0, EVT_ACK, CMD_DISABLE_NOTIFY, &response);
}

bool DebugToolDevice::DisableAllNotify() {
    EventPacket response;
    return SendCommand(MakeHeader(CMD_DISABLE_NOTIFY, kAllInputsSelector), 0, EVT_ACK, CMD_DISABLE_NOTIFY, &response);
}

bool DebugToolDevice::GetVersion(std::uint8_t *versionOut) {
    EventPacket response;
    if (!SendCommand(MakeHeader(CMD_GET_VERSION, kVersionMajorSelector), 0, EVT_ACK, CMD_GET_VERSION, &response)) {
        return false;
    }

    if (versionOut) {
        *versionOut = response.arg;
    }
    return true;
}

bool DebugToolDevice::GetFirmwareVersion(Version *versionOut) {
    Version version;
    EventPacket response;

    if (!SendCommand(MakeHeader(CMD_GET_VERSION, kVersionMajorSelector), 0, EVT_ACK, CMD_GET_VERSION, &response)) {
        return false;
    }
    version.major = response.arg;

    if (!SendCommand(MakeHeader(CMD_GET_VERSION, kVersionMinorSelector), 0, EVT_ACK, CMD_GET_VERSION, &response)) {
        return false;
    }
    version.minor = response.arg;

    if (!SendCommand(MakeHeader(CMD_GET_VERSION, kVersionRevisionSelector), 0, EVT_ACK, CMD_GET_VERSION, &response)) {
        return false;
    }
    version.revision = response.arg;

    lastResponse_ = "Firmware version " + FormatVersion(version);
    if (versionOut) {
        *versionOut = version;
    }
    return true;
}

bool DebugToolDevice::GetHardwareVersion(std::uint8_t *hardwareVersionOut) {
    EventPacket response;
    if (!SendCommand(MakeHeader(CMD_GET_HARDWARE_VERSION, 0), 0, EVT_ACK, CMD_GET_HARDWARE_VERSION, &response)) {
        return false;
    }

    if (hardwareVersionOut) {
        *hardwareVersionOut = response.arg;
    }
    return true;
}

bool DebugToolDevice::Ping(std::uint8_t value, std::uint8_t *echoedOut) {
    EventPacket response;
    if (!SendCommand(MakeHeader(CMD_PING, 0), value, EVT_ACK, CMD_PING, &response)) {
        return false;
    }

    if (echoedOut) {
        *echoedOut = response.arg;
    }
    return true;
}

bool DebugToolDevice::OpenTool() {
    EventPacket response;
    return SendCommand(MakeHeader(CMD_OPEN, 0), 0, EVT_ACK, CMD_OPEN, &response);
}

bool DebugToolDevice::CloseTool() {
    EventPacket response;
    return SendCommand(MakeHeader(CMD_CLOSE, 0), 0, EVT_ACK, CMD_CLOSE, &response);
}

bool DebugToolDevice::SendCommand(std::uint8_t header,
                                  std::uint8_t arg,
                                  EventType expectedType,
                                  std::uint8_t expectedInfo,
                                  EventPacket *responseOut) {
    ResetLastStatus();

    if (!IsOpen()) {
        SetErrorString("Serial port is not open");
        return false;
    }

    if (!WritePacket(header, arg)) {
        return false;
    }

    const auto start = std::chrono::steady_clock::now();
    while (ElapsedMs(start) < timeoutMs_) {
        const int remainingMs = timeoutMs_ - ElapsedMs(start);
        EventPacket packet;
        if (!ReadPacket(&packet, remainingMs)) {
            return false;
        }

        const std::uint8_t type = packet.Type();
        const std::uint8_t info = packet.Info();
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

        pendingEvents_.push_back(packet);
    }

    SetErrorString("Timed out waiting for firmware response");
    return false;
}

bool DebugToolDevice::ReadPacket(EventPacket *packetOut, int timeoutMs) {
    if (!packetOut) {
        SetErrorString("packetOut must not be null");
        return false;
    }

    if (!IsOpen()) {
        SetErrorString("Serial port is not open");
        return false;
    }

    const auto start = std::chrono::steady_clock::now();
    while (rxBuffer_.size() < 2) {
        const int remainingMs = timeoutMs - ElapsedMs(start);
        if (remainingMs <= 0) {
            SetErrorString("Timed out waiting for firmware response");
            return false;
        }

        pollfd pfd{};
        pfd.fd = fd_;
        pfd.events = POLLIN;

        const int pollResult = poll(&pfd, 1, remainingMs);
        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }
            SetErrnoError("poll failed");
            return false;
        }

        if (pollResult == 0) {
            SetErrorString("Timed out waiting for firmware response");
            return false;
        }

        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            SetErrorString("Serial port error while reading");
            return false;
        }

        std::uint8_t buffer[32]{};
        const ssize_t bytesRead = read(fd_, buffer, sizeof(buffer));
        if (bytesRead < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            SetErrnoError("read failed");
            return false;
        }

        if (bytesRead == 0) {
            continue;
        }

        for (ssize_t i = 0; i < bytesRead; ++i) {
            rxBuffer_.push_back(buffer[i]);
        }
    }

    packetOut->header = rxBuffer_.front();
    rxBuffer_.pop_front();
    packetOut->arg = rxBuffer_.front();
    rxBuffer_.pop_front();
    return true;
}

bool DebugToolDevice::WritePacket(std::uint8_t header, std::uint8_t arg) {
    if (!IsOpen()) {
        SetErrorString("Serial port is not open");
        return false;
    }

    const std::uint8_t packet[2] = {header, arg};
    std::size_t totalWritten = 0;
    const auto start = std::chrono::steady_clock::now();

    while (totalWritten < sizeof(packet)) {
        const int remainingMs = timeoutMs_ - ElapsedMs(start);
        if (remainingMs <= 0) {
            SetErrorString("Timed out writing command packet");
            return false;
        }

        pollfd pfd{};
        pfd.fd = fd_;
        pfd.events = POLLOUT;

        const int pollResult = poll(&pfd, 1, remainingMs);
        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }
            SetErrnoError("poll failed");
            return false;
        }

        if (pollResult == 0) {
            SetErrorString("Timed out writing command packet");
            return false;
        }

        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            SetErrorString("Serial port error while writing");
            return false;
        }

        const ssize_t bytesWritten = write(fd_, packet + totalWritten, sizeof(packet) - totalWritten);
        if (bytesWritten < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            SetErrnoError("write failed");
            return false;
        }

        totalWritten += static_cast<std::size_t>(bytesWritten);
    }

    tcdrain(fd_);
    return true;
}

void DebugToolDevice::ResetLastStatus() {
    lastPacket_ = EventPacket{};
    lastResponse_.clear();
    lastErrorString_.clear();
}

void DebugToolDevice::SetErrorString(const std::string &message) {
    lastErrorString_ = message;
}

void DebugToolDevice::SetErrnoError(const std::string &prefix) {
    SetErrorString(ErrnoMessage(prefix));
}

}  // namespace native
}  // namespace magictool
