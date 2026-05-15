#include <QCoreApplication>
#include <QDebug>
#include <QStringList>

#include <vector>

#include "magictool/device_config.h"
#include "magictool/magicdebug.h"

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

magictool::Version HostExampleVersion() {
    return magictool::Version{
        static_cast<quint8>(MAGICTOOL_HOST_EXAMPLE_VERSION_MAJOR),
        static_cast<quint8>(MAGICTOOL_HOST_EXAMPLE_VERSION_MINOR),
        static_cast<quint8>(MAGICTOOL_HOST_EXAMPLE_VERSION_REVISION),
    };
}

int PrintUsage(const QString &programName) {
    qInfo().noquote()
        << "Usage:\n"
        << "  " + programName + " --version\n"
        << "  " + programName + " scan\n"
        << "  " + programName + " --set-default <device-name>\n"
        << "  " + programName + " [--device <device-name>|--port <port>] <command> [args]\n"
        << "  " + programName + " <device-name|port> <command> [args]\n"
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
        << "  close";
    return 1;
}

QString HardwareTypeName(quint8 hardwareType) {
    switch (hardwareType) {
        case 0x1: return QStringLiteral("pico2");
        case 0x2: return QStringLiteral("pico2_w");
        default: return QStringLiteral("unknown");
    }
}

QString HardwareVersionName(quint8 hardwareVersion) {
    if (hardwareVersion == 0) {
        return QStringLiteral("unknown");
    }
    return QStringLiteral("v%1").arg(hardwareVersion);
}

bool IsDeviceCommand(const QString &command) {
    return command == QStringLiteral("pulse") ||
           command == QStringLiteral("set") ||
           command == QStringLiteral("clear") ||
           command == QStringLiteral("toggle") ||
           command == QStringLiteral("read-inputs") ||
           command == QStringLiteral("read-outputs") ||
           command == QStringLiteral("ping") ||
           command == QStringLiteral("version") ||
           command == QStringLiteral("hardware") ||
           command == QStringLiteral("open") ||
           command == QStringLiteral("close");
}

int PrintScannedPorts() {
    const std::vector<magictool::ScannedDevicePort> ports = magictool::ScanMagicToolSerialPorts();
    if (ports.empty()) {
        qInfo().noquote() << "No magictool serial ports found";
        return 1;
    }

    for (const magictool::ScannedDevicePort &port : ports) {
        qInfo().noquote() << QString::fromStdString(port.path);
    }
    return 0;
}

bool ResolvePortFromArgs(const QStringList &args,
                         int *commandIndexOut,
                         QString *portNameOut,
                         QString *errorOut) {
    if (!commandIndexOut || !portNameOut) {
        return false;
    }

    auto setError = [errorOut](const QString &message) {
        if (errorOut) {
            *errorOut = message;
        }
    };

    auto loadConfig = [&errorOut](magictool::DeviceConfig *config) {
        std::string error;
        if (!magictool::LoadDeviceConfig(config, &error)) {
            if (errorOut) {
                *errorOut = QString::fromStdString(error);
            }
            return false;
        }
        return true;
    };

    const QString first = args.at(1);
    if (first == QStringLiteral("--device")) {
        if (args.size() < 4) {
            setError(QStringLiteral("--device requires a device name and command"));
            return false;
        }

        magictool::DeviceConfig config;
        if (!loadConfig(&config)) {
            return false;
        }

        std::string port;
        std::string error;
        *commandIndexOut = 3;
        if (!magictool::ResolveConfiguredDevicePort(config, args.at(2).toStdString(), &port, &error)) {
            setError(QString::fromStdString(error));
            return false;
        }

        *portNameOut = QString::fromStdString(port);
        return true;
    }

    if (first == QStringLiteral("--port")) {
        if (args.size() < 4) {
            setError(QStringLiteral("--port requires a port and command"));
            return false;
        }

        *portNameOut = args.at(2);
        *commandIndexOut = 3;
        return true;
    }

    if (IsDeviceCommand(first)) {
        magictool::DeviceConfig config;
        if (!loadConfig(&config)) {
            return false;
        }

        std::string port;
        std::string error;
        *commandIndexOut = 1;
        if (!magictool::ResolveDefaultDevicePort(config, &port, nullptr, &error)) {
            setError(QString::fromStdString(error));
            return false;
        }

        *portNameOut = QString::fromStdString(port);
        return true;
    }

    if (args.size() < 3) {
        setError(QStringLiteral("Missing command"));
        return false;
    }

    const QString target = args.at(1);
    *commandIndexOut = 2;
    if (target.startsWith(QLatin1Char('/'))) {
        *portNameOut = target;
        return true;
    }

    magictool::DeviceConfig config;
    if (!loadConfig(&config)) {
        return false;
    }

    std::string port;
    std::string error;
    if (!magictool::ResolveConfiguredDevicePort(config, target.toStdString(), &port, &error)) {
        setError(QString::fromStdString(error));
        return false;
    }

    *portNameOut = QString::fromStdString(port);
    return true;
}

}  // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() == 2 && args.at(1) == QStringLiteral("--version")) {
        qInfo().noquote() << "magictool host" << magictool::FormatVersion(HostExampleVersion());
        qInfo().noquote() << "magictool library" << magictool::FormatVersion(magictool::DebugToolDevice::LibraryVersion());
        return 0;
    }

    if (args.size() == 2 && args.at(1) == QStringLiteral("scan")) {
        return PrintScannedPorts();
    }

    if (args.size() == 3 && args.at(1) == QStringLiteral("--set-default")) {
        std::string error;
        if (!magictool::SetDefaultDeviceName(args.at(2).toStdString(), &error)) {
            qWarning().noquote() << "Failed to set default device:" << QString::fromStdString(error);
            return 2;
        }
        qInfo().noquote() << "Default device set to" << args.at(2);
        return 0;
    }

    if (args.size() < 2) {
        return PrintUsage(args.value(0, QStringLiteral("magictool_basic_example")));
    }

    int commandIndex = 0;
    QString portName;
    QString error;
    if (!ResolvePortFromArgs(args, &commandIndex, &portName, &error)) {
        qWarning().noquote() << error;
        return PrintUsage(args.value(0, QStringLiteral("magictool_basic_example")));
    }

    const QString command = args.at(commandIndex).toLower();

    magictool::DebugToolDevice device;
    if (!device.Open(portName)) {
        qWarning().noquote() << "Failed to open" << portName << ":" << device.LastErrorString();
        return 2;
    }

    bool ok = false;
    quint8 value = 0;
    magictool::Version firmwareVersion;

    if (command == QStringLiteral("pulse")) {
        if (args.size() != commandIndex + 3) {
            return PrintUsage(args.at(0));
        }
        ok = device.Pulse(static_cast<quint8>(args.at(commandIndex + 1).toUInt()),
                          static_cast<quint8>(args.at(commandIndex + 2).toUInt()));
    } else if (command == QStringLiteral("set")) {
        if (args.size() != commandIndex + 2) {
            return PrintUsage(args.at(0));
        }
        ok = device.Set(static_cast<quint8>(args.at(commandIndex + 1).toUInt()));
    } else if (command == QStringLiteral("clear")) {
        if (args.size() != commandIndex + 2) {
            return PrintUsage(args.at(0));
        }
        ok = device.Clear(static_cast<quint8>(args.at(commandIndex + 1).toUInt()));
    } else if (command == QStringLiteral("toggle")) {
        if (args.size() != commandIndex + 2) {
            return PrintUsage(args.at(0));
        }
        ok = device.Toggle(static_cast<quint8>(args.at(commandIndex + 1).toUInt()));
    } else if (command == QStringLiteral("read-inputs")) {
        if (args.size() != commandIndex + 1) {
            return PrintUsage(args.at(0));
        }
        ok = device.ReadInputs(&value);
    } else if (command == QStringLiteral("read-outputs")) {
        if (args.size() != commandIndex + 1) {
            return PrintUsage(args.at(0));
        }
        ok = device.ReadOutputs(&value);
    } else if (command == QStringLiteral("ping")) {
        if (args.size() != commandIndex + 2) {
            return PrintUsage(args.at(0));
        }
        ok = device.Ping(static_cast<quint8>(args.at(commandIndex + 1).toUInt()), &value);
    } else if (command == QStringLiteral("version")) {
        if (args.size() != commandIndex + 1) {
            return PrintUsage(args.at(0));
        }
        ok = device.GetFirmwareVersion(&firmwareVersion);
    } else if (command == QStringLiteral("hardware")) {
        if (args.size() != commandIndex + 1) {
            return PrintUsage(args.at(0));
        }
        ok = device.GetHardwareVersion(&value);
    } else if (command == QStringLiteral("open")) {
        if (args.size() != commandIndex + 1) {
            return PrintUsage(args.at(0));
        }
        ok = device.OpenTool();
    } else if (command == QStringLiteral("close")) {
        if (args.size() != commandIndex + 1) {
            return PrintUsage(args.at(0));
        }
        ok = device.CloseTool();
    } else {
        return PrintUsage(args.at(0));
    }

    if (!ok) {
        qWarning().noquote() << "Command failed:" << device.LastErrorString();
        return 3;
    }

    qInfo().noquote() << "Response:" << device.LastResponse();
    if (command == QStringLiteral("read-inputs") ||
        command == QStringLiteral("read-outputs") ||
        command == QStringLiteral("ping") ||
        command == QStringLiteral("hardware")) {
        qInfo().noquote() << "Value:" << value;
        if (command == QStringLiteral("hardware")) {
            const quint8 hardwareType = static_cast<quint8>((value >> 4) & 0x0F);
            const quint8 hardwareVersion = static_cast<quint8>(value & 0x0F);
            qInfo().noquote() << "Hardware:"
                              << HardwareTypeName(hardwareType)
                              << HardwareVersionName(hardwareVersion);
        }
    }
    if (command == QStringLiteral("version")) {
        qInfo().noquote() << "Firmware version:" << magictool::FormatVersion(firmwareVersion);
    }

    return 0;
}
