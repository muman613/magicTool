#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "magictool/device_config.h"
#include "magictool/native/magicdebug.h"

namespace {

#ifndef MAGICTOOL_HOST_EXAMPLE_VERSION_MAJOR
#define MAGICTOOL_HOST_EXAMPLE_VERSION_MAJOR 0
#endif

#ifndef MAGICTOOL_HOST_EXAMPLE_VERSION_MINOR
#define MAGICTOOL_HOST_EXAMPLE_VERSION_MINOR 1
#endif

#ifndef MAGICTOOL_HOST_EXAMPLE_VERSION_REVISION
#define MAGICTOOL_HOST_EXAMPLE_VERSION_REVISION 0
#endif

magictool::native::Version HostExampleVersion() {
    return magictool::native::Version{
        static_cast<std::uint8_t>(MAGICTOOL_HOST_EXAMPLE_VERSION_MAJOR),
        static_cast<std::uint8_t>(MAGICTOOL_HOST_EXAMPLE_VERSION_MINOR),
        static_cast<std::uint8_t>(MAGICTOOL_HOST_EXAMPLE_VERSION_REVISION),
    };
}

int PrintUsage(const std::string &programName) {
    std::cout
        << "Usage:\n"
        << "  " << programName << " --version\n"
        << "  " << programName << " scan\n"
        << "  " << programName << " --set-default <device-name>\n"
        << "  " << programName << " [--device <device-name>|--port <port>] <command> [args]\n"
        << "  " << programName << " <device-name|port> <command> [args]\n"
        << "\n"
        << "Commands:\n"
        << "  pulse <output> <count>\n"
        << "  set <output>\n"
        << "  clear <output>\n"
        << "  toggle <output>\n"
        << "  read-inputs\n"
        << "  read-outputs\n"
        << "  ping <value>\n"
        << "  version\n"
        << "  hardware\n"
        << "  open\n"
        << "  close\n";
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

bool IsDeviceCommand(const std::string &command) {
    return command == "pulse" ||
           command == "set" ||
           command == "clear" ||
           command == "toggle" ||
           command == "read-inputs" ||
           command == "read-outputs" ||
           command == "ping" ||
           command == "version" ||
           command == "hardware" ||
           command == "open" ||
           command == "close";
}

bool LooksLikePortPath(const std::string &value) {
    return !value.empty() && value.front() == '/';
}

int PrintScannedPorts() {
    const std::vector<magictool::ScannedDevicePort> ports = magictool::ScanMagicToolSerialPorts();
    if (ports.empty()) {
        std::cout << "No magictool serial ports found\n";
        return 1;
    }

    for (const magictool::ScannedDevicePort &port : ports) {
        std::cout << port.path << '\n';
    }
    return 0;
}

bool ResolvePortFromArgs(int argc,
                         char *argv[],
                         int *commandIndexOut,
                         std::string *portNameOut,
                         std::string *errorOut) {
    if (!commandIndexOut || !portNameOut) {
        return false;
    }

    auto loadConfig = [&errorOut](magictool::DeviceConfig *config) {
        return magictool::LoadDeviceConfig(config, errorOut);
    };

    const std::string first = argv[1];
    if (first == "--device") {
        if (argc < 4) {
            if (errorOut) {
                *errorOut = "--device requires a device name and command";
            }
            return false;
        }

        magictool::DeviceConfig config;
        if (!loadConfig(&config)) {
            return false;
        }

        *commandIndexOut = 3;
        return magictool::ResolveConfiguredDevicePort(config, argv[2], portNameOut, errorOut);
    }

    if (first == "--port") {
        if (argc < 4) {
            if (errorOut) {
                *errorOut = "--port requires a port and command";
            }
            return false;
        }

        *portNameOut = argv[2];
        *commandIndexOut = 3;
        return true;
    }

    if (IsDeviceCommand(first)) {
        magictool::DeviceConfig config;
        if (!loadConfig(&config)) {
            return false;
        }

        *commandIndexOut = 1;
        return magictool::ResolveDefaultDevicePort(config, portNameOut, nullptr, errorOut);
    }

    if (argc < 3) {
        if (errorOut) {
            *errorOut = "Missing command";
        }
        return false;
    }

    const std::string target = argv[1];
    *commandIndexOut = 2;
    if (LooksLikePortPath(target)) {
        *portNameOut = target;
        return true;
    }

    magictool::DeviceConfig config;
    if (!loadConfig(&config)) {
        return false;
    }
    return magictool::ResolveConfiguredDevicePort(config, target, portNameOut, errorOut);
}

}  // namespace

int main(int argc, char *argv[]) {
    const std::string programName = argc > 0 ? argv[0] : "magictool_native";
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "magictool host "
                  << magictool::native::FormatVersion(HostExampleVersion()) << '\n';
        std::cout << "magictool library "
                  << magictool::native::FormatVersion(magictool::native::DebugToolDevice::LibraryVersion()) << '\n';
        return 0;
    }

    if (argc == 2 && std::string(argv[1]) == "scan") {
        return PrintScannedPorts();
    }

    if (argc == 3 && std::string(argv[1]) == "--set-default") {
        std::string error;
        if (!magictool::SetDefaultDeviceName(argv[2], &error)) {
            std::cerr << "Failed to set default device: " << error << '\n';
            return 2;
        }
        std::cout << "Default device set to " << argv[2] << '\n';
        return 0;
    }

    if (argc < 2) {
        return PrintUsage(programName);
    }

    int commandIndex = 0;
    std::string portName;
    std::string error;
    if (!ResolvePortFromArgs(argc, argv, &commandIndex, &portName, &error)) {
        std::cerr << error << '\n';
        return PrintUsage(programName);
    }

    const std::string command = argv[commandIndex];

    magictool::native::DebugToolDevice device;
    if (!device.Open(portName)) {
        std::cerr << "Failed to open " << portName << ": " << device.LastErrorString() << '\n';
        return 2;
    }

    bool ok = false;
    std::uint8_t value = 0;
    magictool::native::Version firmwareVersion;

    if (command == "pulse") {
        if (argc != commandIndex + 3) {
            return PrintUsage(programName);
        }
        ok = device.Pulse(ParseByte(argv[commandIndex + 1]), ParseByte(argv[commandIndex + 2]));
    } else if (command == "set") {
        if (argc != commandIndex + 2) {
            return PrintUsage(programName);
        }
        ok = device.Set(ParseByte(argv[commandIndex + 1]));
    } else if (command == "clear") {
        if (argc != commandIndex + 2) {
            return PrintUsage(programName);
        }
        ok = device.Clear(ParseByte(argv[commandIndex + 1]));
    } else if (command == "toggle") {
        if (argc != commandIndex + 2) {
            return PrintUsage(programName);
        }
        ok = device.Toggle(ParseByte(argv[commandIndex + 1]));
    } else if (command == "read-inputs") {
        if (argc != commandIndex + 1) {
            return PrintUsage(programName);
        }
        ok = device.ReadInputs(&value);
    } else if (command == "read-outputs") {
        if (argc != commandIndex + 1) {
            return PrintUsage(programName);
        }
        ok = device.ReadOutputs(&value);
    } else if (command == "ping") {
        if (argc != commandIndex + 2) {
            return PrintUsage(programName);
        }
        ok = device.Ping(ParseByte(argv[commandIndex + 1]), &value);
    } else if (command == "version") {
        if (argc != commandIndex + 1) {
            return PrintUsage(programName);
        }
        ok = device.GetFirmwareVersion(&firmwareVersion);
    } else if (command == "hardware") {
        if (argc != commandIndex + 1) {
            return PrintUsage(programName);
        }
        ok = device.GetHardwareVersion(&value);
    } else if (command == "open") {
        if (argc != commandIndex + 1) {
            return PrintUsage(programName);
        }
        ok = device.OpenTool();
    } else if (command == "close") {
        if (argc != commandIndex + 1) {
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
    if (command == "version") {
        std::cout << "Firmware version: "
                  << magictool::native::FormatVersion(firmwareVersion) << '\n';
    }

    return 0;
}
