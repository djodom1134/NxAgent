// nx_agent_config.cpp
#include "nx_agent_config.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <nx/sdk/helpers/settings_response.h>
#include <nlohmann/json.hpp>

namespace nx_agent {

using json = nlohmann::json;

// DeviceConfig implementation
DeviceConfig::DeviceConfig(const std::string& deviceId) : deviceId(deviceId) {}

bool DeviceConfig::loadFromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        
        // Parse basic settings
        deviceName = j.value("deviceName", deviceName);
        minPersonConfidence = j.value("minPersonConfidence", minPersonConfidence);
        minVehicleConfidence = j.value("minVehicleConfidence", minVehicleConfidence);
        
        // Parse anomaly settings
        anomalyThreshold = j.value("anomalyThreshold", anomalyThreshold);
        enableUnknownVisitorDetection = j.value("enableUnknownVisitorDetection", enableUnknownVisitorDetection);
        unknownVisitorThresholdSecs = j.value("unknownVisitorThresholdSecs", unknownVisitorThresholdSecs);
        enableActivityAnalysis = j.value("enableActivityAnalysis", enableActivityAnalysis);
        
        // Parse learning settings
        enableLearning = j.value("enableLearning", enableLearning);
        baselineDurationDays = j.value("baselineDurationDays", baselineDurationDays);
        
        // Parse business hours
        businessHours.clear();
        if (j.contains("businessHours") && j["businessHours"].is_array()) {
            for (const auto& hour : j["businessHours"]) {
                if (hour.contains("start") && hour.contains("end")) {
                    TimeRange range;
                    range.startTime = hour["start"];
                    range.endTime = hour["end"];
                    businessHours.push_back(range);
                }
            }
        }
        
        // Parse detection regions
        detectionRegions.clear();
        if (j.contains("detectionRegions") && j["detectionRegions"].is_array()) {
            for (const auto& regionJson : j["detectionRegions"]) {
                Region region;
                region.name = regionJson.value("name", "");
                region.isExclusionZone = regionJson.value("isExclusionZone", false);
                
                if (regionJson.contains("points") && regionJson["points"].is_array()) {
                    for (const auto& pointJson : regionJson["points"]) {
                        if (pointJson.contains("x") && pointJson.contains("y")) {
                            float x = pointJson["x"];
                            float y = pointJson["y"];
                            region.points.push_back(std::make_pair(x, y));
                        }
                    }
                }
                
                if (!region.points.empty()) {
                    detectionRegions.push_back(region);
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DeviceConfig] Error parsing JSON: " << e.what() << std::endl;
        return false;
    }
}

std::string DeviceConfig::toJson() const {
    json j;
    
    // Basic settings
    j["deviceId"] = deviceId;
    j["deviceName"] = deviceName;
    j["minPersonConfidence"] = minPersonConfidence;
    j["minVehicleConfidence"] = minVehicleConfidence;
    
    // Anomaly settings
    j["anomalyThreshold"] = anomalyThreshold;
    j["enableUnknownVisitorDetection"] = enableUnknownVisitorDetection;
    j["unknownVisitorThresholdSecs"] = unknownVisitorThresholdSecs;
    j["enableActivityAnalysis"] = enableActivityAnalysis;
    
    // Learning settings
    j["enableLearning"] = enableLearning;
    j["baselineDurationDays"] = baselineDurationDays;
    
    // Business hours
    json hoursArray = json::array();
    for (const auto& range : businessHours) {
        json rangeJson;
        rangeJson["start"] = range.startTime;
        rangeJson["end"] = range.endTime;
        hoursArray.push_back(rangeJson);
    }
    j["businessHours"] = hoursArray;
    
    // Detection regions
    json regionsArray = json::array();
    for (const auto& region : detectionRegions) {
        json regionJson;
        regionJson["name"] = region.name;
        regionJson["isExclusionZone"] = region.isExclusionZone;
        
        json pointsArray = json::array();
        for (const auto& point : region.points) {
            json pointJson;
            pointJson["x"] = point.first;
            pointJson["y"] = point.second;
            pointsArray.push_back(pointJson);
        }
        
        regionJson["points"] = pointsArray;
        regionsArray.push_back(regionJson);
    }
    j["detectionRegions"] = regionsArray;
    
    return j.dump(4); // Pretty print with 4-space indent
}

// GlobalConfig implementation
GlobalConfig::GlobalConfig() {
    // Create default storage path if needed
    if (dataStoragePath.empty()) {
        // Use platform-specific paths
        #ifdef _WIN32
        dataStoragePath = "C:\\ProgramData\\NxAgent\\";
        #else
        dataStoragePath = "/var/lib/nx-agent/";
        #endif
    }
    
    // Load existing configuration if available
    try {
        std::filesystem::path configFile = std::filesystem::path(dataStoragePath) / "config.json";
        if (std::filesystem::exists(configFile)) {
            std::ifstream file(configFile);
            if (file.is_open()) {
                std::string jsonStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                loadFromJson(jsonStr);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[GlobalConfig] Error loading config: " << e.what() << std::endl;
    }
}

GlobalConfig::~GlobalConfig() {
    saveConfig();
}

GlobalConfig& GlobalConfig::instance() {
    static GlobalConfig instance;
    return instance;
}

bool GlobalConfig::loadFromJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        
        // Parse global settings
        dataStoragePath = j.value("dataStoragePath", dataStoragePath);
        maxStorageSizeMB = j.value("maxStorageSizeMB", maxStorageSizeMB);
        enableDiagnostics = j.value("enableDiagnostics", enableDiagnostics);
        diagnosticLogLevel = j.value("diagnosticLogLevel", diagnosticLogLevel);
        
        // Parse SIP settings
        enableSipIntegration = j.value("enableSipIntegration", enableSipIntegration);
        sipServer = j.value("sipServer", sipServer);
        sipUsername = j.value("sipUsername", sipUsername);
        sipPassword = j.value("sipPassword", sipPassword);
        alarmPhoneNumber = j.value("alarmPhoneNumber", alarmPhoneNumber);
        
        // Parse device configurations
        if (j.contains("devices") && j["devices"].is_array()) {
            std::lock_guard<std::mutex> lock(m_configMutex);
            for (const auto& deviceJson : j["devices"]) {
                if (deviceJson.contains("deviceId")) {
                    std::string deviceId = deviceJson["deviceId"];
                    auto config = std::make_shared<DeviceConfig>(deviceId);
                    config->loadFromJson(deviceJson.dump());
                    m_deviceConfigs[deviceId] = config;
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[GlobalConfig] Error parsing JSON: " << e.what() << std::endl;
        return false;
    }
}

std::shared_ptr<DeviceConfig> GlobalConfig::getDeviceConfig(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    
    auto it = m_deviceConfigs.find(deviceId);
    if (it != m_deviceConfigs.end()) {
        return it->second;
    }
    
    // Create new config if it doesn't exist
    auto config = std::make_shared<DeviceConfig>(deviceId);
    m_deviceConfigs[deviceId] = config;
    return config;
}

void GlobalConfig::updateDeviceConfig(const std::shared_ptr<DeviceConfig>& config) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_deviceConfigs[config->deviceId] = config;
}

bool GlobalConfig::saveConfig() {
    try {
        // Ensure directory exists
        std::filesystem::path dirPath(dataStoragePath);
        std::filesystem::create_directories(dirPath);
        
        // Create JSON structure
        json j;
        
        // Global settings
        j["dataStoragePath"] = dataStoragePath;
        j["maxStorageSizeMB"] = maxStorageSizeMB;
        j["enableDiagnostics"] = enableDiagnostics;
        j["diagnosticLogLevel"] = diagnosticLogLevel;
        
        // SIP settings
        j["enableSipIntegration"] = enableSipIntegration;
        j["sipServer"] = sipServer;
        j["sipUsername"] = sipUsername;
        j["sipPassword"] = sipPassword;
        j["alarmPhoneNumber"] = alarmPhoneNumber;
        
        // Device configurations
        json devicesArray = json::array();
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            for (const auto& pair : m_deviceConfigs) {
                json deviceJson = json::parse(pair.second->toJson());
                devicesArray.push_back(deviceJson);
            }
        }
        j["devices"] = devicesArray;
        
        // Write to file
        std::filesystem::path configFile = dirPath / "config.json";
        std::ofstream file(configFile);
        if (!file.is_open()) {
            std::cerr << "[GlobalConfig] Failed to open config file for writing" << std::endl;
            return false;
        }
        
        file << j.dump(4); // Pretty print with 4-space indent
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[GlobalConfig] Error saving config: " << e.what() << std::endl;
        return false;
    }
}

// Helper function to parse settings from Nx Meta
void parseSettings(const nx::sdk::ISettingsResponse* settings, std::shared_ptr<DeviceConfig> config) {
    if (!settings || !config)
        return;
    
    // Parse detection settings
    float minConfidence = settings->getFloat("minPersonConfidence", config->minPersonConfidence);
    config->minPersonConfidence = minConfidence;
    
    // Parse anomaly threshold
    float threshold = settings->getFloat("anomalyThreshold", config->anomalyThreshold);
    config->anomalyThreshold = threshold;
    
    // Parse learning settings
    bool learning = settings->getBool("enableLearning", config->enableLearning);
    config->enableLearning = learning;
    
    // Parse regions of interest (this is more complex and would need custom parsing)
    // This would typically involve parsing a JSON string from settings
    // For now, we'll leave it as a placeholder
    
    // Update the config in global settings
    GlobalConfig::instance().updateDeviceConfig(config);
}

} // namespace nx_agent
