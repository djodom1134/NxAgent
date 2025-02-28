// nx_agent_llm.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <future>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace nx_agent {

// Forward declarations
class DeviceConfig;
struct FrameAnalysisResult;
struct DetectedObject;

/**
 * Represents a context item for LLM reasoning
 */
struct ContextItem {
    enum class Type {
        OBJECT_DETECTION,    // Object detected in the scene
        MOTION_EVENT,        // Motion detected
        ANOMALY_DETECTION,   // Anomaly detected
        ENVIRONMENT_INFO,    // Information about the environment (time, location, etc.)
        HISTORICAL_PATTERN,  // Information about historical patterns
        CROSS_CAMERA_INFO,   // Information from other cameras
        SYSTEM_EVENT         // System events or status
    };
    
    Type type;
    std::string description;
    int64_t timestampUs;
    float confidence;
    nlohmann::json metadata;
    
    // Create from detected object
    static ContextItem fromDetectedObject(const DetectedObject& obj);
    
    // Create from analysis result
    static ContextItem fromAnalysisResult(const FrameAnalysisResult& result);
    
    // Convert to string representation for LLM
    std::string toString() const;
};

/**
 * Represents a request to the LLM system
 */
struct LLMRequest {
    enum class Priority {
        LOW,
        MEDIUM,
        HIGH,
        CRITICAL
    };
    
    enum class Type {
        ANOMALY_ANALYSIS,     // Analyze anomaly and provide context
        SITUATION_ASSESSMENT, // Assess overall situation
        RESPONSE_PLANNING,    // Plan a response to a situation
        PREDICTIVE_ANALYSIS,  // Predict future behavior
        CROSS_CAMERA_ANALYSIS // Analyze information from multiple cameras
    };
    
    std::string deviceId;
    Type requestType;
    Priority priority;
    std::vector<ContextItem> contextItems;
    int64_t requestTimeUs;
    std::function<void(const std::string&)> callback;
    
    // Constructor
    LLMRequest(const std::string& deviceId, Type type, Priority priority = Priority::MEDIUM);
    
    // Add context item
    void addContextItem(const ContextItem& item);
    
    // Generate prompt for LLM
    std::string generatePrompt() const;
};

/**
 * Represents a response from the LLM system
 */
struct LLMResponse {
    struct Action {
        enum class Type {
            MONITOR,          // Continue monitoring
            ALERT,            // Generate an alert
            TRACK,            // Track a specific object
            ANALYZE_FURTHER,  // Request additional analysis
            CROSS_REFERENCE,  // Cross-reference with other data
            PREDICT,          // Make a prediction
            RECOMMEND         // Recommend an action to human operators
        };
        
        Type type;
        std::string description;
        float confidence;
        nlohmann::json parameters;
    };
    
    std::string requestId;
    std::string reasoning;
    std::vector<Action> actions;
    float confidenceScore;
    int64_t responseTimeUs;
    bool success;
    std::string errorMessage;
    
    // Parse from LLM output
    static LLMResponse parseFromLLM(const std::string& llmOutput);
};

/**
 * Manages LLM integration for advanced reasoning
 */
class LLMManager {
public:
    LLMManager();
    ~LLMManager();
    
    // Initialize the LLM manager
    bool initialize(const std::string& apiKey, const std::string& modelName);
    
    // Submit a request to the LLM
    std::future<LLMResponse> submitRequest(const LLMRequest& request);
    
    // Submit a request with callback
    void submitRequestWithCallback(LLMRequest request);
    
    // Process a frame analysis result
    LLMResponse processAnalysisResult(const std::string& deviceId, const FrameAnalysisResult& result);
    
    // Generate a response plan for an anomaly
    LLMResponse generateResponsePlan(const std::string& deviceId, const FrameAnalysisResult& result);
    
    // Predict subject behavior based on recent history
    LLMResponse predictBehavior(const std::string& deviceId, const std::vector<FrameAnalysisResult>& history);
    
    // Start and stop the LLM worker thread
    void start();
    void stop();
    
    // Configure the LLM manager
    void configure(const nlohmann::json& config);
    
private:
    // LLM configuration
    std::string m_apiKey;
    std::string m_modelName;
    std::string m_apiEndpoint;
    int m_maxTokens;
    float m_temperature;
    
    // Request queue and processing
    std::queue<LLMRequest> m_requestQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    bool m_running;
    std::thread m_workerThread;
    
    // LLM worker thread function
    void workerFunction();
    
    // Send a request to the LLM
    std::string sendRequest(const std::string& prompt);
    
    // Make HTTP request
    std::string makeHttpRequest(const std::string& url, const std::string& data);
    
    // Parse and process LLM response
    LLMResponse processLLMResponse(const std::string& response, const LLMRequest& request);
    
    // Generate system prompt based on request type
    std::string generateSystemPrompt(const LLMRequest::Type& requestType) const;
};

/**
 * Context manager for building and maintaining context for LLM reasoning
 */
class ContextManager {
public:
    ContextManager(const std::string& deviceId);
    
    // Add context item
    void addContextItem(const ContextItem& item);
    
    // Get recent context items
    std::vector<ContextItem> getRecentContext(int maxItems = 10) const;
    
    // Get context for a specific time range
    std::vector<ContextItem> getContextForTimeRange(int64_t startTimeUs, int64_t endTimeUs) const;
    
    // Get context for a specific object
    std::vector<ContextItem> getContextForObject(const std::string& objectId) const;
    
    // Clear old context
    void clearOldContext(int64_t olderThanUs);
    
private:
    std::string m_deviceId;
    std::vector<ContextItem> m_contextItems;
    std::mutex m_contextMutex;
    
    // Maximum number of context items to keep
    const int MAX_CONTEXT_ITEMS = 1000;
};

} // namespace nx_agent
