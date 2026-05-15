#include "magictool/device_config.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

namespace magictool {
namespace {

constexpr const char *kMagicToolIdSubstring = "usb-magictool_w_GPIO_Debug_Tool";
constexpr const char *kConfigSubdir = ".config/magictool";
constexpr const char *kConfigFilename = "config.json";

void SetError(std::string *errorOut, const std::string &message) {
    if (errorOut) {
        *errorOut = message;
    }
}

std::string HomeDirectory() {
    const char *home = std::getenv("HOME");
    return home ? std::string(home) : std::string();
}

bool ValidateDeviceConfig(const DeviceConfig &config, std::string *errorOut) {
    std::set<std::string> names;
    for (const ConfiguredDevice &device : config.devices) {
        if (device.name.empty()) {
            SetError(errorOut, "Configured device name must not be empty");
            return false;
        }
        if (device.port.empty()) {
            SetError(errorOut, "Configured device '" + device.name + "' has an empty port");
            return false;
        }
        if (!names.insert(device.name).second) {
            SetError(errorOut, "Duplicate configured device name '" + device.name + "'");
            return false;
        }
    }

    if (!config.defaultName.empty()) {
        const auto it = std::find_if(config.devices.begin(), config.devices.end(),
            [&config](const ConfiguredDevice &device) {
                return device.name == config.defaultName;
            });
        if (it == config.devices.end()) {
            SetError(errorOut, "Default device '" + config.defaultName + "' is not listed in devices");
            return false;
        }
    }

    return true;
}

}  // namespace

std::string ConfigDirectoryPath() {
    const std::string home = HomeDirectory();
    if (home.empty()) {
        return std::string();
    }
    return (std::filesystem::path(home) / kConfigSubdir).string();
}

std::string ConfigFilePath() {
    const std::string configDir = ConfigDirectoryPath();
    if (configDir.empty()) {
        return std::string();
    }
    return (std::filesystem::path(configDir) / kConfigFilename).string();
}

bool LoadDeviceConfig(DeviceConfig *configOut, std::string *errorOut) {
    if (!configOut) {
        SetError(errorOut, "configOut must not be null");
        return false;
    }

    configOut->devices.clear();
    configOut->defaultName.clear();

    const std::string configPath = ConfigFilePath();
    if (configPath.empty()) {
        SetError(errorOut, "HOME is not set; cannot locate magictool config");
        return false;
    }

    if (!std::filesystem::exists(configPath)) {
        return true;
    }

    std::ifstream input(configPath);
    if (!input) {
        SetError(errorOut, "Failed to open " + configPath);
        return false;
    }

    try {
        const nlohmann::json root = nlohmann::json::parse(input);
        if (!root.is_object()) {
            SetError(errorOut, "Config root must be a JSON object");
            return false;
        }

        if (root.contains("default")) {
            configOut->defaultName = root.at("default").get<std::string>();
        }

        if (root.contains("devices")) {
            const nlohmann::json &devices = root.at("devices");
            if (!devices.is_array()) {
                SetError(errorOut, "Config field 'devices' must be an array");
                return false;
            }

            for (const nlohmann::json &entry : devices) {
                ConfiguredDevice device;
                device.name = entry.at("name").get<std::string>();
                device.port = entry.at("port").get<std::string>();
                configOut->devices.push_back(device);
            }
        }
    } catch (const std::exception &ex) {
        SetError(errorOut, "Failed to parse " + configPath + ": " + ex.what());
        return false;
    }

    return ValidateDeviceConfig(*configOut, errorOut);
}

bool SaveDeviceConfig(const DeviceConfig &config, std::string *errorOut) {
    if (!ValidateDeviceConfig(config, errorOut)) {
        return false;
    }

    const std::string configDir = ConfigDirectoryPath();
    const std::string configPath = ConfigFilePath();
    if (configDir.empty() || configPath.empty()) {
        SetError(errorOut, "HOME is not set; cannot locate magictool config");
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);
    if (ec) {
        SetError(errorOut, "Failed to create " + configDir + ": " + ec.message());
        return false;
    }

    nlohmann::json root;
    root["devices"] = nlohmann::json::array();
    for (const ConfiguredDevice &device : config.devices) {
        root["devices"].push_back({
            {"name", device.name},
            {"port", device.port},
        });
    }
    root["default"] = config.defaultName;

    std::ofstream output(configPath);
    if (!output) {
        SetError(errorOut, "Failed to write " + configPath);
        return false;
    }

    output << root.dump(2) << '\n';
    return true;
}

bool ResolveConfiguredDevicePort(const DeviceConfig &config,
                                 const std::string &deviceName,
                                 std::string *portOut,
                                 std::string *errorOut) {
    if (deviceName.empty()) {
        SetError(errorOut, "Device name must not be empty");
        return false;
    }

    const auto it = std::find_if(config.devices.begin(), config.devices.end(),
        [&deviceName](const ConfiguredDevice &device) {
            return device.name == deviceName;
        });
    if (it == config.devices.end()) {
        SetError(errorOut, "Unknown configured device '" + deviceName + "'");
        return false;
    }

    if (portOut) {
        *portOut = it->port;
    }
    return true;
}

bool ResolveDefaultDevicePort(const DeviceConfig &config,
                              std::string *portOut,
                              std::string *deviceNameOut,
                              std::string *errorOut) {
    if (config.defaultName.empty()) {
        SetError(errorOut, "No default device is configured in " + ConfigFilePath());
        return false;
    }

    if (deviceNameOut) {
        *deviceNameOut = config.defaultName;
    }
    return ResolveConfiguredDevicePort(config, config.defaultName, portOut, errorOut);
}

bool SetDefaultDeviceName(const std::string &deviceName, std::string *errorOut) {
    DeviceConfig config;
    if (!LoadDeviceConfig(&config, errorOut)) {
        return false;
    }

    std::string ignoredPort;
    if (!ResolveConfiguredDevicePort(config, deviceName, &ignoredPort, errorOut)) {
        return false;
    }

    config.defaultName = deviceName;
    return SaveDeviceConfig(config, errorOut);
}

std::vector<ScannedDevicePort> ScanMagicToolSerialPorts() {
    std::vector<ScannedDevicePort> ports;
    const std::filesystem::path serialById("/dev/serial/by-id");
    std::error_code ec;
    if (!std::filesystem::exists(serialById, ec)) {
        return ports;
    }

    for (const std::filesystem::directory_entry &entry :
         std::filesystem::directory_iterator(serialById, ec)) {
        if (ec) {
            break;
        }

        const std::string idName = entry.path().filename().string();
        if (idName.find(kMagicToolIdSubstring) == std::string::npos) {
            continue;
        }

        ports.push_back(ScannedDevicePort{
            idName,
            entry.path().string(),
        });
    }

    std::sort(ports.begin(), ports.end(),
        [](const ScannedDevicePort &a, const ScannedDevicePort &b) {
            return a.idName < b.idName;
        });
    return ports;
}

}  // namespace magictool
