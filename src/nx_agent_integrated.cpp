// nx_agent_integrated.cpp - Complete integrated implementation

#include "nx_agent_plugin.h"
#include "nx_agent_config.h"
#include "nx_agent_metadata.h"
#include "nx_agent_anomaly.h"
#include "nx_agent_response.h"
#include "nx_agent_utils.h"

#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/analytics/helpers/metadata_packet.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>

namespace nx_agent {

// Plugin Implementation
NxAgentPlugin::NxAgentPlugin():
    nx::sdk::analytics::Plugin(
        nx::sdk::UuidHelper::randomUuid(),
        "nx_agent",
        "NX Agent AI Security Guard",
        "1.0.0")
{
    Logger::info("NxAgentPlugin", "Initializing plugin");
    
    // Initialize global configuration
    auto& config = GlobalConfig::instance();
    
    // Set log level from global config
    Logger::setLogLevel(static_cast<Logger::Level>(config.diagnosticLogLevel));
}

NxAgentPlugin::~NxAgentPlugin() {
    Logger::info("NxAgentPlugin", "Destroying plugin");
}

nx::sdk::Result<nx::sdk::analytics::IEngine*> NxAgentPlugin::doCreateEngine() {
    return new NxAgentEngine(this);
}

// Engine Implementation
NxAgentEngine::NxAgentEngine(NxAgentPlugin* plugin):
    nx::sdk::analytics::Engine(plugin)
{
    Logger::info("NxAgentEngine", "Initializing engine");
}

NxAgentEngine::~NxAgentEngine() {
    Logger::info("NxAgentEngine", "Destroying engine");
    
    // Clean up any remaining device agents
    std::lock_guard<std::mutex> lock(m_engineMutex);
    for (auto& pair : m_deviceAgents) {
        // The SDK will actually destroy these, but we should remove from our map
        pair.second = nullptr;
    }
    m_deviceAgents.clear();
}

std::string NxAgentEngine::manifestString() const {
    // Define the engine capabilities and settings
    static const std::string manifest = R"json(
{
    "id": "nx_agent_engine",
    "name": "NX Agent Security Guard",
    "description": "AI-powered security monitoring system for anomaly detection",
    "version": "1.0.0",
    "vendor": "NX Agent",
    "engineSettingsModel": {
        "settings": [
            {
                "name": "sensitivityLevel",
                "type": "float",
                "defaultValue": 0.7,
                "description": "Anomaly detection sensitivity (0.0 - 1.0)"
            },
            {
                "name": "learningEnabled",
                "type": "boolean",
                "defaultValue": true,
                "description": "Enable continuous learning of normal patterns"
            },
            {
                "name": "enableSipIntegration",
                "type": "boolean",
                "defaultValue": false,
                "description": "Enable SIP phone call notifications"
            },
            {
                "name": "sipServer",
                "type": "string",
                "defaultValue": "",
                "description": "SIP server address for call notifications"
            },
            {
                "name": "sipUsername",
                "type": "string",
                "defaultValue": "",
                "description": "SIP account username"
            },
            {
                "name": "sipPassword",
                "type": "password",
                "defaultValue": "",
                "description": "SIP account password"
            },
            {
                "name": "alarmPhoneNumber",
                "type": "string",
                "defaultValue": "",
                "description": "Phone number to call for high-priority alerts"
            }
        ]
    }
}
)json";
    return manifest;
}

nx::sdk::analytics::IDeviceAgent* NxAgentEngine::doCreateDeviceAgent(
    const nx::sdk::IDeviceInfo* deviceInfo)
{
    std::string deviceId = deviceInfo->id();
    Logger::info("NxAgentEngine", "Creating device agent for " + deviceId);
    
    // Create new device agent
    nx::sdk::analytics::IDeviceAgent* agent = new NxAgentDeviceAgent(deviceInfo);
    
    // Track it in our map
    {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        m_deviceAgents[deviceId] = agent;
    }
    
    return agent;
}

// DeviceAgent Implementation
NxAgentDeviceAgent::NxAgentDeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    nx::sdk::analytics::VideoFrameProcessingDeviceAgent(deviceInfo),
    m_deviceId(deviceInfo->id()),
    m_initialized(false),
    m_isLearningMode(true),
    m_learningFrameCount(0),
    m_lastAnomalyTimeUs(0),
    m_processedFrameCount(0),
    m_anomalyCount(0)
{
    Logger::info("NxAgentDeviceAgent", "Initializing device agent for " + m_deviceId);
    
    // Load configuration for this device
    m_config = GlobalConfig::instance().getDeviceConfig(m_deviceId);
    
    // Initialize processing components
    m_metadataAnalyzer = std::make_unique<MetadataAnalyzer>(m_deviceId);
    m_anomalyDetector = std::make_unique<AnomalyDetector>(m_deviceId);
    m_responseProtocol = std::make_unique<ResponseProtocol>(m_deviceId);
    
    // Configure components with the device settings
    m_metadataAnalyzer->configure(m_config);
    m_anomalyDetector->configure(m_config);
    m_responseProtocol->configure(m_config);
    
    // Check if we're in learning mode
    // Try to load existing models - if none found, start in learning mode
    m_isLearningMode = !m_anomalyDetector->loadModel();
    
    // Set up response protocol to use our event generation
    m_responseProtocol->setNxEventCallback([this](const FrameAnalysisResult& result) {
        generateAnomalyEvent(result);
    });
    
    Logger::info("NxAgentDeviceAgent", "Device agent initialized in " + 
                 std::string(m_isLearningMode ? "learning" : "detection") + " mode");
}

NxAgentDeviceAgent::~NxAgentDeviceAgent() {
    Logger::info("NxAgentDeviceAgent", "Destroying device agent for " + m_deviceId);
    
    // Save the anomaly detection model when shutting down
    if (m_anomalyDetector) {
        m_anomalyDetector->saveModel();
    }
    
    // Log statistics
    Logger::info("NxAgentDeviceAgent", "Statistics: Processed " + 
                 std::to_string(m_processedFrameCount) + " frames, detected " +
                 std::to_string(m_anomalyCount) + " anomalies");
}

std::string NxAgentDeviceAgent::manifestString() const {
    // Define the device agent capabilities (events, objects, etc.)
    static const std::string manifest = R"json(
{
    "supportedMetadataTypes": [
        {
            "objectTypes": [
                {
                    "id": "person",
                    "name": "Person",
                    "attributes": [
                        {
                            "id": "confidence",
                            "name": "Confidence",
                            "type": "float"
                        },
                        {
                            "id": "recognitionStatus",
                            "name": "Recognition Status",
                            "type": "string"
                        },
                        {
                            "id": "durationSecs",
                            "name": "Duration (seconds)",
                            "type": "int"
                        }
                    ]
                },
                {
                    "id": "vehicle",
                    "name": "Vehicle",
                    "attributes": [
                        {
                            "id": "confidence",
                            "name": "Confidence",
                            "type": "float"
                        },
                        {
                            "id": "vehicleType",
                            "name": "Vehicle Type",
                            "type": "string"
                        }
                    ]
                }
            ],
            "eventTypes": [
                {
                    "id": "nx.agent.anomalyDetected",
                    "name": "Anomaly Detected",
                    "attributes": [
                        {
                            "id": "anomalyType",
                            "name": "Anomaly Type",
                            "type": "string"
                        },
                        {
                            "id": "anomalyScore",
                            "name": "Anomaly Score",
                            "type": "float"
                        }
                    ]
                },
                {
                    "id": "nx.agent.unknownVisitor",
                    "name": "Unknown Visitor",
                    "attributes": [
                        {
                            "id": "duration",
                            "name": "Duration",
                            "type": "float"
                        }
                    ]
                },
                {
                    "id": "nx.agent.abnormalActivity",
                    "name": "Abnormal Activity",
                    "attributes": [
                        {
                            "id": "activityType",
                            "name": "Activity Type",
                            "type": "string"
                        }
                    ]
                },
                {
                    "id": "nx.agent.statusEvent",
                    "name": "NX Agent Status",
                    "attributes": [
                        {
                            "id": "statusType",
                            "name": "Status Type",
                            "type": "string"
                        },
                        {
                            "id": "message",
                            "name": "Message",
                            "type": "string"
                        }
                    ]
                }
            ]
        }
    ],
    "deviceAgentSettingsModel": {
        "settings": [
            {
                "name": "detectionRegions",
                "type": "regionOfInterest",
                "defaultValue": [],
                "description": "Regions to monitor for activity"
            },
            {
                "name": "minPersonConfidence",
                "type": "float",
                "defaultValue": 0.6,
                "description": "Minimum confidence for person detection"
            },
            {
                "name": "anomalyThreshold",
                "type": "float",
                "defaultValue": 0.7,
                "description": "Threshold for anomaly detection (0.0-1.0)"
            },
            {
                "name": "enableUnknownVisitorDetection",
                "type": "boolean",
                "defaultValue": true,
                "description": "Detect unknown visitors lingering in the scene"
            },
            {
                "name": "unknownVisitorThresholdSecs",
                "type": "int",
                "defaultValue": 300,
                "description": "Time in seconds before an unknown visitor is considered suspicious"
            },
            {
                "name": "enableLearning",
                "type": "boolean",
                "defaultValue": true,
                "description": "Enable continuous learning and adaptation"
            },
            {
                "name": "businessHoursStart",
                "type": "int",
                "defaultValue": 28800,
                "description": "Business hours start time (seconds from midnight, default 8:00 AM)"
            },
            {
                "name": "businessHoursEnd",
                "type": "int",
                "defaultValue": 64800,
                "description": "Business hours end time (seconds from midnight, default 6:00 PM)"
            }
        ]
    }
}
)json";
    return manifest;
}

nx::sdk::Result<void> NxAgentDeviceAgent::doSetupAnalytics(
    const nx::sdk::analytics::SetupAnalyticsModel& setupAnalyticsModel)
{
    Logger::info("NxAgentDeviceAgent", "Setting up analytics for " + m_deviceId);
    
    TIME_SCOPE("SetupAnalytics");
    
    try {
        // Parse device settings
        if (setupAnalyticsModel.deviceAgent) {
            auto settingsResponse = setupAnalyticsModel.deviceAgent->settings();
            if (settingsResponse) {
                parseSettings(settingsResponse.get(), m_config);
                
                // Update components with new settings
                m_metadataAnalyzer->configure(m_config);
                m_anomalyDetector->configure(m_config);
                m_responseProtocol->configure(m_config);
                
                // Check learning mode setting
                bool enableLearning = settingsResponse->getBool("enableLearning", m_config->enableLearning);
                if (enableLearning != m_config->enableLearning) {
                    m_config->enableLearning = enableLearning;
                    GlobalConfig::instance().updateDeviceConfig(m_config);
                }
                
                // If learning is being turned off and we're in learning mode, attempt to finalize learning
                if (!enableLearning && m_isLearningMode) {
                    if (m_learningFrameCount > 100) { // Minimum frames needed to build a model
                        Logger::info("NxAgentDeviceAgent", "Learning disabled - finalizing model");
                        m_isLearningMode = false;
                        m_anomalyDetector->saveModel();
                    }
                }
                
                // Update business hours settings
                int businessStart = settingsResponse->getInt("businessHoursStart", m_config->businessHours[0].startTime);
                int businessEnd = settingsResponse->getInt("businessHoursEnd", m_config->businessHours[0].endTime);
                
                // Update if changed
                if (businessStart != m_config->businessHours[0].startTime || 
                    businessEnd != m_config->businessHours[0].endTime) {
                    
                    // Update config
                    m_config->businessHours[0].startTime = businessStart;
                    m_config->businessHours[0].endTime = businessEnd;
                    GlobalConfig::instance().updateDeviceConfig(m_config);
                    
                    Logger::info("NxAgentDeviceAgent", "Updated business hours: " + 
                                 std::to_string(businessStart / 3600) + ":" + 
                                 std::to_string((businessStart % 3600) / 60) + " to " +
                                 std::to_string(businessEnd / 3600) + ":" +
                                 std::to_string((businessEnd % 3600) / 60));
                }
            }
        }
        
        // Send a status event to indicate setup completed
        nx::sdk::analytics::EventMetadata statusEvent;
        statusEvent.typeId = "nx.agent.statusEvent";
        statusEvent.caption = "NX Agent Initialized";
        statusEvent.description = "NX Agent has been initialized and is " + 
                                  std::string(m_isLearningMode ? "learning" : "monitoring");
        
        statusEvent.attributes()->addString("statusType", "Initialization");
        statusEvent.attributes()->addString("message", m_isLearningMode ? 
                                           "Learning mode active" : "Monitoring mode active");
                                           
        auto eventPacket = nx::sdk::analytics::MetadataPacket::makeEventMetadataPacket(
            &statusEvent, TimeUtils::getCurrentTimestampUs());
            
        pushMetadataPacket(eventPacket.get());
        
        m_initialized = true;
        return nx::sdk::Result<void>::success();
        
    } catch (const std::exception& e) {
        Logger::error("NxAgentDeviceAgent", "Setup error: " + std::string(e.what()));
        return nx::sdk::Result<void>::error(nx::sdk::error::Error, e.what());
    }
}

nx::sdk::Result<nx::sdk::analytics::DetectionResult> NxAgentDeviceAgent::processVideoFrame(
    const nx::sdk::analytics::VideoFrameProcessingRequest& request)
{
    if (!m_initialized) {
        return nx::sdk::analytics::DetectionResult::error(
            nx::sdk::error::NotInitialized, "Analytics not initialized yet");
    }
    
    auto videoFrame = request.videoFrame();
    if (!videoFrame) {
        return nx::sdk::analytics::DetectionResult::error(
            nx::sdk::error::InvalidArgument, "Missing video frame data");
    }
    
    int64_t timestampUs = videoFrame->timestampUs();
    m_processedFrameCount++;
    
    try {
        TIME_SCOPE("ProcessFrame");
        
        // Convert Nx frame to OpenCV Mat
        cv::Mat frame = ImageUtils::nxFrameToMat(
            dynamic_cast<const nx::sdk::analytics::IUncompressedVideoFrame*>(videoFrame));
            
        if (frame.empty()) {
            Logger::warning("NxAgentDeviceAgent", "Failed to convert frame to OpenCV Mat");
            return nx::sdk::analytics::DetectionResult::error(
                nx::sdk::error::InvalidArgument, "Failed to process frame format");
        }
        
        // Process the frame
        FrameAnalysisResult result = m_metadataAnalyzer->processFrame(
            frame, timestampUs, request.compressionMetadata());
        
        // Always report detected objects regardless of mode
        reportObjects(result);
        
        // In learning mode, collect baseline data
        if (m_isLearningMode && m_config->enableLearning) {
            // Only use every 5th frame for training to avoid too much correlation
            if (m_learningFrameCount % 5 == 0) {
                m_anomalyDetector->addToBaseline(result);
            }
            
            m_learningFrameCount++;
            
            // After collecting enough frames, switch to detection mode
            if (m_learningFrameCount >= 1000) { // ~8-12 hours at 1 frame every 30 seconds
                m_isLearningMode = false;
                m_anomalyDetector->saveModel();
                
                Logger::info("NxAgentDeviceAgent", "Switching from learning to detection mode");
                
                // Send a status event to indicate mode change
                nx::sdk::analytics::EventMetadata statusEvent;
                statusEvent.typeId = "nx.agent.statusEvent";
                statusEvent.caption = "Learning Complete";
                statusEvent.description = "NX Agent has completed learning and is now in monitoring mode";
                statusEvent.attributes()->addString("statusType", "ModeChange");
                statusEvent.attributes()->addString("message", "Monitoring mode active");
                
                auto eventPacket = nx::sdk::analytics::MetadataPacket::makeEventMetadataPacket(
                    &statusEvent, timestampUs);
                pushMetadataPacket(eventPacket.get());
            }
            
            // In learning mode, periodically log progress
            if (m_learningFrameCount % 100 == 0) {
                Logger::info("NxAgentDeviceAgent", "Learning progress: " + 
                             std::to_string(m_learningFrameCount) + " frames collected");
            }
            
        } else {
            // In detection mode
            
            // Apply anomaly detection
            bool anomalyDetected = m_anomalyDetector->detectAnomaly(result);
            
            // Process potential anomaly through response protocol
            if (anomalyDetected) {
                bool responded = m_responseProtocol->processAnomaly(result);
                
                if (responded) {
                    m_anomalyCount++;
                    Logger::info("NxAgentDeviceAgent", "Anomaly detected and response triggered: " + 
                                 result.anomalyType + " (Score: " + std::to_string(result.anomalyScore) + ")");
                }
            }
            
            // Still collect data for continuous learning if enabled
            if (m_config->enableLearning && m_learningFrameCount % 20 == 0) {
                // Only add frames that are not anomalous to the baseline
                if (!result.isAnomaly) {
                    m_anomalyDetector->addToBaseline(result);
                }
            }
            
            m_learningFrameCount++;
            
            // Periodically save model to persist learning
            if (m_config->enableLearning && m_learningFrameCount % 500 == 0) {
                m_anomalyDetector->saveModel();
            }
        }
        
        return nx::sdk::analytics::DetectionResult::success();
        
    } catch (const std::exception& e) {
        Logger::error("NxAgentDeviceAgent", "Processing error: " + std::string(e.what()));
        return nx::sdk::analytics::DetectionResult::error(
            nx::sdk::error::Error, "Frame processing error: " + std::string(e.what()));
    }
}

void NxAgentDeviceAgent::generateAnomalyEvent(const FrameAnalysisResult& result) {
    try {
        nx::sdk::analytics::EventMetadata event;
        
        // Set event type based on anomaly type
        if (result.anomalyType == "UnknownVisitor") {
            event.typeId = "nx.agent.unknownVisitor";
            event.caption = "Unknown Visitor Detected";
        } else if (result.anomalyType == "AbnormalActivity") {
            event.typeId = "nx.agent.abnormalActivity";
            event.caption = "Abnormal Activity Detected";
        } else {
            event.typeId = "nx.agent.anomalyDetected";
            event.caption = "Anomaly Detected";
        }
        
        // Set description
        event.description = result.anomalyDescription.empty() ? 
            "Unusual activity detected by AI Security Guard" : result.anomalyDescription;
            
        // Add attributes
        event.attributes()->addString("anomalyType", result.anomalyType);
        event.attributes()->addFloat("anomalyScore", result.anomalyScore);
        
        // Add timestamp
        int64_t timestampUs = result.timestampUs;
        
        // Create and push event packet
        auto eventPacket = nx::sdk::analytics::MetadataPacket::makeEventMetadataPacket(
            &event, timestampUs);
        pushMetadataPacket(eventPacket.get());
        
        Logger::info("NxAgentDeviceAgent", "Generated anomaly event: " + 
                     event.typeId + " with score " + std::to_string(result.anomalyScore));
                     
        // Update last anomaly time
        m_lastAnomalyTimeUs = timestampUs;
    
    } catch (const std::exception& e) {
        Logger::error("NxAgentDeviceAgent", "Error generating event: " + std::string(e.what()));
    }
}

void NxAgentDeviceAgent::reportObjects(const FrameAnalysisResult& result) {
    try {
        // Convert detected objects to Nx metadata packets
        if (!result.objects.empty()) {
            std::vector<nx::sdk::analytics::ObjectMetadata> nxObjects;
            nxObjects.reserve(result.objects.size());
            
            for (const auto& obj : result.objects) {
                nxObjects.push_back(obj.toNxObjectMetadata());
            }
            
            // Create and push the metadata packet
            auto packet = nx::sdk::analytics::MetadataPacket::makeObjectMetadataPacket(
                nxObjects, result.timestampUs);
                
            pushMetadataPacket(packet.get());
        }
    } catch (const std::exception& e) {
        Logger::error("NxAgentDeviceAgent", "Error reporting objects: " + std::string(e.what()));
    }
}

// Plugin entry point implementation
extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin() {
    return new nx_agent::NxAgentPlugin();
}

} // namespace nx_agent
