// nx_agent_response.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <chrono>

namespace nx_agent {

// Forward declarations
class DeviceConfig;
struct FrameAnalysisResult;

/**
 * Represents a notification or action to be taken in response to an anomaly
 */
struct ResponseAction {
    enum class Type {
        LOG_ONLY,         // Just log the event, no external action
        NX_EVENT,         // Generate Nx event (handled automatically by plugin)
        HTTP_REQUEST,     // Make HTTP request to external system
        SIP_CALL,         // Make a SIP phone call (if enabled)
        EXECUTE_COMMAND   // Execute a local command/script
    };
    
    Type type = Type::LOG_ONLY;
    std::string name;
    std::string description;
    std::string target;    // URL, phone number, or command based on type
    std::string payload;   // Additional data to include
    int priority = 0;      // Higher values = higher priority
    int cooldownMs = 60000; // Minimum time between repeating this action
    
    // Last time this action was triggered (to enforce cooldown)
    std::chrono::system_clock::time_point lastTriggeredTime;
};

/**
 * Manages the verification and response to detected anomalies
 */
class ResponseProtocol {
public:
    ResponseProtocol(const std::string& deviceId);
    ~ResponseProtocol();
    
    // Configure the protocol
    void configure(std::shared_ptr<DeviceConfig> config);
    
    // Process an anomaly detection result and execute appropriate responses
    bool processAnomaly(const FrameAnalysisResult& result);
    
    // Add a custom response action for a specific anomaly type
    void addResponseAction(const std::string& anomalyType, const ResponseAction& action);
    
    // Remove a response action
    void removeResponseAction(const std::string& anomalyType, const std::string& actionName);
    
    // Register a function to be called when triggering an NX event
    // This allows the plugin to integrate with Nx's event system
    using NxEventCallback = std::function<void(const FrameAnalysisResult&)>;
    void setNxEventCallback(NxEventCallback callback);
    
private:
    // Device identification
    std::string m_deviceId;
    
    // Configuration
    std::shared_ptr<DeviceConfig> m_config;
    
    // Response actions for different anomaly types
    std::map<std::string, std::vector<ResponseAction>> m_responseActions;
    
    // Tracking anomalies for verification
    struct AnomalyTracker {
        std::string anomalyType;
        float initialScore;
        int consecutiveDetections = 1;
        std::chrono::system_clock::time_point firstDetectedTime;
        std::chrono::system_clock::time_point lastDetectedTime;
        bool verified = false;
        bool responded = false;
    };
    
    std::mutex m_anomalyMutex;
    std::map<std::string, AnomalyTracker> m_activeAnomalies;
    
    // Callback for NX events
    NxEventCallback m_nxEventCallback;
    
    // Helper methods
    bool verifyAnomaly(const FrameAnalysisResult& result, AnomalyTracker& tracker);
    void triggerResponses(const FrameAnalysisResult& result, const AnomalyTracker& tracker);
    bool executeAction(const ResponseAction& action, const FrameAnalysisResult& result);
    
    // HTTP client for external notifications
    bool sendHttpRequest(const std::string& url, const std::string& payload);
    
    // SIP call functionality (if enabled)
    bool makeSipCall(const std::string& number, const std::string& message);
    
    // Command execution
    bool executeCommand(const std::string& command);
    
    // Clean up old anomaly trackers
    void cleanupAnomalies();
};

} // namespace nx_agent
