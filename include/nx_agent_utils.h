// nx_agent_utils.h
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <nx/sdk/analytics/helpers/metadata_packet.h>

namespace nx_agent {

/**
 * Logging utilities with severity levels
 */
class Logger {
public:
    enum class Level {
        ERROR = 0,
        WARNING = 1,
        INFO = 2,
        DEBUG = 3,
        TRACE = 4
    };
    
    static void setLogLevel(Level level);
    static Level getLogLevel();
    
    static void error(const std::string& message);
    static void warning(const std::string& message);
    static void info(const std::string& message);
    static void debug(const std::string& message);
    static void trace(const std::string& message);
    
    // Log with context (e.g., device ID)
    static void error(const std::string& context, const std::string& message);
    static void warning(const std::string& context, const std::string& message);
    static void info(const std::string& context, const std::string& message);
    static void debug(const std::string& context, const std::string& message);
    static void trace(const std::string& context, const std::string& message);
    
private:
    static Level s_logLevel;
    static void log(Level level, const std::string& context, const std::string& message);
};

/**
 * Image processing utilities
 */
namespace ImageUtils {
    // Resize image to target dimensions while maintaining aspect ratio
    cv::Mat resizeKeepAspectRatio(const cv::Mat& input, 
                                  int targetWidth,
                                  int targetHeight);
    
    // Convert Nx frame to OpenCV Mat
    cv::Mat nxFrameToMat(const nx::sdk::analytics::IUncompressedVideoFrame* frame);
    
    // Enhance image contrast for better visibility
    cv::Mat enhanceContrast(const cv::Mat& input, float alpha = 1.2f, int beta = 10);
    
    // Create a mask from regions of interest
    cv::Mat createMaskFromRegions(int width, int height, 
                                  const std::vector<std::vector<cv::Point>>& regions,
                                  bool inverted = false);
    
    // Draw bounding boxes for objects on an image
    void drawDetections(cv::Mat& image, 
                       const nx::sdk::analytics::ObjectMetadataList& objects,
                       const std::map<std::string, cv::Scalar>& typeColors);
}

/**
 * Time and date utilities
 */
namespace TimeUtils {
    // Format microsecond timestamp to human-readable string
    std::string formatTimestamp(int64_t timestampUs);
    
    // Get current time in microseconds
    int64_t getCurrentTimestampUs();
    
    // Convert timestamp to time of day in seconds
    int getTimeOfDaySeconds(int64_t timestampUs);
    
    // Check if time is within a given range
    bool isTimeInRange(int timeOfDaySeconds, int startSeconds, int endSeconds);
    
    // Get day of week from timestamp (0 = Sunday, 6 = Saturday)
    int getDayOfWeek(int64_t timestampUs);
    
    // Structure to represent a time range
    struct TimeRange {
        int startSeconds;
        int endSeconds;
        std::vector<int> dayOfWeekMask; // 7 elements, 1 = active on that day
        
        // Check if a timestamp is within this range
        bool contains(int64_t timestampUs) const;
    };
}

/**
 * String utilities
 */
namespace StringUtils {
    // Split string by delimiter
    std::vector<std::string> split(const std::string& input, char delimiter);
    
    // Trim whitespace from start and end of string
    std::string trim(const std::string& input);
    
    // Format string with arguments (printf-style)
    template<typename... Args>
    std::string format(const std::string& format, Args... args);
    
    // Convert to lowercase/uppercase
    std::string toLower(const std::string& input);
    std::string toUpper(const std::string& input);
    
    // Sanitize string for use in filenames
    std::string sanitizeForFilename(const std::string& input);
    
    // Base64 encoding/decoding
    std::string base64Encode(const std::vector<uint8_t>& data);
    std::vector<uint8_t> base64Decode(const std::string& encoded);
}

/**
 * Performance measurement utilities
 */
class ScopedTimer {
public:
    ScopedTimer(const std::string& operationName);
    ~ScopedTimer();
    
private:
    std::string m_operationName;
    std::chrono::high_resolution_clock::time_point m_startTime;
};

// Macro for easy timing of code blocks
#define TIME_SCOPE(name) ScopedTimer scopedTimer(name)

} // namespace nx_agent
