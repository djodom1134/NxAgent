// nx_agent_config.h
#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <memory>

namespace nx_agent {

/**
 * Represents a region of interest in the video frame.
 */
struct Region {
    std::vector<std::pair<float, float>> points; // Normalized coordinates (0.0-1.0)
    std::string name;
    bool isExclusionZone = false;
};

/**
 * Configuration for a single camera/device.
 */
class DeviceConfig {
public:
    DeviceConfig(const std::string& deviceId);

    // Camera identification
    std::string deviceId;
    std::string deviceName;

    // Detection settings
    float minPersonConfidence = 0.6f;
    float minVehicleConfidence = 0.6f;
    std::vector<Region> detectionRegions;

    // Anomaly detection settings
    float anomalyThreshold = 0.7f;
    bool enableUnknownVisitorDetection = true;
    int unknownVisitorThresholdSecs = 300; // 5 minutes
    bool enableActivityAnalysis = true;

    // Learning settings
    bool enableLearning = true;
    int baselineDurationDays = 7;

    // AI reasoning settings
    bool enableAIReasoning = true;
    float reasoningConfidenceThreshold = 0.65f;
    int reasoningInterval = 60; // seconds between reasoning cycles
    bool enableCrossCameraAnalysis = true;

    // Schedule settings (in seconds from midnight)
    struct TimeRange {
        int startTime; // Seconds from midnight
        int endTime;   // Seconds from midnight
    };

    std::vector<TimeRange> businessHours = {
        {8 * 3600, 18 * 3600} // 8 AM to 6 PM by default
    };

    // Load settings from JSON string
    bool loadFromJson(const std::string& jsonStr);

    // Serialize to JSON string
    std::string toJson() const;
};

/**
 * Global configuration for the entire plugin.
 */
class GlobalConfig {
public:
    static GlobalConfig& instance();

    // Global settings
    std::string dataStoragePath = "";
    int maxStorageSizeMB = 1024;
    bool enableDiagnostics = true;
    int diagnosticLogLevel = 2; // 0=off, 1=error, 2=warn, 3=info, 4=debug

    // SIP/notification settings
    bool enableSipIntegration = false;
    std::string sipServer = "";
    std::string sipUsername = "";
    std::string sipPassword = "";
    std::string alarmPhoneNumber = "";

    // LLM integration settings
    bool enableLLMIntegration = true;
    std::string llmApiKey = "";
    std::string llmModelName = "claude-3-haiku-20240307";
    std::string llmApiEndpoint = "https://api.anthropic.com/v1/messages";
    int llmMaxTokens = 4096;
    float llmTemperature = 0.7f;
    int llmRequestTimeoutSecs = 30;

    // Load global settings from JSON
    bool loadFromJson(const std::string& jsonStr);

    // Get device configuration (creates if doesn't exist)
    std::shared_ptr<DeviceConfig> getDeviceConfig(const std::string& deviceId);

    // Save all configurations
    bool saveConfig();

    // Update a device's configuration
    void updateDeviceConfig(const std::shared_ptr<DeviceConfig>& config);

private:
    GlobalConfig();
    ~GlobalConfig();

    // Device-specific configurations
    std::map<std::string, std::shared_ptr<DeviceConfig>> m_deviceConfigs;
    std::mutex m_configMutex;

    // Private implementation to prevent copying
    GlobalConfig(const GlobalConfig&) = delete;
    GlobalConfig& operator=(const GlobalConfig&) = delete;
};

// Helper to parse settings from Nx Meta settings model
void parseSettings(const nx::sdk::ISettingsResponse* settings, std::shared_ptr<DeviceConfig> config);

} // namespace nx_agent
