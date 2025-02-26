// nx_agent_main.cpp - Updated implementation with integration of all components

#include "nx_agent_plugin.h"
#include "nx_agent_config.h"
#include "nx_agent_metadata.h"
#include "nx_agent_anomaly.h"

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

// Update DeviceAgent implementation to integrate all components
class NxAgentDeviceAgentImpl : public NxAgentDeviceAgent {
public:
    NxAgentDeviceAgentImpl(const nx::sdk::IDeviceInfo* deviceInfo)
        : NxAgentDeviceAgent(deviceInfo),
          m_deviceId(deviceInfo->id()),
          m_metadataAnalyzer(deviceInfo->id()),
          m_anomalyDetector(deviceInfo->id()),
          m_isLearningMode(true),
          m_learningFrameCount(0),
          m_lastAnomalyTimeUs(0)
    {
        // Load configuration
        m_config = GlobalConfig::instance().getDeviceConfig(m_deviceId);
        
        // Configure components
        m_metadataAnalyzer.configure(m_config);
        m_anomalyDetector.configure(m_config);
        
        // Check if we're in learning mode
        m_isLearningMode = !m_anomalyDetector.loadModel();
        
        std::cout << "[NxAgentDeviceAgent] " << m_deviceId 
                  << " initialized in " << (m_isLearningMode ? "learning" : "detection") 
                  << " mode" << std::endl;
    }
    
    virtual ~NxAgentDeviceAgentImpl() override {
        // Save models on shutdown
        m_anomalyDetector.saveModel();
    }
    
protected:
    nx::sdk::Result<void> doSetupAnalytics(
        const nx::sdk::analytics::SetupAnalyticsModel& setupAnalyticsModel) override
    {
        std::cout << "[NxAgentDeviceAgent] Setting up analytics for " << m_deviceId << std::endl;
        
        // Parse device settings
        if (setupAnalyticsModel.deviceAgent) {
            auto settingsResponse = setupAnalyticsModel.deviceAgent->settings();
            if (settingsResponse) {
                parseSettings(settingsResponse.get(), m_config);
                
                // Update components with new settings
                m_metadataAnalyzer.configure(m_config);
                m_anomalyDetector.configure(m_config);
                
                // Check learning mode setting
                bool enableLearning = settingsResponse->getBool("enableLearning", m_config->enableLearning);
                if (enableLearning != m_config->enableLearning) {
                    m_config->enableLearning = enableLearning;
                    GlobalConfig::instance().updateDeviceConfig(m_config);
                }
            }
        }
        
        m_initialized = true;
        return nx::sdk::Result<void>::success();
    }
    
    nx::sdk::Result<nx::sdk::analytics::DetectionResult> processVideoFrame(
        const nx::sdk::analytics::VideoFrameProcessingRequest& request) override
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
        
        // Convert Nx frame to OpenCV Mat
        cv::Mat frame(
            videoFrame->height(),
            videoFrame->width(),
            CV_8UC3,
            const_cast<void*>(videoFrame->data())
        );
        
        // Process the frame
        FrameAnalysisResult result = m_metadataAnalyzer.processFrame(
            frame, timestampUs, request.compressionMetadata());
        
        // In learning mode, collect baseline data
        if (m_isLearningMode && m_config->enableLearning) {
            // Only use every 5th frame for training to avoid too much correlation
            if (m_learningFrameCount % 5 == 0) {
                m_anomalyDetector.addToBaseline(result);
            }
            
            m_learningFrameCount++;
            
            // After collecting enough frames, switch to detection mode
            if (m_learningFrameCount >= 1000) { // ~8-12 hours at 1 frame every 30 seconds
                m_isLearningMode = false;
                std::cout << "[NxAgentDeviceAgent] " << m_deviceId 
                          << " switching from learning to detection mode" << std::endl;
            }
        } else {
            // In detection mode, check for anomalies
            bool anomalyDetected = m_anomalyDetector.detectAnomaly(result);
            
            // If an anomaly is detected and it's been a while since the last one,
            // generate an event
            if (anomalyDetected && 
                (timestampUs - m_lastAnomalyTimeUs > 10000000)) { // 10 seconds cooldown
                
                generateAnomalyEvent(result);
                m_lastAnomalyTimeUs = timestampUs;
            }
            
            // Still collect data for continuous learning if enabled
            if (m_config->enableLearning && m_learningFrameCount % 20 == 0) {
                // Only add frames that are not anomalous to the baseline
                if (!result.isAnomaly) {
                    m_anomalyDetector.addToBaseline(result);
                }
            }
            
            m_learningFrameCount++;
        }
        
        // Always report the detected objects
        reportObjects(result);
        
        return nx::sdk::analytics::DetectionResult::success();
    }
    
private:
    // Device information
    std::string m_deviceId;
    std::shared_ptr<DeviceConfig> m_config;
    
    // Processing components
    MetadataAnalyzer m_metadataAnalyzer;
    AnomalyDetector m_anomalyDetector;
    
    // State tracking
    bool m_initialized = false;
    bool m_isLearningMode;
    int m_learningFrameCount;
    int64_t m_lastAnomalyTimeUs;
    
    // For tracking unknown visitors
    std::mutex m_visitorMutex;
    std::map<std::string, int64_t> m_unknownVisitors;
    
    void reportObjects(const FrameAnalysisResult& result) {
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
    }
    
    void generateAnomalyEvent(const FrameAnalysisResult& result) {
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
        
        std::cout << "[NxAgentDeviceAgent] Generated anomaly event: " 
                  << event.typeId << " with score " << result.anomalyScore 
                  << " at " << timestampUs << std::endl;
    }
};

// Update Engine's doCreateDeviceAgent to use our implementation
nx::sdk::analytics::IDeviceAgent* NxAgentEngine::doCreateDeviceAgent(
    const nx::sdk::IDeviceInfo* deviceInfo)
{
    std::cout << "[NxAgentEngine] Creating device agent for " << deviceInfo->id() << std::endl;
    return new NxAgentDeviceAgentImpl(deviceInfo);
}

} // namespace nx_agent
