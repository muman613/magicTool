#include <QCoreApplication>
#include <QDebug>
#include <QStringList>

#include "debug_tool_qt5/DebugToolDevice.h"

namespace {

int PrintUsage(const QString &programName) {
    qInfo().noquote()
        << "Usage:\n"
        << "  " + programName + " <port> pulse <count>\n"
        << "  " + programName + " <port> set <0|1>\n"
        << "  " + programName + " <port> clear\n"
        << "  " + programName + " <port> toggle";
    return 1;
}

}  // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 3) {
        return PrintUsage(args.value(0, QStringLiteral("debug_tool_qt5_basic_example")));
    }

    const QString portName = args.at(1);
    const QString command = args.at(2).toLower();

    debug_tool_qt5::DebugToolDevice device;
    if (!device.Open(portName)) {
        qWarning().noquote() << "Failed to open" << portName << ":" << device.LastErrorString();
        return 2;
    }

    bool ok = false;
    if (command == QStringLiteral("pulse")) {
        if (args.size() != 4) {
            return PrintUsage(args.at(0));
        }
        ok = device.Pulse(args.at(3).toInt());
    } else if (command == QStringLiteral("set")) {
        if (args.size() != 4) {
            return PrintUsage(args.at(0));
        }
        ok = device.Set(args.at(3).toInt());
    } else if (command == QStringLiteral("clear")) {
        if (args.size() != 3) {
            return PrintUsage(args.at(0));
        }
        ok = device.Clear();
    } else if (command == QStringLiteral("toggle")) {
        if (args.size() != 3) {
            return PrintUsage(args.at(0));
        }
        ok = device.Toggle();
    } else {
        return PrintUsage(args.at(0));
    }

    if (!ok) {
        qWarning().noquote() << "Command failed:" << device.LastErrorString();
        return 3;
    }

    qInfo().noquote() << "Response:" << device.LastResponse();
    return 0;
}
