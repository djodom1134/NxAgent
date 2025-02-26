// nx_agent_metadata.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <opencv2/opencv.hpp>

namespace nx_agent {

// Forward declarations
class DeviceConfig;

/**
 * Represents a detected object with its metadata
 */
struct DetectedObject {
    std::string typeId;                   // Object type (person, vehicle, etc.)
    float confidence;                     // Detection confidence (0.0-1.0)
    cv::Rect boundingBox;                 // Object bounding box in pixel coordinates
    std::map<std::string, std::string> attributes; // Additional attributes
    int64_t timestampUs;                  // Detection timestamp in microseconds
    std::string trackId;                  // Tracking ID (if available)
    
    // Convert to Nx ObjectMetadata
    nx::sdk::analytics::ObjectMetadata toNxObjectMetadata() const;
};

/**
 * Represents detected motion in a frame
 */
struct MotionInfo {
    float overallMotionLevel;            // Overall motion level (0.0-1.0)
    cv::Mat motionMask;                  // Binary mask showing motion areas
    std::vector<cv::Point> motionCenters; // Centers of motion regions
    int64_t timestampUs;                 // Timestamp in microseconds
};

/**
 * Represents analysis results for a single frame
 */
struct FrameAnalysisResult {
    int64_t timestampUs;                 // Frame timestamp
    std::vector<DetectedObject> objects; // Detected objects
    MotionInfo motionInfo;               // Motion information
    float anomalyScore;                  // Overall anomaly score (0.0-1.0)
    std::string anomalyType;             // Type of anomaly (if any)
    std::string anomalyDescription;      // Human-readable description
    bool isAnomaly;                      // Whether this frame contains an anomaly
    
    // Default constructor
    FrameAnalysisResult() : 
        timestampUs(0), 
        anomalyScore(0.0f), 
        isAnomaly(false) {}
};

/**
 * Main class responsible for analyzing video frames and their metadata
 */
class MetadataAnalyzer {
public:
    MetadataAnalyzer(const std::string& deviceId);
    ~MetadataAnalyzer();
    
    // Configure the analyzer
    void configure(std::shared_ptr<DeviceConfig> config);
    
    // Process a video frame and return analysis results
    FrameAnalysisResult processFrame(
        const cv::Mat& frame, 
        int64_t timestampUs,
        const nx::sdk::analytics::IMetadataPacket* existingMetadata = nullptr);
    
    // Process existing metadata without a frame
    FrameAnalysisResult processMetadata(
        const nx::sdk::analytics::IMetadataPacket* metadata,
        int64_t timestampUs);
        
    // Check if a point is inside any region of interest
    bool isInRegionOfInterest(float x, float y) const;
    
private:
    // Device identification
    std::string m_deviceId;
    
    // Configuration
    std::shared_ptr<DeviceConfig> m_config;
    
    // Object detection components (to be expanded)
    // In a real implementation, this would integrate with detection libraries
    
    // Motion detection
    cv::Ptr<cv::BackgroundSubtractorMOG2> m_bgSubtractor;
    float m_motionThreshold;
    
    // Object tracking state
    std::map<std::string, std::chrono::time_point<std::chrono::system_clock>> m_unknownVisitorTracks;
    
    // Helper methods
    std::vector<DetectedObject> detectObjects(const cv::Mat& frame);
    MotionInfo detectMotion(const cv::Mat& frame);
    void analyzeSceneActivity(FrameAnalysisResult& result);
    float calculateAnomalyScore(const FrameAnalysisResult& result);
    bool detectUnknownVisitors(FrameAnalysisResult& result);
    bool detectAnomalousActivity(FrameAnalysisResult& result);
    
    // Convert normalized coordinates to pixel coordinates
    cv::Rect normalizedToPixelCoords(float x, float y, float width, float height, 
                                     int frameWidth, int frameHeight) const;
                                     
    // Extract objects from metadata packet
    std::vector<DetectedObject> extractObjectsFromMetadata(
        const nx::sdk::analytics::IMetadataPacket* metadata,
        int frameWidth, int frameHeight);
};

} // namespace nx_agent
