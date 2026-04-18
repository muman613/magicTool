#include <QCoreApplication>
#include <QDebug>
#include <QStringList>

#include "magictool/magicdebug.h"

namespace {

int PrintUsage(const QString &programName) {
    qInfo().noquote()
        << "Usage:\n"
        << "  " + programName + " <port> pulse <output> <count>\n"
        << "  " + programName + " <port> set <output>\n"
        << "  " + programName + " <port> clear <output>\n"
        << "  " + programName + " <port> toggle <output>\n"
        << "  " + programName + " <port> read-inputs\n"
        << "  " + programName + " <port> read-outputs\n"
        << "  " + programName + " <port> ping <value>\n"
        << "  " + programName + " <port> version\n"
        << "  " + programName + " <port> hardware\n"
        << "  " + programName + " <port> open\n"
        << "  " + programName + " <port> close";
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

}  // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 3) {
        return PrintUsage(args.value(0, QStringLiteral("magictool_basic_example")));
    }

    const QString portName = args.at(1);
    const QString command = args.at(2).toLower();

    magictool::DebugToolDevice device;
    if (!device.Open(portName)) {
        qWarning().noquote() << "Failed to open" << portName << ":" << device.LastErrorString();
        return 2;
    }

    bool ok = false;
    quint8 value = 0;

    if (command == QStringLiteral("pulse")) {
        if (args.size() != 5) {
            return PrintUsage(args.at(0));
        }
        ok = device.Pulse(static_cast<quint8>(args.at(3).toUInt()),
                          static_cast<quint8>(args.at(4).toUInt()));
    } else if (command == QStringLiteral("set")) {
        if (args.size() != 4) {
            return PrintUsage(args.at(0));
        }
        ok = device.Set(static_cast<quint8>(args.at(3).toUInt()));
    } else if (command == QStringLiteral("clear")) {
        if (args.size() != 4) {
            return PrintUsage(args.at(0));
        }
        ok = device.Clear(static_cast<quint8>(args.at(3).toUInt()));
    } else if (command == QStringLiteral("toggle")) {
        if (args.size() != 4) {
            return PrintUsage(args.at(0));
        }
        ok = device.Toggle(static_cast<quint8>(args.at(3).toUInt()));
    } else if (command == QStringLiteral("read-inputs")) {
        if (args.size() != 3) {
            return PrintUsage(args.at(0));
        }
        ok = device.ReadInputs(&value);
    } else if (command == QStringLiteral("read-outputs")) {
        if (args.size() != 3) {
            return PrintUsage(args.at(0));
        }
        ok = device.ReadOutputs(&value);
    } else if (command == QStringLiteral("ping")) {
        if (args.size() != 4) {
            return PrintUsage(args.at(0));
        }
        ok = device.Ping(static_cast<quint8>(args.at(3).toUInt()), &value);
    } else if (command == QStringLiteral("version")) {
        if (args.size() != 3) {
            return PrintUsage(args.at(0));
        }
        ok = device.GetVersion(&value);
    } else if (command == QStringLiteral("hardware")) {
        if (args.size() != 3) {
            return PrintUsage(args.at(0));
        }
        ok = device.GetHardwareVersion(&value);
    } else if (command == QStringLiteral("open")) {
        if (args.size() != 3) {
            return PrintUsage(args.at(0));
        }
        ok = device.OpenTool();
    } else if (command == QStringLiteral("close")) {
        if (args.size() != 3) {
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
        command == QStringLiteral("version") ||
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

    return 0;
}
