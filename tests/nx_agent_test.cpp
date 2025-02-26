// tests/nx_agent_test.cpp - Test framework for NX Agent plugin

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>

// Include plugin components
#include "../nx_agent_config.h"
#include "../nx_agent_metadata.h"
#include "../nx_agent_anomaly.h"
#include "../nx_agent_response.h"
#include "../nx_agent_utils.h"

using namespace nx_agent;

// Mock class for testing - mimics Nx SDK interfaces but doesn't require actual SDK
namespace mock {

class MockFrame {
public:
    MockFrame(const cv::Mat& image, int64_t timestamp)
        : m_image(image.clone()), m_timestamp(timestamp) {}
    
    int width() const { return m_image.cols; }
    int height() const { return m_image.rows; }
    int64_t timestampUs() const { return m_timestamp; }
    const void* data() const { return m_image.data; }
    
private:
    cv::Mat m_image;
    int64_t m_timestamp;
};

// Helper to load a test video or image sequence
class TestVideoProvider {
public:
    TestVideoProvider(const std::string& source) {
        // Check if source is a video file or image sequence pattern
        if (source.find('%') != std::string::npos) {
            // Image sequence (e.g., "frames/frame_%04d.jpg")
            m_isImageSequence = true;
            m_imagePattern = source;
            m_frameIndex = 0;
        } else {
            // Video file
            m_isImageSequence = false;
            m_cap.open(source);
            if (!m_cap.isOpened()) {
                throw std::runtime_error("Failed to open video: " + source);
            }
        }
    }
    
    bool getNextFrame(cv::Mat& frame, int64_t& timestampUs) {
        if (m_isImageSequence) {
            // Load next image in sequence
            std::string filename = StringUtils::format(m_imagePattern.c_str(), m_frameIndex++);
            frame = cv::imread(filename);
            if (frame.empty()) {
                return false; // End of sequence
            }
        } else {
            // Get next video frame
            if (!m_cap.read(frame)) {
                return false; // End of video
            }
        }
        
        // Generate timestamp (simulate 30fps)
        int64_t frameTime = m_frameIndex * (1000000 / 30); // microseconds
        timestampUs = m_startTimestampUs + frameTime;
        
        return true;
    }
    
    void reset() {
        if (m_isImageSequence) {
            m_frameIndex = 0;
        } else {
            m_cap.set(cv::CAP_PROP_POS_FRAMES, 0);
        }
        m_startTimestampUs = TimeUtils::getCurrentTimestampUs();
    }
    
private:
    bool m_isImageSequence;
    std::string m_imagePattern;
    cv::VideoCapture m_cap;
    int m_frameIndex = 0;
    int64_t m_startTimestampUs = TimeUtils::getCurrentTimestampUs();
};

// Collects events and objects for verification
class OutputCollector {
public:
    void addEvent(const std::string& eventType, const std::string& description, 
                 float anomalyScore, int64_t timestampUs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back({eventType, description, anomalyScore, timestampUs});
    }
    
    void addObject(const std::string& objectType, float confidence, 
                  const cv::Rect& boundingBox, int64_t timestampUs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_objects.push_back({objectType, confidence, boundingBox, timestampUs});
    }
    
    struct Event {
        std::string type;
        std::string description;
        float anomalyScore;
        int64_t timestampUs;
    };
    
    struct Object {
        std::string type;
        float confidence;
        cv::Rect boundingBox;
        int64_t timestampUs;
    };
    
    const std::vector<Event>& getEvents() const { return m_events; }
    const std::vector<Object>& getObjects() const { return m_objects; }
    
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.clear();
        m_objects.clear();
    }
    
    // Print summary of collected outputs
    void printSummary() const {
        std::cout << "---- Output Summary ----" << std::endl;
        std::cout << "Total events: " << m_events.size() << std::endl;
        std::cout << "Total objects: " << m_objects.size() << std::endl;
        
        // Count by type
        std::map<std::string, int> eventTypeCounts;
        std::map<std::string, int> objectTypeCounts;
        
        for (const auto& event : m_events) {
            eventTypeCounts[event.type]++;
        }
        
        for (const auto& obj : m_objects) {
            objectTypeCounts[obj.type]++;
        }
        
        std::cout << "Events by type:" << std::endl;
        for (const auto& pair : eventTypeCounts) {
            std::cout << "  " << pair.first << ": " << pair.second << std::endl;
        }
        
        std::cout << "Objects by type:" << std::endl;
        for (const auto& pair : objectTypeCounts) {
            std::cout << "  " << pair.first << ": " << pair.second << std::endl;
        }
    }
    
private:
    std::vector<Event> m_events;
    std::vector<Object> m_objects;
    std::mutex m_mutex;
};

} // namespace mock

// Test scenarios
void runBasicTest() {
    std::cout << "=== Running Basic Test ===" << std::endl;
    
    // Create test components
    std::string deviceId = "test_camera_01";
    auto config = GlobalConfig::instance().getDeviceConfig(deviceId);
    
    // Set test configuration
    config->anomalyThreshold = 0.6f;
    config->enableLearning = true;
    config->minPersonConfidence = 0.5f;
    
    // Create components
    MetadataAnalyzer analyzer(deviceId);
    AnomalyDetector detector(deviceId);
    ResponseProtocol response(deviceId);
    
    // Configure components
    analyzer.configure(config);
    detector.configure(config);
    response.configure(config);
    
    // Create output collector
    mock::OutputCollector collector;
    
    // Set response callback
    response.setNxEventCallback([&collector](const FrameAnalysisResult& result) {
        collector.addEvent(result.anomalyType, result.anomalyDescription, 
                          result.anomalyScore, result.timestampUs);
    });
    
    // Create test video source
    try {
        mock::TestVideoProvider videoProvider("test_data/test_video.mp4");
        
        // Process frames in learning mode first
        std::cout << "Starting learning phase..." << std::endl;
        int frameCount = 0;
        int learningFrames = 100; // Use small number for test
        
        while (frameCount < learningFrames) {
            cv::Mat frame;
            int64_t timestampUs;
            
            if (!videoProvider.getNextFrame(frame, timestampUs)) {
                videoProvider.reset();
                continue;
            }
            
            // Process frame
            FrameAnalysisResult result = analyzer.processFrame(frame, timestampUs);
            
            // Add to baseline
            detector.addToBaseline(result);
            
            // Track objects
            for (const auto& obj : result.objects) {
                collector.addObject(obj.typeId, obj.confidence, obj.boundingBox, timestampUs);
            }
            
            frameCount++;
            if (frameCount % 10 == 0) {
                std::cout << "Processed " << frameCount << " frames for learning" << std::endl;
            }
            
            // Small delay to not overwhelm the system
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        std::cout << "Learning phase complete. Starting detection phase..." << std::endl;
        
        // Save and reload model (test persistence)
        detector.saveModel();
        detector.loadModel();
        
        // Reset video and process again in detection mode
        videoProvider.reset();
        collector.clear();
        frameCount = 0;
        int testFrames = 200; // Process more frames to trigger anomalies
        
        while (frameCount < testFrames) {
            cv::Mat frame;
            int64_t timestampUs;
            
            if (!videoProvider.getNextFrame(frame, timestampUs)) {
                videoProvider.reset();
                continue;
            }
            
            // Process frame
            FrameAnalysisResult result = analyzer.processFrame(frame, timestampUs);
            
            // Check for anomalies
            bool anomalyDetected = detector.detectAnomaly(result);
            
            // Process through response protocol
            if (anomalyDetected) {
                bool responded = response.processAnomaly(result);
                if (responded) {
                    std::cout << "Anomaly detected and response triggered: " 
                              << result.anomalyType << " (Score: " 
                              << result.anomalyScore << ")" << std::endl;
                }
            }
            
            // Track objects
            for (const auto& obj : result.objects) {
                collector.addObject(obj.typeId, obj.confidence, obj.boundingBox, timestampUs);
            }
            
            frameCount++;
            if (frameCount % 20 == 0) {
                std::cout << "Processed " << frameCount << " frames for detection" << std::endl;
            }
            
            // Small delay
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Print summary
        collector.printSummary();
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
    }
}

void runUnknownVisitorTest() {
    std::cout << "=== Running Unknown Visitor Test ===" << std::endl;
    
    // Create test components
    std::string deviceId = "test_camera_02";
    auto config = GlobalConfig::instance().getDeviceConfig(deviceId);
    
    // Set test configuration
    config->anomalyThreshold = 0.5f;
    config->enableLearning = false; // Skip learning phase for this test
    config->enableUnknownVisitorDetection = true;
    config->unknownVisitorThresholdSecs = 5; // Short for testing
    
    // Create components
    MetadataAnalyzer analyzer(deviceId);
    AnomalyDetector detector(deviceId);
    ResponseProtocol response(deviceId);
    
    // Configure components
    analyzer.configure(config);
    detector.configure(config);
    response.configure(config);
    
    // Create output collector
    mock::OutputCollector collector;
    
    // Set response callback
    response.setNxEventCallback([&collector](const FrameAnalysisResult& result) {
        collector.addEvent(result.anomalyType, result.anomalyDescription, 
                          result.anomalyScore, result.timestampUs);
    });
    
    // Simulate an unknown visitor scenario
    // We'll create synthetic frames with an unknown person staying in the scene
    
    cv::Mat backgroundFrame(720, 1280, CV_8UC3, cv::Scalar(200, 200, 200)); // Gray background
    
    std::cout << "Simulating an unknown person scenario..." << std::endl;
    
    int frameCount = 0;
    int totalFrames = 30; // 30 frames at simulated 1 fps = 30 seconds
    int64_t startTime = TimeUtils::getCurrentTimestampUs();
    
    for (frameCount = 0; frameCount < totalFrames; frameCount++) {
        // Create a copy of the background
        cv::Mat frame = backgroundFrame.clone();
        
        // Add timestamp
        int64_t timestampUs = startTime + frameCount * 1000000; // 1 second between frames
        
        // Create a simulated frame with a synthetic detection
        FrameAnalysisResult result;
        result.timestampUs = timestampUs;
        
        // Add a person detection
        DetectedObject person;
        person.typeId = "person";
        person.confidence = 0.85f;
        person.boundingBox = cv::Rect(500, 200, 100, 300); // Fixed position to simulate staying
        person.timestampUs = timestampUs;
        person.trackId = "unknown_person_01"; // Same ID throughout to simulate persistence
        person.attributes["recognitionStatus"] = "unknown";
        
        result.objects.push_back(person);
        
        // Add a known person for contrast (appears briefly)
        if (frameCount < 5) {
            DetectedObject knownPerson;
            knownPerson.typeId = "person";
            knownPerson.confidence = 0.9f;
            knownPerson.boundingBox = cv::Rect(300, 220, 90, 250);
            knownPerson.timestampUs = timestampUs;
            knownPerson.trackId = "known_person_01";
            knownPerson.attributes["recognitionStatus"] = "known";
            
            result.objects.push_back(knownPerson);
        }
        
        // Process the frame
        bool anomalyDetected = detector.detectAnomaly(result);
        
        // Process through response protocol
        if (anomalyDetected) {
            bool responded = response.processAnomaly(result);
            if (responded) {
                std::cout << "Frame " << frameCount << ": Anomaly detected and response triggered: " 
                          << result.anomalyType << " (Score: " 
                          << result.anomalyScore << ")" << std::endl;
            }
        }
        
        // Track objects
        for (const auto& obj : result.objects) {
            collector.addObject(obj.typeId, obj.confidence, obj.boundingBox, timestampUs);
        }
        
        // Small delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Print summary
    collector.printSummary();
}

int main(int argc, char** argv) {
    // Set up logging
    Logger::setLogLevel(Logger::Level::DEBUG);
    
    // Run tests
    try {
        runBasicTest();
        runUnknownVisitorTest();
        
        std::cout << "All tests completed." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
