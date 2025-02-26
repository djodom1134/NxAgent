// nx_agent_plugin.cpp
#include "nx_agent_plugin.h"
#include "nx_agent_system.h"
#include "nx_agent_config.h"

#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/analytics/helpers/metadata_packet.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace nx_agent {

// Plugin Implementation
NxAgentPlugin::NxAgentPlugin():
    nx::sdk::analytics::Plugin(
        nx::sdk::UuidHelper::randomUuid(),
        "nx_agent",
        "NX Agent AI Security Guard",
        "1.0.0")
{
    std::cout << "[NxAgentPlugin] Initializing plugin" << std::endl;
}

NxAgentPlugin::~NxAgentPlugin() {
    std::cout << "[NxAgentPlugin] Destroying plugin" << std::endl;
}

nx::sdk::Result<nx::sdk::analytics::IEngine*> NxAgentPlugin::doCreateEngine() {
    return new NxAgentEngine(this);
}

// Engine Implementation
NxAgentEngine::NxAgentEngine(NxAgentPlugin* plugin):
    nx::sdk::analytics::Engine(plugin)
{
    std::cout << "[NxAgentEngine] Initializing engine" << std::endl;
}

NxAgentEngine::~NxAgentEngine() {
    std::cout << "[NxAgentEngine] Destroying engine" << std::endl;
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
    std::cout << "[NxAgentEngine] Creating device agent for " << deviceInfo->id() << std::endl;
    return new NxAgentDeviceAgent(deviceInfo);
}

// DeviceAgent Implementation
NxAgentDeviceAgent::NxAgentDeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    nx::sdk::analytics::VideoFrameProcessingDeviceAgent(deviceInfo),
    m_deviceId(deviceInfo->id())
{
    std::cout << "[NxAgentDeviceAgent] Initializing device agent for " << m_deviceId << std::endl;

    // Create and initialize the AI agent system
    m_system = std::make_shared<NxAgentSystem>("nx_plugin_" + std::string(id()));

    // Get global configuration
    auto& globalConfig = GlobalConfig::instance();

    // Create configuration for the AI system
    nlohmann::json config;

    // Configure LLM settings from global config
    config["llm"]["apiKey"] = globalConfig.llmApiKey;
    config["llm"]["modelName"] = globalConfig.llmModelName;
    config["llm"]["apiEndpoint"] = globalConfig.llmApiEndpoint;
    config["llm"]["maxTokens"] = globalConfig.llmMaxTokens;
    config["llm"]["temperature"] = globalConfig.llmTemperature;
    config["llm"]["timeoutSecs"] = globalConfig.llmRequestTimeoutSecs;
    config["llm"]["enabled"] = globalConfig.enableLLMIntegration;

    // Initialize and start the system
    m_system->initialize(config);

    // Only start if LLM integration is enabled
    if (globalConfig.enableLLMIntegration) {
        m_system->start();
        std::cout << "[NxAgentDeviceAgent] Started AI system with model: " << globalConfig.llmModelName << std::endl;
    } else {
        std::cout << "[NxAgentDeviceAgent] AI system initialized but not started (LLM integration disabled)" << std::endl;
    }
}

NxAgentDeviceAgent::~NxAgentDeviceAgent() {
    std::cout << "[NxAgentDeviceAgent] Destroying device agent for " << m_deviceId << std::endl;
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
    std::cout << "[NxAgentDeviceAgent] Setting up analytics for " << m_deviceId << std::endl;

    // Here we would initialize our analytics components
    // For now, just mark as initialized
    m_initialized = true;

    return nx::sdk::Result<void>::success();
}

nx::sdk::Result<nx::sdk::analytics::DetectionResult> NxAgentDeviceAgent::processVideoFrame(
    const nx::sdk::analytics::VideoFrameProcessingRequest& request)
{
    if (!m_initialized) {
        std::cout << "[NxAgentDeviceAgent] Warning: Processing frame before initialization" << std::endl;
        return nx::sdk::analytics::DetectionResult::error(
            nx::sdk::error::NotInitialized, "Analytics not initialized yet");
    }

    // For now, just log that we received a frame
    // In a real implementation, we would process the frame here

    auto timestamp = request.videoFrame()->timestampUs();
    auto videoFrame = request.videoFrame();

    // Forward frame to AI system for advanced processing
    if (m_system && GlobalConfig::instance().enableLLMIntegration) {
        try {
            // Process the frame with the AI system
            m_system->processFrame(
                m_deviceId,
                videoFrame->data(),
                videoFrame->width(),
                videoFrame->height(),
                timestamp
            );

            // Check if we need to run reasoning cycle
            auto& config = GlobalConfig::instance();
            if (config.enableLLMIntegration &&
                timestamp % (config.llmRequestTimeoutSecs * 1000000) < 100000) {
                // Generate cognitive status report periodically
                std::string status = m_system->generateCognitiveStatus();

                if (!status.empty()) {
                    // Log the cognitive status
                    std::cout << "[NxAgentDeviceAgent] Cognitive status: " << status << std::endl;

                    // Generate recommendations if available
                    auto recommendations = m_system->getSecurityRecommendations();
                    if (!recommendations.empty()) {
                        std::cout << "[NxAgentDeviceAgent] Security recommendations:" << std::endl;
                        for (const auto& rec : recommendations) {
                            std::cout << "  - " << rec << std::endl;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cout << "[NxAgentDeviceAgent] Error in AI processing: " << e.what() << std::endl;
        }
    }

    // Create a dummy object detection for testing
    if (timestamp % 10000000 == 0) { // Roughly every 10 seconds
        std::cout << "[NxAgentDeviceAgent] Processing frame at timestamp " << timestamp << std::endl;

        // Create a dummy person detection
        nx::sdk::analytics::ObjectMetadata person;
        person.typeId = "person";

        // Set a bounding box in the center of the frame
        float frameWidth = request.videoFrame()->width();
        float frameHeight = request.videoFrame()->height();
        float boxWidth = frameWidth * 0.2f;  // 20% of frame width
        float boxHeight = frameHeight * 0.4f; // 40% of frame height
        float x = (frameWidth - boxWidth) / 2;
        float y = (frameHeight - boxHeight) / 2;

        person.setBoundingBox(x, y, boxWidth, boxHeight);

        // Add confidence attribute
        person.attributes()->addFloat("confidence", 0.85f);
        person.attributes()->addString("recognitionStatus", "unknown");

        // Create a metadata packet with this object
        auto packet = nx::sdk::analytics::MetadataPacket::makeObjectMetadataPacket(
            { &person }, timestamp);

        // Push the metadata packet to the system
        pushMetadataPacket(packet.get());

        // Also generate an anomaly event every 30 seconds
        if (timestamp % 30000000 == 0) {
            nx::sdk::analytics::EventMetadata event;
            event.typeId = "nx.agent.anomalyDetected";
            event.caption = "Anomaly Detected";
            event.description = "Unusual activity detected in camera";

            // Add attributes
            event.attributes()->addString("anomalyType", "UnknownPerson");
            event.attributes()->addFloat("anomalyScore", 0.75f);

            // Create a metadata packet with this event
            auto eventPacket = nx::sdk::analytics::MetadataPacket::makeEventMetadataPacket(
                &event, timestamp);

            // Push the event metadata packet to the system
            pushMetadataPacket(eventPacket.get());

            std::cout << "[NxAgentDeviceAgent] Generated anomaly event at " << timestamp << std::endl;
        }
    }

    return nx::sdk::analytics::DetectionResult::success();
}

} // namespace nx_agent

// Plugin entry point implementation
extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin() {
    return new nx_agent::NxAgentPlugin();
}
