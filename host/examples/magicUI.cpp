#include <algorithm>
#include <array>
#include <functional>

#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMetaObject>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

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

QString FormatBits(quint8 bits, int width) {
    QString text;
    for (int bit = width - 1; bit >= 0; --bit) {
        text += ((bits >> bit) & 0x1) ? QLatin1Char('1') : QLatin1Char('0');
    }
    return text;
}

QString FormatTimestamped(const QString &message) {
    return QStringLiteral("[%1] %2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")))
        .arg(message);
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

QString FormatHardwareVersionByte(quint8 hardwareVersionByte) {
    const quint8 hardwareType = static_cast<quint8>((hardwareVersionByte >> 4) & 0x0F);
    const quint8 hardwareVersion = static_cast<quint8>(hardwareVersionByte & 0x0F);
    return QStringLiteral("%1 %2 (0x%3)")
        .arg(HardwareTypeName(hardwareType),
             HardwareVersionName(hardwareVersion),
             QStringLiteral("%1").arg(hardwareVersionByte, 2, 16, QLatin1Char('0')).toUpper());
}

QString SerialFromByIdName(const QString &idName) {
    const int serialStart = idName.lastIndexOf(QLatin1Char('_'));
    const int serialEnd = idName.indexOf(QStringLiteral("-if"), serialStart + 1);
    if (serialStart < 0 || serialEnd <= serialStart + 1) {
        return idName;
    }
    return idName.mid(serialStart + 1, serialEnd - serialStart - 1);
}

void SetIndicatorState(QLabel *label,
                       bool high,
                       const QString &highText = QStringLiteral("HIGH"),
                       const QString &lowText = QStringLiteral("LOW"),
                       bool emphasize = false) {
    label->setText(high ? highText : lowText);

    const QString border = emphasize ? QStringLiteral("2px solid #0f172a") : QStringLiteral("1px solid #64748b");
    const QString background = high ? QStringLiteral("#22c55e") : QStringLiteral("#e2e8f0");
    const QString foreground = high ? QStringLiteral("#052e16") : QStringLiteral("#334155");

    label->setStyleSheet(QStringLiteral(
        "QLabel {"
        " border: %1;"
        " border-radius: 4px;"
        " background: %2;"
        " color: %3;"
        " font-weight: 700;"
        " padding: 4px 10px;"
        " min-width: 54px;"
        "}")
        .arg(border, background, foreground));
}

class DeviceWorker : public QObject {
    Q_OBJECT

public:
    explicit DeviceWorker(QObject *parent = nullptr)
        : QObject(parent),
          pollTimer_(new QTimer(this)) {
        pollTimer_->setInterval(50);
        connect(pollTimer_, &QTimer::timeout, this, &DeviceWorker::PollEvents);
    }

signals:
    void LogMessage(const QString &message);
    void ConnectionChanged(bool connected, const QString &portName);
    void InputsUpdated(quint8 bits, quint8 changedMask);
    void OutputsUpdated(quint8 bits);
    void NotifyMaskUpdated(quint8 bits);

public slots:
    void StartPolling() {
        if (!pollTimer_->isActive()) {
            pollTimer_->start();
        }
    }

    void ConnectToPort(const QString &portName) {
        if (portName.isEmpty()) {
            emit LogMessage(FormatTimestamped(QStringLiteral("Error: no serial port selected")));
            return;
        }

        if (device_.IsOpen()) {
            DisconnectDevice();
        }

        emit LogMessage(FormatTimestamped(QStringLiteral("Opening %1").arg(portName)));
        if (!device_.Open(portName)) {
            emit LogMessage(FormatTimestamped(QStringLiteral("Error: %1").arg(device_.LastErrorString())));
            emit ConnectionChanged(false, QString());
            return;
        }

        emit ConnectionChanged(true, device_.PortName());

        ExecuteCommand(QStringLiteral("OPEN"), [this]() {
            return device_.OpenTool();
        });

        ExecuteCommand(QStringLiteral("ENABLE_NOTIFY all"), [this]() {
            return device_.EnableAllNotify();
        }, [this]() {
            emit NotifyMaskUpdated(device_.LastPacket().arg & 0x03);
        });

        magictool::Version version;
        ExecuteCommand(QStringLiteral("GET_VERSION"), [this, &version]() {
            return device_.GetFirmwareVersion(&version);
        }, [this, &version]() {
            emit LogMessage(FormatTimestamped(QStringLiteral("Firmware version: %1").arg(magictool::FormatVersion(version))));
        });

        quint8 hardwareVersion = 0;
        ExecuteCommand(QStringLiteral("GET_HARDWARE_VERSION"), [this, &hardwareVersion]() {
            return device_.GetHardwareVersion(&hardwareVersion);
        }, [this, &hardwareVersion]() {
            emit LogMessage(FormatTimestamped(QStringLiteral("Hardware: %1").arg(FormatHardwareVersionByte(hardwareVersion))));
        });

        quint8 inputs = 0;
        ExecuteCommand(QStringLiteral("READ_INPUTS"), [this, &inputs]() {
            return device_.ReadInputs(&inputs);
        }, [this, &inputs]() {
            emit InputsUpdated(static_cast<quint8>(inputs & 0x03), 0x03);
        });

        quint8 outputs = 0;
        ExecuteCommand(QStringLiteral("READ_OUTPUTS"), [this, &outputs]() {
            return device_.ReadOutputs(&outputs);
        }, [this, &outputs]() {
            emit OutputsUpdated(static_cast<quint8>(outputs & 0x0F));
        });
    }

    void DisconnectDevice() {
        if (!device_.IsOpen()) {
            emit ConnectionChanged(false, QString());
            return;
        }

        ExecuteCommand(QStringLiteral("CLOSE"), [this]() {
            return device_.CloseTool();
        });

        emit LogMessage(FormatTimestamped(QStringLiteral("Closing %1").arg(device_.PortName())));
        device_.Close();
        emit ConnectionChanged(false, QString());
    }

    void SetOutput(int index) {
        ExecuteOutputCommand(QStringLiteral("SET output %1").arg(index), [this, index]() {
            return device_.Set(static_cast<quint8>(index));
        });
    }

    void ClearOutput(int index) {
        ExecuteOutputCommand(QStringLiteral("CLEAR output %1").arg(index), [this, index]() {
            return device_.Clear(static_cast<quint8>(index));
        });
    }

    void ToggleOutput(int index) {
        ExecuteOutputCommand(QStringLiteral("TOGGLE output %1").arg(index), [this, index]() {
            return device_.Toggle(static_cast<quint8>(index));
        });
    }

    void PulseOutput(int index, int count) {
        ExecuteOutputCommand(QStringLiteral("PULSE output %1 count %2").arg(index).arg(count), [this, index, count]() {
            return device_.Pulse(static_cast<quint8>(index), static_cast<quint8>(count));
        });
    }

    void WriteMask(quint8 mask) {
        ExecuteOutputCommand(QStringLiteral("WRITE_MASK 0b%1").arg(FormatBits(mask, 4)), [this, mask]() {
            return device_.WriteMask(mask);
        });
    }

    void ReadInputs() {
        quint8 inputs = 0;
        ExecuteCommand(QStringLiteral("READ_INPUTS"), [this, &inputs]() {
            return device_.ReadInputs(&inputs);
        }, [this, &inputs]() {
            emit LogMessage(FormatTimestamped(QStringLiteral("Inputs: 0b%1").arg(FormatBits(inputs, 2))));
            emit InputsUpdated(static_cast<quint8>(inputs & 0x03), 0x03);
        });
    }

    void ReadOutputs() {
        quint8 outputs = 0;
        ExecuteCommand(QStringLiteral("READ_OUTPUTS"), [this, &outputs]() {
            return device_.ReadOutputs(&outputs);
        }, [this, &outputs]() {
            emit LogMessage(FormatTimestamped(QStringLiteral("Outputs: 0b%1").arg(FormatBits(outputs, 4))));
            emit OutputsUpdated(static_cast<quint8>(outputs & 0x0F));
        });
    }

    void GetVersion() {
        magictool::Version version;
        ExecuteCommand(QStringLiteral("GET_VERSION"), [this, &version]() {
            return device_.GetFirmwareVersion(&version);
        }, [this, &version]() {
            emit LogMessage(FormatTimestamped(QStringLiteral("Firmware version: %1").arg(magictool::FormatVersion(version))));
        });
    }

    void GetHardwareVersion() {
        quint8 hardwareVersion = 0;
        ExecuteCommand(QStringLiteral("GET_HARDWARE_VERSION"), [this, &hardwareVersion]() {
            return device_.GetHardwareVersion(&hardwareVersion);
        }, [this, &hardwareVersion]() {
            emit LogMessage(FormatTimestamped(QStringLiteral("Hardware: %1").arg(FormatHardwareVersionByte(hardwareVersion))));
        });
    }

    void Ping(int value) {
        quint8 echoed = 0;
        ExecuteCommand(QStringLiteral("PING %1").arg(value), [this, value, &echoed]() {
            return device_.Ping(static_cast<quint8>(value), &echoed);
        }, [this, echoed]() {
            emit LogMessage(FormatTimestamped(QStringLiteral("Ping reply: %1").arg(echoed)));
        });
    }

    void EnableNotify(int index) {
        ExecuteCommand(QStringLiteral("ENABLE_NOTIFY input %1").arg(index), [this, index]() {
            return device_.EnableNotify(static_cast<quint8>(index));
        }, [this]() {
            emit NotifyMaskUpdated(device_.LastPacket().arg & 0x03);
        });
    }

    void DisableNotify(int index) {
        ExecuteCommand(QStringLiteral("DISABLE_NOTIFY input %1").arg(index), [this, index]() {
            return device_.DisableNotify(static_cast<quint8>(index));
        }, [this]() {
            emit NotifyMaskUpdated(device_.LastPacket().arg & 0x03);
        });
    }

    void EnableAllNotify() {
        ExecuteCommand(QStringLiteral("ENABLE_NOTIFY all"), [this]() {
            return device_.EnableAllNotify();
        }, [this]() {
            emit NotifyMaskUpdated(device_.LastPacket().arg & 0x03);
        });
    }

    void DisableAllNotify() {
        ExecuteCommand(QStringLiteral("DISABLE_NOTIFY all"), [this]() {
            return device_.DisableAllNotify();
        }, [this]() {
            emit NotifyMaskUpdated(device_.LastPacket().arg & 0x03);
        });
    }

private slots:
    void PollEvents() {
        if (!device_.IsOpen()) {
            return;
        }

        magictool::EventPacket packet;
        while (device_.TakePendingEvent(&packet)) {
            HandleEvent(packet);
        }

        if (!device_.WaitForEvent(&packet, 1)) {
            return;
        }

        HandleEvent(packet);

        while (device_.TakePendingEvent(&packet)) {
            HandleEvent(packet);
        }
    }

private:
    template <typename CommandFn, typename SuccessFn>
    bool ExecuteCommand(const QString &request, CommandFn command, SuccessFn onSuccess) {
        emit LogMessage(FormatTimestamped(QStringLiteral("Request: %1").arg(request)));
        if (!device_.IsOpen()) {
            emit LogMessage(FormatTimestamped(QStringLiteral("Error: device is not connected")));
            return false;
        }

        if (!command()) {
            emit LogMessage(FormatTimestamped(QStringLiteral("Error: %1").arg(device_.LastErrorString())));
            return false;
        }

        emit LogMessage(FormatTimestamped(QStringLiteral("Response: %1").arg(device_.LastResponse())));
        onSuccess();
        return true;
    }

    template <typename CommandFn>
    bool ExecuteCommand(const QString &request, CommandFn command) {
        return ExecuteCommand(request, command, []() {});
    }

    template <typename CommandFn>
    void ExecuteOutputCommand(const QString &request, CommandFn command) {
        ExecuteCommand(request, command, [this]() {
            emit OutputsUpdated(static_cast<quint8>(device_.LastPacket().arg & 0x0F));
        });
    }

    void HandleEvent(const magictool::EventPacket &packet) {
        if (packet.Type() != magictool::EVT_INPUT_CHANGE) {
            emit LogMessage(FormatTimestamped(QStringLiteral("Event: %1 info=%2 arg=%3")
                .arg(static_cast<int>(packet.Type()))
                .arg(packet.Info())
                .arg(packet.arg)));
            return;
        }

        const quint8 changedMask = static_cast<quint8>(packet.Info() & 0x03);
        const quint8 currentBits = static_cast<quint8>(packet.arg & 0x03);
        emit LogMessage(FormatTimestamped(QStringLiteral("Event: INPUT_CHANGE changed=0b%1 current=0b%2")
            .arg(FormatBits(changedMask, 2))
            .arg(FormatBits(currentBits, 2))));
        emit InputsUpdated(currentBits, changedMask);
    }

    magictool::DebugToolDevice device_;
    QTimer *pollTimer_;
};

class MainWindow : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle(QStringLiteral("magicUI"));
        resize(1200, 760);

        auto *central = new QWidget(this);
        auto *mainLayout = new QVBoxLayout(central);
        mainLayout->setSpacing(12);

        auto *connectionBox = new QGroupBox(QStringLiteral("Connection"), central);
        auto *connectionLayout = new QHBoxLayout(connectionBox);
        portCombo_ = new QComboBox(connectionBox);
        portCombo_->setEditable(true);
        portCombo_->setMinimumContentsLength(18);
        auto *refreshButton = new QPushButton(QStringLiteral("Refresh"), connectionBox);
        connectButton_ = new QPushButton(QStringLiteral("Connect"), connectionBox);
        disconnectButton_ = new QPushButton(QStringLiteral("Disconnect"), connectionBox);
        connectionStatusLabel_ = new QLabel(QStringLiteral("Disconnected"), connectionBox);
        connectionLayout->addWidget(new QLabel(QStringLiteral("Port:"), connectionBox));
        connectionLayout->addWidget(portCombo_, 1);
        connectionLayout->addWidget(refreshButton);
        connectionLayout->addWidget(connectButton_);
        connectionLayout->addWidget(disconnectButton_);
        connectionLayout->addWidget(connectionStatusLabel_);
        mainLayout->addWidget(connectionBox);

        auto *controlLayout = new QHBoxLayout();
        controlLayout->setSpacing(12);

        auto *outputsBox = new QGroupBox(QStringLiteral("Outputs"), central);
        auto *outputsLayout = new QGridLayout(outputsBox);
        outputsLayout->addWidget(new QLabel(QStringLiteral("Output"), outputsBox), 0, 0);
        outputsLayout->addWidget(new QLabel(QStringLiteral("State"), outputsBox), 0, 1);
        outputsLayout->addWidget(new QLabel(QStringLiteral("Actions"), outputsBox), 0, 2);
        for (int index = 0; index < 4; ++index) {
            auto *nameLabel = new QLabel(QStringLiteral("OUT%1").arg(index), outputsBox);
            outputStateLabels_[index] = new QLabel(outputsBox);
            SetIndicatorState(outputStateLabels_[index], false);
            auto *row = new QWidget(outputsBox);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            auto *setButton = new QPushButton(QStringLiteral("Set"), row);
            auto *clearButton = new QPushButton(QStringLiteral("Clear"), row);
            auto *toggleButton = new QPushButton(QStringLiteral("Toggle"), row);
            auto *pulseCount = new QSpinBox(row);
            pulseCount->setRange(1, 255);
            pulseCount->setValue(1);
            auto *pulseButton = new QPushButton(QStringLiteral("Pulse"), row);
            rowLayout->addWidget(setButton);
            rowLayout->addWidget(clearButton);
            rowLayout->addWidget(toggleButton);
            rowLayout->addWidget(new QLabel(QStringLiteral("Count"), row));
            rowLayout->addWidget(pulseCount);
            rowLayout->addWidget(pulseButton);

            outputsLayout->addWidget(nameLabel, index + 1, 0);
            outputsLayout->addWidget(outputStateLabels_[index], index + 1, 1);
            outputsLayout->addWidget(row, index + 1, 2);

            managedWidgets_.push_back(setButton);
            managedWidgets_.push_back(clearButton);
            managedWidgets_.push_back(toggleButton);
            managedWidgets_.push_back(pulseCount);
            managedWidgets_.push_back(pulseButton);

            connect(setButton, &QPushButton::clicked, this, [this, index]() {
                InvokeWorker([this, index]() { worker_->SetOutput(index); });
            });
            connect(clearButton, &QPushButton::clicked, this, [this, index]() {
                InvokeWorker([this, index]() { worker_->ClearOutput(index); });
            });
            connect(toggleButton, &QPushButton::clicked, this, [this, index]() {
                InvokeWorker([this, index]() { worker_->ToggleOutput(index); });
            });
            connect(pulseButton, &QPushButton::clicked, this, [this, index, pulseCount]() {
                InvokeWorker([this, index, pulseCount]() { worker_->PulseOutput(index, pulseCount->value()); });
            });
        }

        auto *maskBox = new QGroupBox(QStringLiteral("Write Mask"), outputsBox);
        auto *maskLayout = new QHBoxLayout(maskBox);
        for (int bit = 0; bit < 4; ++bit) {
            maskChecks_[bit] = new QPushButton(QStringLiteral("OUT%1").arg(bit), maskBox);
            maskChecks_[bit]->setCheckable(true);
            maskLayout->addWidget(maskChecks_[bit]);
            managedWidgets_.push_back(maskChecks_[bit]);
        }
        auto *applyMaskButton = new QPushButton(QStringLiteral("Apply Mask"), maskBox);
        maskLayout->addWidget(applyMaskButton);
        managedWidgets_.push_back(applyMaskButton);
        outputsLayout->addWidget(maskBox, 5, 0, 1, 3);
        connect(applyMaskButton, &QPushButton::clicked, this, [this]() {
            quint8 mask = 0;
            for (int bit = 0; bit < 4; ++bit) {
                if (maskChecks_[bit]->isChecked()) {
                    mask |= static_cast<quint8>(1u << bit);
                }
            }
            InvokeWorker([this, mask]() { worker_->WriteMask(mask); });
        });

        auto *inputsBox = new QGroupBox(QStringLiteral("Inputs"), central);
        auto *inputsLayout = new QGridLayout(inputsBox);
        inputsLayout->addWidget(new QLabel(QStringLiteral("Input"), inputsBox), 0, 0);
        inputsLayout->addWidget(new QLabel(QStringLiteral("State"), inputsBox), 0, 1);
        inputsLayout->addWidget(new QLabel(QStringLiteral("Notify"), inputsBox), 0, 2);
        inputsLayout->addWidget(new QLabel(QStringLiteral("Actions"), inputsBox), 0, 3);
        for (int index = 0; index < 2; ++index) {
            inputStateLabels_[index] = new QLabel(inputsBox);
            notifyLabels_[index] = new QLabel(inputsBox);
            SetIndicatorState(inputStateLabels_[index], false);
            SetIndicatorState(notifyLabels_[index], true, QStringLiteral("ON"), QStringLiteral("OFF"));
            auto *enableButton = new QPushButton(QStringLiteral("Enable"), inputsBox);
            auto *disableButton = new QPushButton(QStringLiteral("Disable"), inputsBox);
            inputsLayout->addWidget(new QLabel(QStringLiteral("IN%1").arg(index), inputsBox), index + 1, 0);
            inputsLayout->addWidget(inputStateLabels_[index], index + 1, 1);
            inputsLayout->addWidget(notifyLabels_[index], index + 1, 2);
            auto *buttonRow = new QWidget(inputsBox);
            auto *buttonLayout = new QHBoxLayout(buttonRow);
            buttonLayout->setContentsMargins(0, 0, 0, 0);
            buttonLayout->addWidget(enableButton);
            buttonLayout->addWidget(disableButton);
            inputsLayout->addWidget(buttonRow, index + 1, 3);

            managedWidgets_.push_back(enableButton);
            managedWidgets_.push_back(disableButton);

            connect(enableButton, &QPushButton::clicked, this, [this, index]() {
                InvokeWorker([this, index]() { worker_->EnableNotify(index); });
            });
            connect(disableButton, &QPushButton::clicked, this, [this, index]() {
                InvokeWorker([this, index]() { worker_->DisableNotify(index); });
            });
        }

        auto *notifyButtons = new QWidget(inputsBox);
        auto *notifyLayout = new QHBoxLayout(notifyButtons);
        notifyLayout->setContentsMargins(0, 0, 0, 0);
        auto *enableAllButton = new QPushButton(QStringLiteral("Enable All"), notifyButtons);
        auto *disableAllButton = new QPushButton(QStringLiteral("Disable All"), notifyButtons);
        notifyLayout->addWidget(enableAllButton);
        notifyLayout->addWidget(disableAllButton);
        inputsLayout->addWidget(notifyButtons, 3, 0, 1, 4);
        managedWidgets_.push_back(enableAllButton);
        managedWidgets_.push_back(disableAllButton);
        connect(enableAllButton, &QPushButton::clicked, this, [this]() {
            InvokeWorker([this]() { worker_->EnableAllNotify(); });
        });
        connect(disableAllButton, &QPushButton::clicked, this, [this]() {
            InvokeWorker([this]() { worker_->DisableAllNotify(); });
        });

        auto *deviceBox = new QGroupBox(QStringLiteral("Device"), central);
        auto *deviceLayout = new QFormLayout(deviceBox);
        auto *readInputsButton = new QPushButton(QStringLiteral("Read Inputs"), deviceBox);
        auto *readOutputsButton = new QPushButton(QStringLiteral("Read Outputs"), deviceBox);
        auto *versionButton = new QPushButton(QStringLiteral("Get Version"), deviceBox);
        auto *hardwareButton = new QPushButton(QStringLiteral("Get Hardware"), deviceBox);
        auto *pingRow = new QWidget(deviceBox);
        auto *pingLayout = new QHBoxLayout(pingRow);
        pingLayout->setContentsMargins(0, 0, 0, 0);
        pingSpin_ = new QSpinBox(pingRow);
        pingSpin_->setRange(0, 255);
        auto *pingButton = new QPushButton(QStringLiteral("Ping"), pingRow);
        pingLayout->addWidget(pingSpin_);
        pingLayout->addWidget(pingButton);
        deviceLayout->addRow(QStringLiteral("Query inputs"), readInputsButton);
        deviceLayout->addRow(QStringLiteral("Query outputs"), readOutputsButton);
        deviceLayout->addRow(QStringLiteral("Firmware"), versionButton);
        deviceLayout->addRow(QStringLiteral("Hardware"), hardwareButton);
        deviceLayout->addRow(QStringLiteral("Ping"), pingRow);
        managedWidgets_.push_back(readInputsButton);
        managedWidgets_.push_back(readOutputsButton);
        managedWidgets_.push_back(versionButton);
        managedWidgets_.push_back(hardwareButton);
        managedWidgets_.push_back(pingSpin_);
        managedWidgets_.push_back(pingButton);

        connect(readInputsButton, &QPushButton::clicked, this, [this]() {
            InvokeWorker([this]() { worker_->ReadInputs(); });
        });
        connect(readOutputsButton, &QPushButton::clicked, this, [this]() {
            InvokeWorker([this]() { worker_->ReadOutputs(); });
        });
        connect(versionButton, &QPushButton::clicked, this, [this]() {
            InvokeWorker([this]() { worker_->GetVersion(); });
        });
        connect(hardwareButton, &QPushButton::clicked, this, [this]() {
            InvokeWorker([this]() { worker_->GetHardwareVersion(); });
        });
        connect(pingButton, &QPushButton::clicked, this, [this]() {
            InvokeWorker([this]() { worker_->Ping(pingSpin_->value()); });
        });

        controlLayout->addWidget(outputsBox, 2);
        controlLayout->addWidget(inputsBox, 1);
        controlLayout->addWidget(deviceBox, 1);
        mainLayout->addLayout(controlLayout);

        auto *logBox = new QGroupBox(QStringLiteral("Protocol Log"), central);
        auto *logLayout = new QVBoxLayout(logBox);
        logView_ = new QTextEdit(logBox);
        logView_->setReadOnly(true);
        logView_->setLineWrapMode(QTextEdit::NoWrap);
        logLayout->addWidget(logView_);
        mainLayout->addWidget(logBox, 1);

        setCentralWidget(central);

        worker_ = new DeviceWorker();
        worker_->moveToThread(&workerThread_);
        connect(&workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
        connect(&workerThread_, &QThread::started, worker_, &DeviceWorker::StartPolling);
        connect(worker_, &DeviceWorker::LogMessage, this, [this](const QString &message) {
            logView_->append(message);
        });
        connect(worker_, &DeviceWorker::ConnectionChanged, this, [this](bool connected, const QString &portName) {
            UpdateConnectionState(connected, portName);
        });
        connect(worker_, &DeviceWorker::InputsUpdated, this, [this](quint8 bits, quint8 changedMask) {
            UpdateInputIndicators(bits, changedMask);
        });
        connect(worker_, &DeviceWorker::OutputsUpdated, this, [this](quint8 bits) {
            UpdateOutputIndicators(bits);
        });
        connect(worker_, &DeviceWorker::NotifyMaskUpdated, this, [this](quint8 bits) {
            UpdateNotifyIndicators(bits);
        });
        workerThread_.start();

        connect(refreshButton, &QPushButton::clicked, this, [this]() {
            RefreshPorts();
        });
        connect(connectButton_, &QPushButton::clicked, this, [this]() {
            InvokeWorker([this]() { worker_->ConnectToPort(SelectedPortName()); });
        });
        connect(disconnectButton_, &QPushButton::clicked, this, [this]() {
            InvokeWorker([this]() { worker_->DisconnectDevice(); });
        });

        RefreshPorts();
        UpdateConnectionState(false, QString());
        UpdateInputIndicators(0, 0x03);
        UpdateOutputIndicators(0);
        UpdateNotifyIndicators(0x03);
        logView_->append(FormatTimestamped(QStringLiteral("Host version: %1").arg(magictool::FormatVersion(HostExampleVersion()))));
        logView_->append(FormatTimestamped(QStringLiteral("Library version: %1").arg(magictool::FormatVersion(magictool::DebugToolDevice::LibraryVersion()))));
    }

    ~MainWindow() override {
        InvokeWorker([this]() { worker_->DisconnectDevice(); });
        workerThread_.quit();
        workerThread_.wait();
    }

private:
    void RefreshPorts() {
        const QString current = SelectedPortName();
        portCombo_->clear();
        hasDevicePorts_ = false;

        const QDir serialById(QStringLiteral("/dev/serial/by-id"));
        const QFileInfoList entries = serialById.entryInfoList(
            QDir::Files | QDir::System | QDir::NoDotAndDotDot,
            QDir::Name);

        for (const QFileInfo &entry : entries) {
            const QString idName = entry.fileName();
            if (!idName.startsWith(QStringLiteral("usb-magictool"), Qt::CaseInsensitive)) {
                continue;
            }

            QString portPath = entry.canonicalFilePath();
            if (portPath.isEmpty()) {
                portPath = entry.symLinkTarget();
            }
            if (portPath.isEmpty()) {
                continue;
            }

            const QString label = QStringLiteral("%1 (%2)")
                .arg(SerialFromByIdName(idName), QFileInfo(portPath).fileName());
            portCombo_->addItem(label, portPath);
        }

        hasDevicePorts_ = portCombo_->count() > 0;
        if (!hasDevicePorts_) {
            portCombo_->addItem(QStringLiteral("No magictool devices found"), QString());
        }

        const int matchIndex = portCombo_->findData(current);
        if (matchIndex >= 0) {
            portCombo_->setCurrentIndex(matchIndex);
        } else if (hasDevicePorts_) {
            portCombo_->setCurrentIndex(0);
        }

        if (!isConnected_) {
            UpdateConnectionState(false, QString());
        }
    }

    void UpdateConnectionState(bool connected, const QString &portName) {
        isConnected_ = connected;
        connectionStatusLabel_->setText(connected
            ? QStringLiteral("Connected: %1").arg(portName)
            : (hasDevicePorts_
                ? QStringLiteral("Disconnected")
                : QStringLiteral("No magictool devices found")));
        connectButton_->setEnabled(!connected && hasDevicePorts_);
        disconnectButton_->setEnabled(connected);
        portCombo_->setEnabled(!connected && hasDevicePorts_);
        for (QWidget *widget : managedWidgets_) {
            widget->setEnabled(connected);
        }
    }

    void UpdateInputIndicators(quint8 bits, quint8 changedMask) {
        for (int index = 0; index < 2; ++index) {
            const bool high = ((bits >> index) & 0x1) != 0;
            const bool changed = ((changedMask >> index) & 0x1) != 0;
            SetIndicatorState(inputStateLabels_[index], high, QStringLiteral("HIGH"), QStringLiteral("LOW"), changed);
        }
    }

    void UpdateOutputIndicators(quint8 bits) {
        for (int index = 0; index < 4; ++index) {
            const bool high = ((bits >> index) & 0x1) != 0;
            SetIndicatorState(outputStateLabels_[index], high);
            maskChecks_[index]->setChecked(high);
        }
    }

    void UpdateNotifyIndicators(quint8 bits) {
        for (int index = 0; index < 2; ++index) {
            const bool enabled = ((bits >> index) & 0x1) != 0;
            SetIndicatorState(notifyLabels_[index], enabled, QStringLiteral("ON"), QStringLiteral("OFF"));
        }
    }

    void InvokeWorker(const std::function<void()> &fn) {
        QMetaObject::invokeMethod(worker_, fn, Qt::QueuedConnection);
    }

    QString SelectedPortName() const {
        const QVariant data = portCombo_->currentData();
        return data.isValid() ? data.toString() : QString();
    }

    QComboBox *portCombo_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *disconnectButton_ = nullptr;
    QLabel *connectionStatusLabel_ = nullptr;
    QTextEdit *logView_ = nullptr;
    QSpinBox *pingSpin_ = nullptr;
    std::array<QLabel *, 4> outputStateLabels_{};
    std::array<QPushButton *, 4> maskChecks_{};
    std::array<QLabel *, 2> inputStateLabels_{};
    std::array<QLabel *, 2> notifyLabels_{};
    QList<QWidget *> managedWidgets_;
    QThread workerThread_;
    DeviceWorker *worker_ = nullptr;
    bool hasDevicePorts_ = false;
    bool isConnected_ = false;
};

}  // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

#include "magicUI.moc"
