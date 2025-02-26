// nx_agent_system.cpp - Main system integration implementation

#include "nx_agent_system.h"
#include "nx_agent_config.h"
#include "nx_agent_metadata.h"
#include "nx_agent_anomaly.h"
#include "nx_agent_response.h"
#include "nx_agent_llm.h"
#include "nx_agent_strategy.h"
#include "nx_agent_reasoning.h"
#include "nx_agent_utils.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <regex>

namespace nx_agent {

using json = nlohmann::json;

NxAgentSystem::NxAgentSystem(const std::string& systemId)
    : m_systemId(systemId),
      m_running(false),
      m_cognitiveInterval(std::chrono::milliseconds(30000)) // Default 30 seconds
{
}

NxAgentSystem::~NxAgentSystem() {
    stop();
}

bool NxAgentSystem::initialize(const json& config) {
    // Initialize LLM Manager
    std::string apiKey = "";
    std::string modelName = "claude-3-haiku-20240307";
    
    if (config.contains("llm")) {
        auto llmConfig = config["llm"];
        if (llmConfig.contains("apiKey")) {
            apiKey = llmConfig["apiKey"];
        }
        if (llmConfig.contains("modelName")) {
            modelName = llmConfig["modelName"];
        }
    }
    
    m_llmManager = std::make_shared<LLMManager>();
    if (!m_llmManager->initialize(apiKey, modelName)) {
        Logger::error("NxAgentSystem", "Failed to initialize LLM Manager");
        return false;
    }
    
    // Initialize ContextManager
    m_contextManager = std::make_shared<ContextManager>(m_systemId);
    
    // Initialize StrategyManager
    m_strategyManager = std::make_shared<StrategyManager>(m_systemId);
    m_strategyManager->initialize(m_llmManager);
    
    // Configure StrategyManager if camera information provided
    if (config.contains("cameras")) {
        m_strategyManager->configure(config);
    }
    
    // Initialize ReasoningSystem
    m_reasoningSystem = std::make_shared<ReasoningSystem>(m_systemId);
    m_reasoningSystem->initialize(m_llmManager, m_contextManager, m_strategyManager);
    
    // Configure cognitive cycle interval
    if (config.contains("cognitiveInterval")) {
        int intervalMs = config["cognitiveInterval"];
        if (intervalMs > 0) {
            m_cognitiveInterval = std::chrono::milliseconds(intervalMs);
        }
    }
    
    Logger::info("NxAgentSystem", "System initialized successfully");
    
    return true;
}

void NxAgentSystem::processFrame(const std::string& deviceId, 
                               const void* frameData, 
                               int width, 
                               int height, 
                               int64_t timestampUs) {
    // Create OpenCV Mat from frame data
    cv::Mat frame(height, width, CV_8UC3, const_cast<void*>(frameData));
    
    // Get camera components
    auto& components = getCameraComponents(deviceId);
    std::lock_guard<std::mutex> lock(components.mutex);
    
    try {
        // Process frame
        FrameAnalysisResult result = components.analyzer->processFrame(frame, timestampUs);
        
        // Detect anomalies
        bool anomalyDetected = components.detector->detectAnomaly(result);
        
        // Process through response protocol
        if (anomalyDetected) {
            bool responded = components.responseProtocol->processAnomaly(result);
            
            if (responded) {
                Logger::info("NxAgentSystem", "Anomaly detected and response triggered on camera " + deviceId);
            }
        }
        
        // Forward to strategy manager
        m_strategyManager->processAnalysisResult(deviceId, result);
        
        // Forward to reasoning system
        m_reasoningSystem->processAnalysisResult(deviceId, result);
        
        // Add to continuous learning if enabled
        DeviceConfig* config = GlobalConfig::instance().getDeviceConfig(deviceId).get();
        if (config && config->enableLearning && !result.isAnomaly) {
            components.detector->addToBaseline(result);
        }
    } catch (const std::exception& e) {
        Logger::error("NxAgentSystem", "Error processing frame: " + std::string(e.what()));
    }
}

void NxAgentSystem::processMetadata(const std::string& deviceId,
                                  const std::string& metadataType,
                                  const json& metadata,
                                  int64_t timestampUs) {
    // Get camera components
    auto& components = getCameraComponents(deviceId);
    std::lock_guard<std::mutex> lock(components.mutex);
    
    try {
        // Process metadata differently based on type
        if (metadataType == "motion") {
            // Process motion metadata
            FrameAnalysisResult result;
            result.timestampUs = timestampUs;
            
            // Extract motion level
            if (metadata.contains("level")) {
                result.motionInfo.overallMotionLevel = metadata["level"];
            }
            
            // Process through analyzer
            components.analyzer->processMetadata(nullptr, timestampUs);
            
            // Forward to strategy manager
            m_strategyManager->processAnalysisResult(deviceId, result);
            
            // Forward to reasoning system
            m_reasoningSystem->processAnalysisResult(deviceId, result);
        } else if (metadataType == "object") {
            // Process object metadata
            // In a real system, this would convert the object metadata
            // to the internal DetectedObject format
            
            // For now, just add information to context
            if (m_contextManager) {
                ContextItem item;
                item.type = ContextItem::Type::OBJECT_DETECTION;
                
                // Get object type and properties
                std::string objectType = "unknown";
                if (metadata.contains("type")) {
                    objectType = metadata["type"];
                }
                
                item.description = "Detected " + objectType + " from external metadata";
                item.timestampUs = timestampUs;
                item.confidence = metadata.contains("confidence") ? 
                                 metadata["confidence"].get<float>() : 0.8f;
                item.metadata = metadata;
                
                m_contextManager->addContextItem(item);
            }
        } else if (metadataType == "face") {
            // Process face recognition metadata
            // For now, just add to context
            if (m_contextManager) {
                ContextItem item;
                item.type = ContextItem::Type::OBJECT_DETECTION;
                
                // Get recognition status
                std::string recognitionStatus = "unknown";
                if (metadata.contains("recognized") && metadata["recognized"].get<bool>()) {
                    recognitionStatus = "known";
                }
                
                std::string name = metadata.contains("name") ? 
                                  metadata["name"].get<std::string>() : "unknown";
                                  
                item.description = "Face detected: " + name + " (" + recognitionStatus + ")";
                item.timestampUs = timestampUs;
                item.confidence = metadata.contains("confidence") ? 
                                 metadata["confidence"].get<float>() : 0.8f;
                item.metadata = metadata;
                
                m_contextManager->addContextItem(item);
            }
        } else if (metadataType == "analytics") {
            // Process generic analytics metadata
            // For now, just add to context
            if (m_contextManager) {
                ContextItem item;
                item.type = ContextItem::Type::ENVIRONMENT_INFO;
                
                item.description = "Analytics metadata received: " + metadata.dump().substr(0, 100);
                item.timestampUs = timestampUs;
                item.confidence = 0.9f;
                item.metadata = metadata;
                
                m_contextManager->addContextItem(item);
            }
        }
    } catch (const std::exception& e) {
        Logger::error("NxAgentSystem", "Error processing metadata: " + std::string(e.what()));
    }
}

std::string NxAgentSystem::getStatusReport() {
    std::ostringstream report;
    
    report << "NX Agent System Status Report\n";
    report << "===========================\n\n";
    
    // Add timestamp
    report << "Time: " << TimeUtils::formatTimestamp(TimeUtils::getCurrentTimestampUs()) << "\n\n";
    
    // Add system ID
    report << "System ID: " << m_systemId << "\n\n";
    
    // Add camera status
    report << "Cameras:\n";
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        for (const auto& pair : m_cameraComponents) {
            report << "- " << pair.first << "\n";
        }
    }
    report << "\n";
    
    // Add cognitive status if available
    if (m_reasoningSystem) {
        report << "Cognitive Status:\n";
        report << m_reasoningSystem->generateCognitiveStatus() << "\n\n";
    }
    
    // Add situation report if available
    if (m_strategyManager) {
        report << "Security Situation:\n";
        report << m_strategyManager->generateSituationReport() << "\n\n";
    }
    
    return report.str();
}

std::string NxAgentSystem::addGoal(const std::string& description, int priority) {
    if (!m_reasoningSystem) {
        return "";
    }
    
    return m_reasoningSystem->addGoal(
        Goal::Type::MONITOR, // Default type
        description,
        convertPriority(priority)
    );
}

std::string NxAgentSystem::queryKnowledge(const std::string& query) {
    if (!m_reasoningSystem) {
        return "Reasoning system not initialized";
    }
    
    std::vector<KnowledgeItem> results = m_reasoningSystem->queryKnowledge(query, 10);
    
    std::ostringstream oss;
    oss << "Knowledge query results for: " << query << "\n\n";
    
    if (results.empty()) {
        oss << "No results found.";
    } else {
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& item = results[i];
            oss << (i + 1) << ". " << item.content 
                << " (Confidence: " << item.confidence 
                << ", Source: " << item.source << ")\n";
        }
    }
    
    return oss.str();
}

void NxAgentSystem::start() {
    if (m_running) {
        return;
    }
    
    m_running = true;
    
    // Start background cognitive thread
    m_cognitiveThread = std::thread(&NxAgentSystem::cognitiveFunction, this);
    
    Logger::info("NxAgentSystem", "System started");
}

void NxAgentSystem::stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    // Stop cognitive thread
    if (m_cognitiveThread.joinable()) {
        m_cognitiveThread.join();
    }
    
    Logger::info("NxAgentSystem", "System stopped");
}

void NxAgentSystem::configureCamera(const std::string& deviceId, const json& config) {
    // Get camera components
    auto& components = getCameraComponents(deviceId);
    std::lock_guard<std::mutex> lock(components.mutex);
    
    // Get device config
    auto deviceConfig = GlobalConfig::instance().getDeviceConfig(deviceId);
    
    // Update configuration from JSON
    if (config.contains("minPersonConfidence")) {
        deviceConfig->minPersonConfidence = config["minPersonConfidence"];
    }
    
    if (config.contains("anomalyThreshold")) {
        deviceConfig->anomalyThreshold = config["anomalyThreshold"];
    }
    
    if (config.contains("enableUnknownVisitorDetection")) {
        deviceConfig->enableUnknownVisitorDetection = config["enableUnknownVisitorDetection"];
    }
    
    if (config.contains("unknownVisitorThresholdSecs")) {
        deviceConfig->unknownVisitorThresholdSecs = config["unknownVisitorThresholdSecs"];
    }
    
    if (config.contains("enableLearning")) {
        deviceConfig->enableLearning = config["enableLearning"];
    }
    
    if (config.contains("businessHours") && config["businessHours"].is_array()) {
        deviceConfig->businessHours.clear();
        
        for (const auto& hours : config["businessHours"]) {
            if (hours.contains("start") && hours.contains("end")) {
                DeviceConfig::TimeRange range;
                range.startTime = hours["start"];
                range.endTime = hours["end"];
                deviceConfig->businessHours.push_back(range);
            }
        }
    }
    
    // Update global config
    GlobalConfig::instance().updateDeviceConfig(deviceConfig);
    
    // Update components with new config
    components.analyzer->configure(deviceConfig);
    components.detector->configure(deviceConfig);
    components.responseProtocol->configure(deviceConfig);
    
    // Register camera with strategy manager if it has position information
    if (config.contains("position")) {
        CameraInfo cameraInfo;
        cameraInfo.deviceId = deviceId;
        cameraInfo.name = config.contains("name") ? config["name"] : deviceId;
        cameraInfo.location = config.contains("location") ? config["location"] : "";
        cameraInfo.isActive = true;
        
        // Parse position
        cameraInfo.position.x = config["position"].contains("x") ? config["position"]["x"] : 0.0f;
        cameraInfo.position.y = config["position"].contains("y") ? config["position"]["y"] : 0.0f;
        cameraInfo.position.z = config["position"].contains("z") ? config["position"]["z"] : 0.0f;
        
        // Parse adjacent cameras
        if (config.contains("adjacentCameras") && config["adjacentCameras"].is_array()) {
            for (const auto& adjCamera : config["adjacentCameras"]) {
                cameraInfo.adjacentCameras.insert(adjCamera);
            }
        }
        
        m_strategyManager->registerCamera(cameraInfo);
    }
    
    Logger::info("NxAgentSystem", "Camera " + deviceId + " configured");
}

std::string NxAgentSystem::generateCognitiveStatus() {
    if (!m_reasoningSystem) {
        return "Reasoning system not initialized";
    }
    
    return m_reasoningSystem->generateCognitiveStatus();
}

std::vector<std::string> NxAgentSystem::getSecurityRecommendations() {
    std::vector<std::string> recommendations;
    
    if (!m_reasoningSystem) {
        recommendations.push_back("Reasoning system not initialized");
        return recommendations;
    }
    
    // Query knowledge for recommendations
    std::vector<KnowledgeItem> results = m_reasoningSystem->queryKnowledge("recommend", 5);
    
    for (const auto& item : results) {
        recommendations.push_back(item.content);
    }
    
    // If no specific recommendations found, add defaults
    if (recommendations.empty()) {
        recommendations.push_back("Monitor all camera feeds for unusual activity");
        recommendations.push_back("Verify any detected anomalies");
        recommendations.push_back("Consider running system in learning mode to improve detection accuracy");
    }
    
    return recommendations;
}

void NxAgentSystem::cognitiveFunction() {
    while (m_running) {
        // Execute cognitive cycle
        if (m_reasoningSystem) {
            m_reasoningSystem->executeCognitiveCycle();
        }
        
        // Sleep for the configured interval
        std::this_thread::sleep_for(m_cognitiveInterval);
    }
}

NxAgentSystem::CameraComponents& NxAgentSystem::getCameraComponents(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_cameraMutex);
    
    auto it = m_cameraComponents.find(deviceId);
    if (it != m_cameraComponents.end()) {
        return it->second;
    }
    
    // Create new components
    CameraComponents components;
    components.analyzer = std::make_shared<MetadataAnalyzer>(deviceId);
    components.detector = std::make_shared<AnomalyDetector>(deviceId);
    components.responseProtocol = std::make_shared<ResponseProtocol>(deviceId);
    
    // Configure components with device config
    auto deviceConfig = GlobalConfig::instance().getDeviceConfig(deviceId);
    components.analyzer->configure(deviceConfig);
    components.detector->configure(deviceConfig);
    components.responseProtocol->configure(deviceConfig);
    
    // Set up response protocol to use our event generation from ReasoningSystem
    if (m_reasoningSystem) {
        components.responseProtocol->setNxEventCallback([this, deviceId](const FrameAnalysisResult& result) {
            // Forward the event to the reasoning system
            m_reasoningSystem->processAnalysisResult(deviceId, result);
        });
    }
    
    // Add to map
    m_cameraComponents[deviceId] = components;
    
    return m_cameraComponents[deviceId];
}

Goal::Priority NxAgentSystem::convertPriority(int priority) {
    if (priority >= 9) {
        return Goal::Priority::CRITICAL;
    } else if (priority >= 7) {
        return Goal::Priority::HIGH;
    } else if (priority >= 4) {
        return Goal::Priority::MEDIUM;
    } else if (priority >= 2) {
        return Goal::Priority::LOW;
    } else {
        return Goal::Priority::BACKGROUND;
    }
}

} // namespace nx_agent
