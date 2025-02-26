// nx_agent_metadata.cpp
#include "nx_agent_metadata.h"
#include "nx_agent_config.h"

#include <iostream>
#include <algorithm>
#include <ctime>

namespace nx_agent {

// DetectedObject methods
nx::sdk::analytics::ObjectMetadata DetectedObject::toNxObjectMetadata() const {
    nx::sdk::analytics::ObjectMetadata obj;
    obj.typeId = typeId;
    obj.trackId = trackId;
    
    // Set bounding box using Nx SDK format
    obj.setBoundingBox(
        boundingBox.x, 
        boundingBox.y, 
        boundingBox.width, 
        boundingBox.height
    );
    
    // Add confidence as an attribute
    obj.attributes()->addFloat("confidence", confidence);
    
    // Add other attributes
    for (const auto& attr : attributes) {
        // Determine type and add accordingly
        // This is a simple approach - you might need more sophisticated type detection
        try {
            float val = std::stof(attr.second);
            obj.attributes()->addFloat(attr.first, val);
        } catch (std::invalid_argument&) {
            // Not a float, add as string
            obj.attributes()->addString(attr.first, attr.second);
        }
    }
    
    return obj;
}

// MetadataAnalyzer implementation
MetadataAnalyzer::MetadataAnalyzer(const std::string& deviceId) 
    : m_deviceId(deviceId),
      m_motionThreshold(0.03f)
{
    // Initialize background subtractor for motion detection
    m_bgSubtractor = cv::createBackgroundSubtractorMOG2(500, 16, false);
    
    // Load configuration for this device
    m_config = GlobalConfig::instance().getDeviceConfig(deviceId);
}

MetadataAnalyzer::~MetadataAnalyzer() {
    // Clean up resources if needed
}

void MetadataAnalyzer::configure(std::shared_ptr<DeviceConfig> config) {
    m_config = config;
    
    // Update internal parameters based on config
    // For example, adjust motion threshold based on sensitivity
    m_motionThreshold = 0.01f + (1.0f - m_config->anomalyThreshold) * 0.1f;
}

FrameAnalysisResult MetadataAnalyzer::processFrame(
    const cv::Mat& frame, 
    int64_t timestampUs,
    const nx::sdk::analytics::IMetadataPacket* existingMetadata)
{
    FrameAnalysisResult result;
    result.timestampUs = timestampUs;
    
    // Detect motion in the frame
    result.motionInfo = detectMotion(frame);
    
    // If existing metadata is provided, extract objects from it
    if (existingMetadata) {
        result.objects = extractObjectsFromMetadata(existingMetadata, frame.cols, frame.rows);
    } else {
        // Otherwise, run our own object detection
        result.objects = detectObjects(frame);
    }
    
    // Analyze scene activity (people count, motion patterns, etc.)
    analyzeSceneActivity(result);
    
    // Calculate anomaly score
    result.anomalyScore = calculateAnomalyScore(result);
    
    // Detect specific anomaly types
    bool unknownVisitorAnomaly = detectUnknownVisitors(result);
    bool activityAnomaly = detectAnomalousActivity(result);
    
    // Determine if there's an anomaly and what type
    result.isAnomaly = unknownVisitorAnomaly || activityAnomaly || result.anomalyScore > m_config->anomalyThreshold;
    
    if (unknownVisitorAnomaly) {
        result.anomalyType = "UnknownVisitor";
        result.anomalyDescription = "Unknown visitor detected for extended period";
    } else if (activityAnomaly) {
        result.anomalyType = "AbnormalActivity";
        result.anomalyDescription = "Unusual activity pattern detected";
    } else if (result.anomalyScore > m_config->anomalyThreshold) {
        result.anomalyType = "GeneralAnomaly";
        result.anomalyDescription = "General unusual activity detected";
    }
    
    return result;
}

FrameAnalysisResult MetadataAnalyzer::processMetadata(
    const nx::sdk::analytics::IMetadataPacket* metadata,
    int64_t timestampUs)
{
    // Process metadata without a frame
    // This is useful when we only have metadata from another source
    FrameAnalysisResult result;
    result.timestampUs = timestampUs;
    
    // Default frame size (this would ideally come from device info)
    const int DEFAULT_WIDTH = 1920;
    const int DEFAULT_HEIGHT = 1080;
    
    // Extract objects from metadata
    result.objects = extractObjectsFromMetadata(metadata, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    
    // Set motion info to default values
    result.motionInfo.overallMotionLevel = 0.0f;
    
    // Analyze what we can without a frame
    analyzeSceneActivity(result);
    
    // Calculate anomaly score with limited information
    result.anomalyScore = calculateAnomalyScore(result);
    
    // Detect specific anomalies based on objects
    bool unknownVisitorAnomaly = detectUnknownVisitors(result);
    
    // Determine if there's an anomaly
    result.isAnomaly = unknownVisitorAnomaly || result.anomalyScore > m_config->anomalyThreshold;
    
    if (unknownVisitorAnomaly) {
        result.anomalyType = "UnknownVisitor";
        result.anomalyDescription = "Unknown visitor detected for extended period";
    } else if (result.anomalyScore > m_config->anomalyThreshold) {
        result.anomalyType = "GeneralAnomaly";
        result.anomalyDescription = "Unusual metadata patterns detected";
    }
    
    return result;
}

bool MetadataAnalyzer::isInRegionOfInterest(float x, float y) const {
    // If no regions defined, the entire frame is considered a region of interest
    if (m_config->detectionRegions.empty()) {
        return true;
    }
    
    // Check each region
    for (const auto& region : m_config->detectionRegions) {
        if (region.isExclusionZone) {
            continue; // Skip exclusion zones for now
        }
        
        if (region.points.size() < 3) {
            continue; // Need at least 3 points for a polygon
        }
        
        // Convert region points to format for cv::pointPolygonTest
        std::vector<cv::Point2f> contour;
        for (const auto& point : region.points) {
            contour.push_back(cv::Point2f(point.first, point.second));
        }
        
        // Check if point is inside the polygon
        if (cv::pointPolygonTest(contour, cv::Point2f(x, y), false) >= 0) {
            return true;
        }
    }
    
    // Check if point is outside any exclusion zones
    for (const auto& region : m_config->detectionRegions) {
        if (!region.isExclusionZone) {
            continue; // Skip inclusion zones
        }
        
        if (region.points.size() < 3) {
            continue; // Need at least 3 points for a polygon
        }
        
        // Convert region points to format for cv::pointPolygonTest
        std::vector<cv::Point2f> contour;
        for (const auto& point : region.points) {
            contour.push_back(cv::Point2f(point.first, point.second));
        }
        
        // If point is inside an exclusion zone, return false
        if (cv::pointPolygonTest(contour, cv::Point2f(x, y), false) >= 0) {
            return false;
        }
    }
    
    // If no specific regions defined, or point not in any region, 
    // depend on whether we have exclusion zones
    bool hasExclusionZones = false;
    for (const auto& region : m_config->detectionRegions) {
        if (region.isExclusionZone) {
            hasExclusionZones = true;
            break;
        }
    }
    
    // If we have exclusion zones and the point isn't in any inclusion zone
    // but also not in any exclusion zone, it's considered of interest.
    // If we only have inclusion zones and point isn't in any, it's not of interest.
    return hasExclusionZones;
}

// Private methods
std::vector<DetectedObject> MetadataAnalyzer::detectObjects(const cv::Mat& frame) {
    // In a real implementation, this would call an object detection model
    // For now, we'll simulate detections for testing
    
    std::vector<DetectedObject> objects;
    
    // For demonstration, randomly detect a person or vehicle
    // In real code, this would be replaced with actual detection logic
    if (std::rand() % 10 < 3) { // 30% chance to detect an object
        DetectedObject obj;
        
        // 70% chance for person, 30% for vehicle
        if (std::rand() % 10 < 7) {
            obj.typeId = "person";
            obj.confidence = 0.7f + (std::rand() % 300) / 1000.0f; // 0.7 - 0.99
            
            // Create a person-sized box in a random location
            int personWidth = frame.cols / 10;
            int personHeight = frame.rows / 4;
            int x = std::rand() % (frame.cols - personWidth);
            int y = std::rand() % (frame.rows - personHeight);
            
            obj.boundingBox = cv::Rect(x, y, personWidth, personHeight);
            
            // Add attributes
            obj.attributes["recognitionStatus"] = (std::rand() % 10 < 3) ? "known" : "unknown";
            
            // Generate a consistent tracking ID
            obj.trackId = "person_" + std::to_string(std::rand() % 10);
        } else {
            obj.typeId = "vehicle";
            obj.confidence = 0.75f + (std::rand() % 250) / 1000.0f; // 0.75 - 0.99
            
            // Create a vehicle-sized box
            int vehicleWidth = frame.cols / 5;
            int vehicleHeight = frame.rows / 6;
            int x = std::rand() % (frame.cols - vehicleWidth);
            int y = std::rand() % (frame.rows - vehicleHeight);
            
            obj.boundingBox = cv::Rect(x, y, vehicleWidth, vehicleHeight);
            
            // Add attributes
            obj.attributes["vehicleType"] = (std::rand() % 2 == 0) ? "car" : "truck";
            
            // Generate a consistent tracking ID
            obj.trackId = "vehicle_" + std::to_string(std::rand() % 5);
        }
        
        obj.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
            
        objects.push_back(obj);
    }
    
    return objects;
}

MotionInfo MetadataAnalyzer::detectMotion(const cv::Mat& frame) {
    MotionInfo info;
    info.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Convert frame to grayscale if needed
    cv::Mat gray;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame.clone();
    }
    
    // Apply background subtraction
    m_bgSubtractor->apply(gray, info.motionMask);
    
    // Calculate overall motion level
    info.overallMotionLevel = cv::countNonZero(info.motionMask) / 
                              static_cast<float>(info.motionMask.rows * info.motionMask.cols);
    
    // Find motion centers (contours)
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(info.motionMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    // Calculate centers of significant contours
    for (const auto& contour : contours) {
        // Filter small contours
        if (cv::contourArea(contour) < 100) {
            continue;
        }
        
        // Calculate center of mass
        cv::Moments moments = cv::moments(contour);
        if (moments.m00 != 0) {
            int cx = static_cast<int>(moments.m10 / moments.m00);
            int cy = static_cast<int>(moments.m01 / moments.m00);
            info.motionCenters.push_back(cv::Point(cx, cy));
        }
    }
    
    return info;
}

void MetadataAnalyzer::analyzeSceneActivity(FrameAnalysisResult& result) {
    // Count people and vehicles
    int personCount = 0;
    int vehicleCount = 0;
    
    for (const auto& obj : result.objects) {
        if (obj.typeId == "person") {
            personCount++;
        } else if (obj.typeId == "vehicle") {
            vehicleCount++;
        }
    }
    
    // Determine current time of day
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&time);
    
    int currentTimeOfDay = localTime->tm_hour * 3600 + localTime->tm_min * 60 + localTime->tm_sec;
    
    // Check if we're in business hours
    bool duringBusinessHours = false;
    for (const auto& timeRange : m_config->businessHours) {
        if (currentTimeOfDay >= timeRange.startTime && currentTimeOfDay <= timeRange.endTime) {
            duringBusinessHours = true;
            break;
        }
    }
    
    // In a real implementation, we would analyze the activity more deeply
    // For example, compare current activity to historical patterns for this time of day
    // For now, we'll use simple heuristics for testing
    
    // Example: After hours activity is unusual
    if (!duringBusinessHours && (personCount > 0 || result.motionInfo.overallMotionLevel > 0.05f)) {
        // Increase anomaly score based on activity level
        result.anomalyScore += 0.3f + result.motionInfo.overallMotionLevel;
    }
}

float MetadataAnalyzer::calculateAnomalyScore(const FrameAnalysisResult& result) {
    // In a real implementation, this would be a much more sophisticated calculation
    // possibly involving statistical models, machine learning, etc.
    // For now, we'll use a simple heuristic approach for testing
    
    float score = 0.0f;
    
    // Check motion level
    if (result.motionInfo.overallMotionLevel > m_motionThreshold) {
        score += result.motionInfo.overallMotionLevel * 0.5f;
    }
    
    // Check object counts
    int personCount = 0;
    int vehicleCount = 0;
    int unknownPersonCount = 0;
    
    for (const auto& obj : result.objects) {
        if (obj.typeId == "person") {
            personCount++;
            auto it = obj.attributes.find("recognitionStatus");
            if (it != obj.attributes.end() && it->second == "unknown") {
                unknownPersonCount++;
            }
        } else if (obj.typeId == "vehicle") {
            vehicleCount++;
        }
    }
    
    // Determine current time of day
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm* localTime = std::localtime(&time);
    
    int currentTimeOfDay = localTime->tm_hour * 3600 + localTime->tm_min * 60 + localTime->tm_sec;
    
    // Check if we're in business hours
    bool duringBusinessHours = false;
    for (const auto& timeRange : m_config->businessHours) {
        if (currentTimeOfDay >= timeRange.startTime && currentTimeOfDay <= timeRange.endTime) {
            duringBusinessHours = true;
            break;
        }
    }
    
    // After hours activity is more suspicious
    if (!duringBusinessHours) {
        score += personCount * 0.15f;
        score += vehicleCount * 0.1f;
    } else {
        // During business hours, only unknown persons are somewhat unusual
        score += unknownPersonCount * 0.05f;
    }
    
    // Cap the score at 1.0
    return std::min(score, 1.0f);
}

bool MetadataAnalyzer::detectUnknownVisitors(FrameAnalysisResult& result) {
    if (!m_config->enableUnknownVisitorDetection) {
        return false;
    }
    
    bool anomalyDetected = false;
    auto now = std::chrono::system_clock::now();
    
    // Process each person in the current frame
    for (const auto& obj : result.objects) {
        if (obj.typeId != "person" || obj.trackId.empty()) {
            continue;
        }
        
        // Check if this is an unknown person
        auto it = obj.attributes.find("recognitionStatus");
        if (it == obj.attributes.end() || it->second != "unknown") {
            continue;
        }
        
        // Check if we already have this person in our tracking map
        auto trackIt = m_unknownVisitorTracks.find(obj.trackId);
        if (trackIt == m_unknownVisitorTracks.end()) {
            // New unknown person, start tracking
            m_unknownVisitorTracks[obj.trackId] = now;
        } else {
            // Already tracking this person, check duration
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                now - trackIt->second).count();
            
            // If they've been present longer than the threshold, mark as anomaly
            if (duration > m_config->unknownVisitorThresholdSecs) {
                anomalyDetected = true;
                
                // Add duration as an attribute to the object
                result.objects[&obj - &result.objects[0]].attributes["durationSecs"] = 
                    std::to_string(duration);
            }
        }
    }
    
    // Clean up tracking for persons no longer in the frame
    auto it = m_unknownVisitorTracks.begin();
    while (it != m_unknownVisitorTracks.end()) {
        bool found = false;
        for (const auto& obj : result.objects) {
            if (obj.trackId == it->first) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            it = m_unknownVisitorTracks.erase(it);
        } else {
            ++it;
        }
    }
    
    return anomalyDetected;
}

bool MetadataAnalyzer::detectAnomalousActivity(FrameAnalysisResult& result) {
    if (!m_config->enableActivityAnalysis) {
        return false;
    }
    
    // In a real implementation, this would involve more sophisticated analysis
    // For example, comparing current activity patterns to historical baselines
    // or using machine learning to identify unusual patterns
    
    // For testing, we'll use a simple threshold on motion
    if (result.motionInfo.overallMotionLevel > 0.2f) {
        return true;
    }
    
    // An example of rule-based anomaly:
    // Detect multiple people moving in a restricted area
    
    int peopleInRestrictedAreas = 0;
    for (const auto& obj : result.objects) {
        if (obj.typeId != "person") {
            continue;
        }
        
        // Calculate center point of bounding box
        float centerX = obj.boundingBox.x + obj.boundingBox.width / 2.0f;
        float centerY = obj.boundingBox.y + obj.boundingBox.height / 2.0f;
        
        // Normalize to 0-1 range (assuming frame dimensions)
        // This is simplified - in a real implementation, you'd have the actual frame size
        float normalizedX = centerX / 1920.0f; // Assuming 1920x1080 for simplicity
        float normalizedY = centerY / 1080.0f;
        
        // Check if this point is in a region of interest
        if (!isInRegionOfInterest(normalizedX, normalizedY)) {
            peopleInRestrictedAreas++;
        }
    }
    
    // If multiple people are in restricted areas, that's unusual
    return peopleInRestrictedAreas >= 2;
}

cv::Rect MetadataAnalyzer::normalizedToPixelCoords(
    float x, float y, float width, float height, 
    int frameWidth, int frameHeight) const 
{
    return cv::Rect(
        static_cast<int>(x * frameWidth),
        static_cast<int>(y * frameHeight),
        static_cast<int>(width * frameWidth),
        static_cast<int>(height * frameHeight)
    );
}

std::vector<DetectedObject> MetadataAnalyzer::extractObjectsFromMetadata(
    const nx::sdk::analytics::IMetadataPacket* metadata,
    int frameWidth, int frameHeight)
{
    std::vector<DetectedObject> objects;
    
    if (!metadata) {
        return objects;
    }
    
    // Extract object metadata
    auto objectMetadata = nx::sdk::analytics::ObjectMetadata::fromMetadataPacket(metadata);
    if (objectMetadata) {
        for (int i = 0; i < objectMetadata->size(); ++i) {
            const auto& nxObj = objectMetadata->at(i);
            
            DetectedObject obj;
            obj.typeId = nxObj.typeId;
            obj.trackId = nxObj.trackId;
            obj.timestampUs = metadata->timestampUs();
            
            // Get bounding box
            float x, y, width, height;
            nxObj.boundingBox(&x, &y, &width, &height);
            obj.boundingBox = normalizedToPixelCoords(
                x, y, width, height, frameWidth, frameHeight);
            
            // Extract attributes
            if (nxObj.attributes()) {
                // Get confidence
                float confidence;
                if (nxObj.attributes()->getFloat("confidence", &confidence)) {
                    obj.confidence = confidence;
                } else {
                    obj.confidence = 1.0f; // Default if not specified
                }
                
                // Extract other attributes
                for (int j = 0; j < nxObj.attributes()->size(); ++j) {
                    std::string key = nxObj.attributes()->key(j);
                    std::string value;
                    // We just store everything as strings internally
                    nxObj.attributes()->getString(key, &value);
                    obj.attributes[key] = value;
                }
            }
            
            objects.push_back(obj);
        }
    }
    
    return objects;
}

} // namespace nx_agent
