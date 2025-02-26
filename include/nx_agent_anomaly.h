// nx_agent_anomaly.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <deque>
#include <opencv2/opencv.hpp>

namespace nx_agent {

// Forward declarations
class DeviceConfig;
struct FrameAnalysisResult;

/**
 * Feature vector for anomaly detection model
 */
struct FeatureVector {
    int64_t timestampUs;
    int timeOfDaySeconds;  // Seconds since midnight
    int dayOfWeek;         // 0-6 (Sunday=0)
    float motionLevel;
    int personCount;
    int unknownPersonCount;
    int vehicleCount;
    std::vector<float> additionalFeatures;  // For extensibility
    
    // Convert to/from OpenCV Mat for model input
    cv::Mat toMat() const;
    static FeatureVector fromMat(const cv::Mat& mat);
};

/**
 * Abstract base class for anomaly detection models
 */
class AnomalyModel {
public:
    virtual ~AnomalyModel() = default;
    
    // Train the model with new data
    virtual void train(const std::vector<FeatureVector>& normalFeatures) = 0;
    
    // Score a feature vector (higher score = more anomalous)
    virtual float scoreAnomaly(const FeatureVector& features) = 0;
    
    // Save model to file
    virtual bool saveToFile(const std::string& filePath) = 0;
    
    // Load model from file
    virtual bool loadFromFile(const std::string& filePath) = 0;
    
    // Check if the model has been trained
    virtual bool isTrained() const = 0;
};

/**
 * Simple statistical model based on Gaussian distribution
 */
class GaussianModel : public AnomalyModel {
public:
    GaussianModel();
    
    void train(const std::vector<FeatureVector>& normalFeatures) override;
    float scoreAnomaly(const FeatureVector& features) override;
    bool saveToFile(const std::string& filePath) override;
    bool loadFromFile(const std::string& filePath) override;
    bool isTrained() const override { return m_trained; }
    
private:
    cv::Mat m_mean;        // Mean vector for each feature
    cv::Mat m_stdDev;      // Standard deviation for each feature
    bool m_trained = false;
};

/**
 * Main anomaly detection engine
 */
class AnomalyDetector {
public:
    AnomalyDetector(const std::string& deviceId);
    ~AnomalyDetector();
    
    // Configure the detector
    void configure(std::shared_ptr<DeviceConfig> config);
    
    // Process a frame analysis result to detect anomalies
    bool detectAnomaly(FrameAnalysisResult& result);
    
    // Add a normal frame to the baseline data
    void addToBaseline(const FrameAnalysisResult& result);
    
    // Clear existing baseline data
    void resetBaseline();
    
    // Manually set an alert threshold
    void setThreshold(float threshold);
    
    // Save model to disk
    bool saveModel();
    
    // Load model from disk
    bool loadModel();
    
private:
    // Device identification
    std::string m_deviceId;
    
    // Configuration
    std::shared_ptr<DeviceConfig> m_config;
    
    // Anomaly models (time-based)
    std::map<int, std::unique_ptr<AnomalyModel>> m_timeModels;  // Key: hour of day
    
    // Baseline data collection
    std::mutex m_baselineMutex;
    std::map<int, std::vector<FeatureVector>> m_baselineData;  // Key: hour of day
    
    // Recent history for short-term pattern detection
    std::deque<FeatureVector> m_recentHistory;
    
    // Thresholds
    float m_anomalyThreshold;
    
    // Helper methods
    FeatureVector extractFeatures(const FrameAnalysisResult& result);
    void trainModels();
    std::string getModelFilePath(int hourOfDay) const;
    int getHourOfDay(int64_t timestampUs) const;
};

} // namespace nx_agent
