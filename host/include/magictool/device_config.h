#pragma once

#include <string>
#include <vector>

namespace magictool {

struct ConfiguredDevice {
    std::string name;
    std::string port;
};

struct ScannedDevicePort {
    std::string idName;
    std::string path;
};

struct DeviceConfig {
    std::vector<ConfiguredDevice> devices;
    std::string defaultName;
};

std::string ConfigDirectoryPath();
std::string ConfigFilePath();
bool LoadDeviceConfig(DeviceConfig *configOut, std::string *errorOut = nullptr);
bool SaveDeviceConfig(const DeviceConfig &config, std::string *errorOut = nullptr);
bool ResolveConfiguredDevicePort(const DeviceConfig &config,
                                 const std::string &deviceName,
                                 std::string *portOut,
                                 std::string *errorOut = nullptr);
bool ResolveDefaultDevicePort(const DeviceConfig &config,
                              std::string *portOut,
                              std::string *deviceNameOut = nullptr,
                              std::string *errorOut = nullptr);
bool SetDefaultDeviceName(const std::string &deviceName, std::string *errorOut = nullptr);
std::vector<ScannedDevicePort> ScanMagicToolSerialPorts();

}  // namespace magictool
