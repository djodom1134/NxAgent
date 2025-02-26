// nx_agent_utils.cpp
#include "nx_agent_utils.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <cstdio>
#include <mutex>

namespace nx_agent {

//
// Logger implementation
//
Logger::Level Logger::s_logLevel = Logger::Level::INFO;

void Logger::setLogLevel(Level level) {
    s_logLevel = level;
}

Logger::Level Logger::getLogLevel() {
    return s_logLevel;
}

void Logger::error(const std::string& message) {
    log(Level::ERROR, "", message);
}

void Logger::warning(const std::string& message) {
    log(Level::WARNING, "", message);
}

void Logger::info(const std::string& message) {
    log(Level::INFO, "", message);
}

void Logger::debug(const std::string& message) {
    log(Level::DEBUG, "", message);
}

void Logger::trace(const std::string& message) {
    log(Level::TRACE, "", message);
}

void Logger::error(const std::string& context, const std::string& message) {
    log(Level::ERROR, context, message);
}

void Logger::warning(const std::string& context, const std::string& message) {
    log(Level::WARNING, context, message);
}

void Logger::info(const std::string& context, const std::string& message) {
    log(Level::INFO, context, message);
}

void Logger::debug(const std::string& context, const std::string& message) {
    log(Level::DEBUG, context, message);
}

void Logger::trace(const std::string& context, const std::string& message) {
    log(Level::TRACE, context, message);
}

void Logger::log(Level level, const std::string& context, const std::string& message) {
    static std::mutex logMutex;
    
    if (level > s_logLevel) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(logMutex);
    
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    std::string levelStr;
    switch (level) {
        case Level::ERROR:
            levelStr = "ERROR";
            break;
        case Level::WARNING:
            levelStr = "WARNING";
            break;
        case Level::INFO:
            levelStr = "INFO";
            break;
        case Level::DEBUG:
            levelStr = "DEBUG";
            break;
        case Level::TRACE:
            levelStr = "TRACE";
            break;
    }
    
    std::string contextStr = context.empty() ? "" : "[" + context + "] ";
    
    // Log to console (in a real implementation, this might go to a file)
    std::cout << timestamp.str() << " " << levelStr << " " << contextStr << message << std::endl;
}

//
// ImageUtils implementation
//
namespace ImageUtils {

cv::Mat resizeKeepAspectRatio(const cv::Mat& input, int targetWidth, int targetHeight) {
    if (input.empty()) {
        return input;
    }
    
    double h1 = targetWidth * (input.rows / (double)input.cols);
    double w2 = targetHeight * (input.cols / (double)input.rows);
    
    cv::Mat resized;
    
    if (h1 <= targetHeight) {
        // Resize by width
        cv::resize(input, resized, cv::Size(targetWidth, h1));
        
        // Add padding if needed
        if (h1 < targetHeight) {
            int padding = targetHeight - h1;
            int topPadding = padding / 2;
            int bottomPadding = padding - topPadding;
            
            cv::copyMakeBorder(resized, resized, topPadding, bottomPadding, 0, 0, 
                               cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
        }
    } else {
        // Resize by height
        cv::resize(input, resized, cv::Size(w2, targetHeight));
        
        // Add padding if needed
        if (w2 < targetWidth) {
            int padding = targetWidth - w2;
            int leftPadding = padding / 2;
            int rightPadding = padding - leftPadding;
            
            cv::copyMakeBorder(resized, resized, 0, 0, leftPadding, rightPadding,
                               cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
        }
    }
    
    return resized;
}

cv::Mat nxFrameToMat(const nx::sdk::analytics::IUncompressedVideoFrame* frame) {
    if (!frame) {
        return cv::Mat();
    }
    
    // Get frame format and dimensions
    auto format = frame->format();
    int width = frame->width();
    int height = frame->height();
    
    if (format == nx::sdk::analytics::UncompressedVideoFrame::Format::rgb24) {
        // RGB24 format - convert to BGR for OpenCV
        cv::Mat rgbMat(height, width, CV_8UC3, const_cast<void*>(frame->data()));
        cv::Mat bgrMat;
        cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
        return bgrMat;
    } else if (format == nx::sdk::analytics::UncompressedVideoFrame::Format::bgr24) {
        // BGR24 format - already suitable for OpenCV
        return cv::Mat(height, width, CV_8UC3, const_cast<void*>(frame->data())).clone();
    } else if (format == nx::sdk::analytics::UncompressedVideoFrame::Format::nv12) {
        // NV12 format - convert to BGR
        cv::Mat yuv(height * 3 / 2, width, CV_8UC1, const_cast<void*>(frame->data()));
        cv::Mat bgr;
        cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_NV12);
        return bgr;
    } else if (format == nx::sdk::analytics::UncompressedVideoFrame::Format::y800) {
        // Y800 format (grayscale) - create a grayscale Mat
        return cv::Mat(height, width, CV_8UC1, const_cast<void*>(frame->data())).clone();
    } else {
        // Unsupported format
        Logger::error("ImageUtils", "Unsupported frame format");
        return cv::Mat();
    }
}

cv::Mat enhanceContrast(const cv::Mat& input, float alpha, int beta) {
    cv::Mat enhanced;
    input.convertTo(enhanced, -1, alpha, beta);
    return enhanced;
}

cv::Mat createMaskFromRegions(int width, int height, 
                             const std::vector<std::vector<cv::Point>>& regions,
                             bool inverted) {
    // Create a black mask
    cv::Mat mask = cv::Mat::zeros(height, width, CV_8UC1);
    
    // Draw white polygons for all regions
    for (const auto& region : regions) {
        if (region.size() >= 3) {
            std::vector<std::vector<cv::Point>> contours = { region };
            cv::fillPoly(mask, contours, cv::Scalar(255));
        }
    }
    
    // Invert if requested
    if (inverted) {
        cv::bitwise_not(mask, mask);
    }
    
    return mask;
}

void drawDetections(cv::Mat& image, 
                   const nx::sdk::analytics::ObjectMetadataList& objects,
                   const std::map<std::string, cv::Scalar>& typeColors) {
    for (int i = 0; i < objects.size(); ++i) {
        const auto& obj = objects.at(i);
        
        // Get bounding box
        float x, y, width, height;
        obj.boundingBox(&x, &y, &width, &height);
        
        // Convert to pixel coordinates
        int pixelX = x * image.cols;
        int pixelY = y * image.rows;
        int pixelWidth = width * image.cols;
        int pixelHeight = height * image.rows;
        
        // Get color based on object type
        cv::Scalar color(0, 255, 0); // Default to green
        auto colorIt = typeColors.find(obj.typeId);
        if (colorIt != typeColors.end()) {
            color = colorIt->second;
        }
        
        // Draw rectangle
        cv::rectangle(image, 
                      cv::Rect(pixelX, pixelY, pixelWidth, pixelHeight),
                      color, 2);
        
        // Draw label
        std::string label = obj.typeId;
        
        // Add confidence if available
        float confidence;
        if (obj.attributes() && obj.attributes()->getFloat("confidence", &confidence)) {
            label += " " + std::to_string(int(confidence * 100)) + "%";
        }
        
        // Add recognition status if available
        std::string recognitionStatus;
        if (obj.attributes() && obj.attributes()->getString("recognitionStatus", &recognitionStatus)) {
            label += " (" + recognitionStatus + ")";
        }
        
        // Draw text background and text
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        cv::rectangle(image, 
                     cv::Point(pixelX, pixelY - textSize.height - 5),
                     cv::Point(pixelX + textSize.width, pixelY),
                     color, -1);
                     
        cv::putText(image, label, 
                   cv::Point(pixelX, pixelY - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }
}

} // namespace ImageUtils

//
// TimeUtils implementation
//
namespace TimeUtils {

std::string formatTimestamp(int64_t timestampUs) {
    // Convert to time_t (seconds)
    time_t seconds = timestampUs / 1000000;
    
    // Get milliseconds part
    int milliseconds = (timestampUs % 1000000) / 1000;
    
    // Format the time part
    std::tm* timeInfo = std::localtime(&seconds);
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo);
    
    // Add milliseconds
    std::stringstream result;
    result << buffer << "." << std::setfill('0') << std::setw(3) << milliseconds;
    
    return result.str();
}

int64_t getCurrentTimestampUs() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
}

int getTimeOfDaySeconds(int64_t timestampUs) {
    // Convert to time_t (seconds)
    time_t seconds = timestampUs / 1000000;
    
    // Extract time of day
    std::tm* timeInfo = std::localtime(&seconds);
    return timeInfo->tm_hour * 3600 + timeInfo->tm_min * 60 + timeInfo->tm_sec;
}

bool isTimeInRange(int timeOfDaySeconds, int startSeconds, int endSeconds) {
    // Handle ranges that cross midnight
    if (startSeconds <= endSeconds) {
        // Normal range (e.g., 9:00 to 17:00)
        return timeOfDaySeconds >= startSeconds && timeOfDaySeconds <= endSeconds;
    } else {
        // Range crosses midnight (e.g., 22:00 to 6:00)
        return timeOfDaySeconds >= startSeconds || timeOfDaySeconds <= endSeconds;
    }
}

int getDayOfWeek(int64_t timestampUs) {
    // Convert to time_t (seconds)
    time_t seconds = timestampUs / 1000000;
    
    // Extract day of week (0 = Sunday, 6 = Saturday)
    std::tm* timeInfo = std::localtime(&seconds);
    return timeInfo->tm_wday;
}

bool TimeRange::contains(int64_t timestampUs) const {
    int timeOfDay = getTimeOfDaySeconds(timestampUs);
    int dayOfWeek = getDayOfWeek(timestampUs);
    
    // Check if this day of week is enabled
    if (dayOfWeekMask.size() == 7 && dayOfWeekMask[dayOfWeek] == 0) {
        return false;
    }
    
    // Check time range
    return isTimeInRange(timeOfDay, startSeconds, endSeconds);
}

} // namespace TimeUtils

//
// StringUtils implementation
//
namespace StringUtils {

std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(input);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string trim(const std::string& input) {
    auto start = input.begin();
    while (start != input.end() && std::isspace(*start)) {
        start++;
    }
    
    auto end = input.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    
    return std::string(start, end + 1);
}

// Implementation of format (variadic template function)
// This is a simple implementation that uses sprintf
template<typename... Args>
std::string format(const std::string& formatStr, Args... args) {
    // Calculate required buffer size
    int size = std::snprintf(nullptr, 0, formatStr.c_str(), args...) + 1;
    if (size <= 0) {
        return "FORMAT_ERROR";
    }
    
    std::vector<char> buffer(size);
    std::snprintf(buffer.data(), size, formatStr.c_str(), args...);
    
    return std::string(buffer.data(), buffer.data() + size - 1);
}

// To avoid linker errors, we need explicit instantiations for common types
// Include the ones you expect to use
template std::string format<int>(const std::string&, int);
template std::string format<int, int>(const std::string&, int, int);
template std::string format<float>(const std::string&, float);
template std::string format<const char*>(const std::string&, const char*);
template std::string format<std::string>(const std::string&, std::string);

std::string toLower(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string toUpper(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string sanitizeForFilename(const std::string& input) {
    std::string result = input;
    
    // Replace characters that are invalid in filenames
    const std::string invalidChars = "<>:\"/\\|?*";
    for (char c : invalidChars) {
        std::replace(result.begin(), result.end(), c, '_');
    }
    
    return result;
}

// Simple Base64 encoding
static const std::string base64Chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<uint8_t>& data) {
    std::string encoded;
    encoded.reserve(((data.size() + 2) / 3) * 4);
    
    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = i < data.size() ? data[i++] : 0;
        uint32_t octet_b = i < data.size() ? data[i++] : 0;
        uint32_t octet_c = i < data.size() ? data[i++] : 0;
        
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        encoded.push_back(base64Chars[(triple >> 18) & 0x3F]);
        encoded.push_back(base64Chars[(triple >> 12) & 0x3F]);
        encoded.push_back(base64Chars[(triple >> 6) & 0x3F]);
        encoded.push_back(base64Chars[triple & 0x3F]);
    }
    
    // Add padding if needed
    size_t paddingCount = (3 - (data.size() % 3)) % 3;
    for (size_t i = 0; i < paddingCount; ++i) {
        encoded[encoded.size() - 1 - i] = '=';
    }
    
    return encoded;
}

std::vector<uint8_t> base64Decode(const std::string& encoded) {
    std::vector<uint8_t> decoded;
    if (encoded.empty()) {
        return decoded;
    }
    
    // Build reverse lookup table
    int reverseTable[256];
    std::fill_n(reverseTable, 256, -1);
    for (size_t i = 0; i < base64Chars.size(); ++i) {
        reverseTable[static_cast<int>(base64Chars[i])] = i;
    }
    
    size_t padding = 0;
    if (encoded.size() >= 2) {
        if (encoded[encoded.size() - 1] == '=') padding++;
        if (encoded[encoded.size() - 2] == '=') padding++;
    }
    
    decoded.reserve(((encoded.size() / 4) * 3) - padding);
    
    size_t i = 0;
    while (i < encoded.size()) {
        // Skip non-base64 characters
        if (reverseTable[static_cast<int>(encoded[i])] == -1) {
            i++;
            continue;
        }
        
        size_t block_start = i;
        size_t block_end = std::min(i + 4, encoded.size());
        
        // Process a 4-character block
        if (block_end - block_start == 4) {
            uint32_t sextet_a = encoded[block_start] == '=' ? 0 : reverseTable[static_cast<int>(encoded[block_start])];
            uint32_t sextet_b = encoded[block_start + 1] == '=' ? 0 : reverseTable[static_cast<int>(encoded[block_start + 1])];
            uint32_t sextet_c = encoded[block_start + 2] == '=' ? 0 : reverseTable[static_cast<int>(encoded[block_start + 2])];
            uint32_t sextet_d = encoded[block_start + 3] == '=' ? 0 : reverseTable[static_cast<int>(encoded[block_start + 3])];
            
            uint32_t triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;
            
            if (block_start + 2 < encoded.size() && encoded[block_start + 2] != '=') {
                decoded.push_back((triple >> 16) & 0xFF);
            }
            if (block_start + 3 < encoded.size() && encoded[block_start + 3] != '=') {
                decoded.push_back((triple >> 8) & 0xFF);
            }
            if (block_start + 4 < encoded.size() && encoded[block_start + 4] != '=') {
                decoded.push_back(triple & 0xFF);
            }
        }
        
        i = block_end;
    }
    
    return decoded;
}

} // namespace StringUtils

//
// ScopedTimer implementation
//
ScopedTimer::ScopedTimer(const std::string& operationName)
    : m_operationName(operationName),
      m_startTime(std::chrono::high_resolution_clock::now())
{
}

ScopedTimer::~ScopedTimer() {
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - m_startTime).count();
    
    Logger::debug("ScopedTimer", m_operationName + " took " + 
                  std::to_string(duration) + " microseconds");
}

} // namespace nx_agent
