// nx_agent_plugin.h
#pragma once

#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/device_agent.h>
#include <memory>
#include <mutex>

// Forward declarations
namespace nx_agent {
    class DeviceConfig;
    class MetadataAnalyzer;
    class AnomalyDetector;
    class ResponseProtocol;
    struct FrameAnalysisResult;
}

namespace nx_agent {

/**
 * Main plugin class that serves as an entry point for Nx Meta VMS.
 */
class NxAgentPlugin: public nx::sdk::analytics::Plugin {
public:
    NxAgentPlugin();
    virtual ~NxAgentPlugin() override;

protected:
    virtual nx::sdk::Result<nx::sdk::analytics::IEngine*> doCreateEngine() override;
};

/**
 * Engine class responsible for creating device agents and managing global plugin data.
 */
class NxAgentEngine: public nx::sdk::analytics::Engine {
public:
    NxAgentEngine(NxAgentPlugin* plugin);
    virtual ~NxAgentEngine() override;

    virtual std::string manifestString() const override;
    
protected:
    virtual nx::sdk::analytics::IDeviceAgent* doCreateDeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo) override;
        
private:
    // Mutex for thread-safe operations
    std::mutex m_engineMutex;
    
    // Track active device agents
    std::map<std::string, nx::sdk::analytics::IDeviceAgent*> m_deviceAgents;
};

/**
 * DeviceAgent class that processes video frames for a single camera.
 */
class NxAgentDeviceAgent: public nx::sdk::analytics::VideoFrameProcessingDeviceAgent {
public:
    NxAgentDeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~NxAgentDeviceAgent() override;

    virtual std::string manifestString() const override;
    
protected:
    virtual bool needUncompressedVideoFrame() const override { return true; }
    virtual bool needCompressedVideoFrame() const override { return false; }
    
    virtual nx::sdk::Result<void> doSetupAnalytics(
        const nx::sdk::analytics::SetupAnalyticsModel& setupAnalyticsModel) override;
        
    virtual nx::sdk::Result<nx::sdk::analytics::DetectionResult> processVideoFrame(
        const nx::sdk::analytics::VideoFrameProcessingRequest& request) override;
    
    // Generate an event for an anomaly
    virtual void generateAnomalyEvent(const FrameAnalysisResult& result);
    
    // Report detected objects to the VMS
    virtual void reportObjects(const FrameAnalysisResult& result);
        
protected:
    // Track device settings and state
    std::string m_deviceId;
    bool m_initialized = false;
    
    // Configuration
    std::shared_ptr<DeviceConfig> m_config;
    
    // Processing components
    std::unique_ptr<MetadataAnalyzer> m_metadataAnalyzer;
    std::unique_ptr<AnomalyDetector> m_anomalyDetector;
    std::unique_ptr<ResponseProtocol> m_responseProtocol;
    
    // State variables
    bool m_isLearningMode = true;
    int m_learningFrameCount = 0;
    int64_t m_lastAnomalyTimeUs = 0;
    
    // Statistics
    int m_processedFrameCount = 0;
    int m_anomalyCount = 0;
};

} // namespace nx_agent

// Plugin entry point function that Nx Meta will call
extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin();
