// nx_agent_anomaly.cpp
#include "nx_agent_anomaly.h"
#include "nx_agent_config.h"
#include "nx_agent_metadata.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <algorithm>

namespace nx_agent {

// FeatureVector implementation
cv::Mat FeatureVector::toMat() const {
    // Create a feature matrix (row vector)
    int featureCount = 5 + additionalFeatures.size();
    cv::Mat mat(1, featureCount, CV_32F);
    
    // Basic features
    mat.at<float>(0, 0) = static_cast<float>(timeOfDaySeconds) / 86400.0f; // Normalize to 0-1
    mat.at<float>(0, 1) = static_cast<float>(dayOfWeek) / 7.0f;           // Normalize to 0-1
    mat.at<float>(0, 2) = motionLevel;
    mat.at<float>(0, 3) = static_cast<float>(personCount);
    mat.at<float>(0, 4) = static_cast<float>(vehicleCount);
    
    // Additional features
    for (size_t i = 0; i < additionalFeatures.size(); ++i) {
        mat.at<float>(0, 5 + i) = additionalFeatures[i];
    }
    
    return mat;
}

FeatureVector FeatureVector::fromMat(const cv::Mat& mat) {
    FeatureVector features;
    
    // Ensure the matrix has the expected format
    if (mat.rows != 1 || mat.type() != CV_32F) {
        std::cerr << "Invalid matrix format for FeatureVector conversion" << std::endl;
        return features;
    }
    
    // Extract basic features
    features.timeOfDaySeconds = static_cast<int>(mat.at<float>(0, 0) * 86400.0f);
    features.dayOfWeek = static_cast<int>(mat.at<float>(0, 1) * 7.0f);
    features.motionLevel = mat.at<float>(0, 2);
    features.personCount = static_cast<int>(mat.at<float>(0, 3));
    features.vehicleCount = static_cast<int>(mat.at<float>(0, 4));
    
    // Extract additional features
    features.additionalFeatures.clear();
    for (int i = 5; i < mat.cols; ++i) {
        features.additionalFeatures.push_back(mat.at<float>(0, i));
    }
    
    return features;
}

// GaussianModel implementation
GaussianModel::GaussianModel() {}

void GaussianModel::train(const std::vector<FeatureVector>& normalFeatures) {
    if (normalFeatures.empty()) {
        std::cerr << "Cannot train on empty feature set" << std::endl;
        return;
    }
    
    // Convert features to matrix for statistical analysis
    int featureCount = normalFeatures[0].toMat().cols;
    cv::Mat featureMat(normalFeatures.size(), featureCount, CV_32F);
    
    for (size_t i = 0; i < normalFeatures.size(); ++i) {
        cv::Mat row = normalFeatures[i].toMat();
        row.copyTo(featureMat.row(i));
    }
    
    // Calculate mean and standard deviation for each feature
    cv::calcCovarMatrix(featureMat, m_mean, m_stdDev, cv::COVAR_NORMAL | cv::COVAR_ROWS | cv::COVAR_SCALE);
    m_mean = m_mean.reshape(1, 1); // Ensure it's a row vector
    
    // Calculate standard deviation as sqrt of variance (diagonal of covariance matrix)
    m_stdDev = cv::Mat(1, featureCount, CV_32F);
    for (int i = 0; i < featureCount; ++i) {
        m_stdDev.at<float>(0, i) = std::sqrt(m_stdDev.at<float>(i, i));
    }
    
    m_trained = true;
}

float GaussianModel::scoreAnomaly(const FeatureVector& features) {
    if (!m_trained) {
        return 1.0f; // Consider everything anomalous if not trained
    }
    
    cv::Mat featureMat = features.toMat();
    
    // Calculate Mahalanobis distance (simplified version using std dev instead of full covariance)
    float anomalyScore = 0.0f;
    for (int i = 0; i < featureMat.cols; ++i) {
        float value = featureMat.at<float>(0, i);
        float mean = m_mean.at<float>(0, i);
        float stdDev = m_stdDev.at<float>(0, i);
        
        // Avoid division by zero
        if (stdDev > 1e-5) {
            float normalizedDiff = (value - mean) / stdDev;
            anomalyScore += normalizedDiff * normalizedDiff;
        }
    }
    
    // Normalize to 0-1 range using an exponential transformation
    return 1.0f - std::exp(-anomalyScore / (2.0f * featureMat.cols));
}

bool GaussianModel::saveToFile(const std::string& filePath) {
    try {
        cv::FileStorage fs(filePath, cv::FileStorage::WRITE);
        if (!fs.isOpened()) {
            std::cerr << "Failed to open file for writing: " << filePath << std::endl;
            return false;
        }
        
        fs << "trained" << m_trained;
        fs << "mean" << m_mean;
        fs << "stdDev" << m_stdDev;
        
        fs.release();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving model: " << e.what() << std::endl;
        return false;
    }
}

bool GaussianModel::loadFromFile(const std::string& filePath) {
    try {
        cv::FileStorage fs(filePath, cv::FileStorage::READ);
        if (!fs.isOpened()) {
            std::cerr << "Failed to open file for reading: " << filePath << std::endl;
            return false;
        }
        
        fs["trained"] >> m_trained;
        fs["mean"] >> m_mean;
        fs["stdDev"] >> m_stdDev;
        
        fs.release();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading model: " << e.what() << std::endl;
        return false;
    }
}

// AnomalyDetector implementation
AnomalyDetector::AnomalyDetector(const std::string& deviceId)
    : m_deviceId(deviceId),
      m_anomalyThreshold(0.7f)
{
    // Load configuration
    m_config = GlobalConfig::instance().getDeviceConfig(deviceId);
    m_anomalyThreshold = m_config->anomalyThreshold;
    
    // Initialize models for each hour of the day
    for (int hour = 0; hour < 24; ++hour) {
        m_timeModels[hour] = std::make_unique<GaussianModel>();
        m_baselineData[hour] = std::vector<FeatureVector>();
        
        // Try to load existing model
        std::string modelPath = getModelFilePath(hour);
        if (std::filesystem::exists(modelPath)) {
            m_timeModels[hour]->loadFromFile(modelPath);
        }
    }
}

AnomalyDetector::~AnomalyDetector() {
    // Save models on destruction
    saveModel();
}

void AnomalyDetector::configure(std::shared_ptr<DeviceConfig> config) {
    m_config = config;
    m_anomalyThreshold = m_config->anomalyThreshold;
}

bool AnomalyDetector::detectAnomaly(FrameAnalysisResult& result) {
    // Extract features from the analysis result
    FeatureVector features = extractFeatures(result);
    
    // Get the appropriate model for this time of day
    int hour = getHourOfDay(result.timestampUs);
    auto modelIt = m_timeModels.find(hour);
    if (modelIt == m_timeModels.end() || !modelIt->second->isTrained()) {
        // No model available, consider as normal
        // Alternatively, you could consider everything anomalous when no model exists
        return false;
    }
    
    // Score anomaly using the model
    float anomalyScore = modelIt->second->scoreAnomaly(features);
    
    // Update the result
    result.anomalyScore = std::max(result.anomalyScore, anomalyScore);
    
    // If score exceeds threshold, mark as anomaly
    if (anomalyScore > m_anomalyThreshold) {
        // If no specific anomaly type is set, set a generic one
        if (result.anomalyType.empty()) {
            result.anomalyType = "StatisticalAnomaly";
            result.anomalyDescription = "Activity deviates from normal patterns";
        }
        result.isAnomaly = true;
        return true;
    }
    
    return false;
}

void AnomalyDetector::addToBaseline(const FrameAnalysisResult& result) {
    if (!m_config->enableLearning) {
        return;
    }
    
    FeatureVector features = extractFeatures(result);
    int hour = getHourOfDay(result.timestampUs);
    
    {
        std::lock_guard<std::mutex> lock(m_baselineMutex);
        
        // Add to hourly baseline data
        m_baselineData[hour].push_back(features);
        
        // If we've collected enough data, train the model
        if (m_baselineData[hour].size() >= 100) { // Arbitrary threshold, adjust as needed
            trainModels();
        }
    }
    
    // Also maintain recent history for short-term pattern detection
    m_recentHistory.push_back(features);
    while (m_recentHistory.size() > 1000) { // Keep last ~30 minutes assuming 1 frame/2 seconds
        m_recentHistory.pop_front();
    }
}

void AnomalyDetector::resetBaseline() {
    std::lock_guard<std::mutex> lock(m_baselineMutex);
    
    for (auto& pair : m_baselineData) {
        pair.second.clear();
    }
    
    m_recentHistory.clear();
    
    // Also reset models
    for (auto& pair : m_timeModels) {
        pair.second = std::make_unique<GaussianModel>();
    }
}

void AnomalyDetector::setThreshold(float threshold) {
    m_anomalyThreshold = std::max(0.0f, std::min(1.0f, threshold));
    if (m_config) {
        m_config->anomalyThreshold = m_anomalyThreshold;
        GlobalConfig::instance().updateDeviceConfig(m_config);
    }
}

bool AnomalyDetector::saveModel() {
    bool allSaved = true;
    
    for (const auto& pair : m_timeModels) {
        int hour = pair.first;
        const auto& model = pair.second;
        
        if (model->isTrained()) {
            std::string filePath = getModelFilePath(hour);
            if (!model->saveToFile(filePath)) {
                std::cerr << "Failed to save model for hour " << hour << std::endl;
                allSaved = false;
            }
        }
    }
    
    return allSaved;
}

bool AnomalyDetector::loadModel() {
    bool anyLoaded = false;
    
    for (auto& pair : m_timeModels) {
        int hour = pair.first;
        auto& model = pair.second;
        
        std::string filePath = getModelFilePath(hour);
        if (std::filesystem::exists(filePath)) {
            if (model->loadFromFile(filePath)) {
                anyLoaded = true;
            } else {
                std::cerr << "Failed to load model for hour " << hour << std::endl;
            }
        }
    }
    
    return anyLoaded;
}

// Private helper methods
FeatureVector AnomalyDetector::extractFeatures(const FrameAnalysisResult& result) {
    FeatureVector features;
    features.timestampUs = result.timestampUs;
    
    // Convert timestamp to time of day
    auto timePoint = std::chrono::system_clock::time_point(
        std::chrono::microseconds(result.timestampUs));
    std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm* localTime = std::localtime(&time);
    
    features.timeOfDaySeconds = localTime->tm_hour * 3600 + 
                                localTime->tm_min * 60 + 
                                localTime->tm_sec;
    features.dayOfWeek = localTime->tm_wday;
    
    // Basic motion and object features
    features.motionLevel = result.motionInfo.overallMotionLevel;
    
    // Count objects by type
    features.personCount = 0;
    features.unknownPersonCount = 0;
    features.vehicleCount = 0;
    
    for (const auto& obj : result.objects) {
        if (obj.typeId == "person") {
            features.personCount++;
            
            // Check if this person is recognized or unknown
            auto it = obj.attributes.find("recognitionStatus");
            if (it != obj.attributes.end() && it->second == "unknown") {
                features.unknownPersonCount++;
            }
        } else if (obj.typeId == "vehicle") {
            features.vehicleCount++;
        }
    }
    
    // Additional features can be added here
    // For example, ratios, motion pattern descriptors, etc.
    features.additionalFeatures.push_back(static_cast<float>(features.unknownPersonCount) / 
                                         std::max(1, features.personCount));
    
    return features;
}

void AnomalyDetector::trainModels() {
    std::lock_guard<std::mutex> lock(m_baselineMutex);
    
    for (const auto& pair : m_baselineData) {
        int hour = pair.first;
        const auto& data = pair.second;
        
        if (!data.empty()) {
            auto it = m_timeModels.find(hour);
            if (it != m_timeModels.end()) {
                it->second->train(data);
            }
        }
    }
    
    // Save models after training
    saveModel();
}

std::string AnomalyDetector::getModelFilePath(int hourOfDay) const {
    std::string dataPath = GlobalConfig::instance().dataStoragePath;
    
    // Create device-specific directory
    std::string deviceDir = dataPath + "/" + m_deviceId;
    std::filesystem::create_directories(deviceDir);
    
    return deviceDir + "/model_hour_" + std::to_string(hourOfDay) + ".xml";
}

int AnomalyDetector::getHourOfDay(int64_t timestampUs) const {
    auto timePoint = std::chrono::system_clock::time_point(
        std::chrono::microseconds(timestampUs));
    std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm* localTime = std::localtime(&time);
    
    return localTime->tm_hour;
}

} // namespace nx_agent
