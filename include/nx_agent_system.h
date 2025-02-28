// nx_agent_system.h - Main system integration header

#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>

namespace nx_agent {

// Forward declarations
class LLMManager;
class ContextManager;
class StrategyManager;
class ReasoningSystem;
class MetadataAnalyzer;
class AnomalyDetector;
class ResponseProtocol;
struct FrameAnalysisResult;
struct Goal;

/**
 * Main system class that integrates all components of the NX Agent system
 */
class NxAgentSystem {
public:
    NxAgentSystem(const std::string& systemId);
    ~NxAgentSystem();
    
    // Initialize the system
    bool initialize(const nlohmann::json& config);
    
    // Process a video frame from a camera
    void processFrame(const std::string& deviceId, 
                     const void* frameData, 
                     int width, 
                     int height, 
                     int64_t timestampUs);
    
    // Process a metadata packet from the VMS
    void processMetadata(const std::string& deviceId,
                        const std::string& metadataType,
                        const nlohmann::json& metadata,
                        int64_t timestampUs);
    
    // Get a status report from the system
    std::string getStatusReport();
    
    // Add a goal to the system
    std::string addGoal(const std::string& description, int priority = 5);
    
    // Query the system's knowledge base
    std::string queryKnowledge(const std::string& query);
    
    // Start/stop the system
    void start();
    void stop();
    
    // Configure a camera
    void configureCamera(const std::string& deviceId, const nlohmann::json& config);
    
    // Generate a cognitive status summary
    std::string generateCognitiveStatus();
    
    // Get security recommendations
    std::vector<std::string> getSecurityRecommendations();
    
private:
    // System identification
    std::string m_systemId;
    std::atomic<bool> m_running;
    
    // Core components
    std::shared_ptr<LLMManager> m_llmManager;
    std::shared_ptr<ContextManager> m_contextManager;
    std::shared_ptr<StrategyManager> m_strategyManager;
    std::shared_ptr<ReasoningSystem> m_reasoningSystem;
    
    // Component instances for each camera
    struct CameraComponents {
        std::shared_ptr<MetadataAnalyzer> analyzer;
        std::shared_ptr<AnomalyDetector> detector;
        std::shared_ptr<ResponseProtocol> responseProtocol;
        std::mutex mutex;
    };
    
    std::map<std::string, CameraComponents> m_cameraComponents;
    std::mutex m_cameraMutex;
    
    // Background threads
    std::thread m_cognitiveThread;
    std::chrono::milliseconds m_cognitiveInterval;
    
    // Cognitive cycle function
    void cognitiveFunction();
    
    // Get or create camera components
    CameraComponents& getCameraComponents(const std::string& deviceId);
    
    // Convert priority to Goal::Priority
    Goal::Priority convertPriority(int priority);
};

} // namespace nx_agent
