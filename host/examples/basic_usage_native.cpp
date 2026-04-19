#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "magictool/native/magicdebug.h"

namespace {

int PrintUsage(const std::string &programName) {
    std::cout
        << "Usage:\n"
        << "  " << programName << " <port> pulse <output> <count>\n"
        << "  " << programName << " <port> set <output>\n"
        << "  " << programName << " <port> clear <output>\n"
        << "  " << programName << " <port> toggle <output>\n"
        << "  " << programName << " <port> read-inputs\n"
        << "  " << programName << " <port> read-outputs\n"
        << "  " << programName << " <port> ping <value>\n"
        << "  " << programName << " <port> version\n"
        << "  " << programName << " <port> hardware\n"
        << "  " << programName << " <port> open\n"
        << "  " << programName << " <port> close\n";
    return 1;
}

std::uint8_t ParseByte(const char *value) {
    return static_cast<std::uint8_t>(std::strtoul(value, nullptr, 0));
}

const char *HardwareTypeName(std::uint8_t hardwareType) {
    switch (hardwareType) {
        case 0x1: return "pico2";
        case 0x2: return "pico2_w";
        default: return "unknown";
    }
}

std::string HardwareVersionName(std::uint8_t hardwareVersion) {
    if (hardwareVersion == 0) {
        return "unknown";
    }
    return "v" + std::to_string(hardwareVersion);
}

}  // namespace

int main(int argc, char *argv[]) {
    const std::string programName = argc > 0 ? argv[0] : "magictool_native";
    if (argc < 3) {
        return PrintUsage(programName);
    }

    const std::string portName = argv[1];
    const std::string command = argv[2];

    magictool::native::DebugToolDevice device;
    if (!device.Open(portName)) {
        std::cerr << "Failed to open " << portName << ": " << device.LastErrorString() << '\n';
        return 2;
    }

    bool ok = false;
    std::uint8_t value = 0;

    if (command == "pulse") {
        if (argc != 5) {
            return PrintUsage(programName);
        }
        ok = device.Pulse(ParseByte(argv[3]), ParseByte(argv[4]));
    } else if (command == "set") {
        if (argc != 4) {
            return PrintUsage(programName);
        }
        ok = device.Set(ParseByte(argv[3]));
    } else if (command == "clear") {
        if (argc != 4) {
            return PrintUsage(programName);
        }
        ok = device.Clear(ParseByte(argv[3]));
    } else if (command == "toggle") {
        if (argc != 4) {
            return PrintUsage(programName);
        }
        ok = device.Toggle(ParseByte(argv[3]));
    } else if (command == "read-inputs") {
        if (argc != 3) {
            return PrintUsage(programName);
        }
        ok = device.ReadInputs(&value);
    } else if (command == "read-outputs") {
        if (argc != 3) {
            return PrintUsage(programName);
        }
        ok = device.ReadOutputs(&value);
    } else if (command == "ping") {
        if (argc != 4) {
            return PrintUsage(programName);
        }
        ok = device.Ping(ParseByte(argv[3]), &value);
    } else if (command == "version") {
        if (argc != 3) {
            return PrintUsage(programName);
        }
        ok = device.GetVersion(&value);
    } else if (command == "hardware") {
        if (argc != 3) {
            return PrintUsage(programName);
        }
        ok = device.GetHardwareVersion(&value);
    } else if (command == "open") {
        if (argc != 3) {
            return PrintUsage(programName);
        }
        ok = device.OpenTool();
    } else if (command == "close") {
        if (argc != 3) {
            return PrintUsage(programName);
        }
        ok = device.CloseTool();
    } else {
        return PrintUsage(programName);
    }

    if (!ok) {
        std::cerr << "Command failed: " << device.LastErrorString() << '\n';
        return 3;
    }

    std::cout << "Response: " << device.LastResponse() << '\n';
    if (command == "read-inputs" ||
        command == "read-outputs" ||
        command == "ping" ||
        command == "version" ||
        command == "hardware") {
        std::cout << "Value: " << static_cast<unsigned>(value) << '\n';
        if (command == "hardware") {
            const auto hardwareType = static_cast<std::uint8_t>((value >> 4) & 0x0F);
            const auto hardwareVersion = static_cast<std::uint8_t>(value & 0x0F);
            std::cout << "Hardware: "
                      << HardwareTypeName(hardwareType)
                      << ' '
                      << HardwareVersionName(hardwareVersion)
                      << '\n';
        }
    }

    return 0;
}
