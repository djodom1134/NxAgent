// nx_agent_response.cpp
#include "nx_agent_response.h"
#include "nx_agent_config.h"
#include "nx_agent_metadata.h"

#include <iostream>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <curl/curl.h>
#include <thread>
#include <future>

namespace nx_agent {

// Helper function for CURL response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char*)contents, newLength);
        return newLength;
    } catch(std::bad_alloc& e) {
        // Handle memory problem
        return 0;
    }
}

// ResponseProtocol implementation
ResponseProtocol::ResponseProtocol(const std::string& deviceId)
    : m_deviceId(deviceId)
{
    // Load configuration
    m_config = GlobalConfig::instance().getDeviceConfig(deviceId);
    
    // Set up default response actions
    ResponseAction logAction;
    logAction.type = ResponseAction::Type::LOG_ONLY;
    logAction.name = "LogAnomaly";
    logAction.description = "Log anomaly detection to system log";
    logAction.priority = 0;
    
    ResponseAction nxEventAction;
    nxEventAction.type = ResponseAction::Type::NX_EVENT;
    nxEventAction.name = "NxEvent";
    nxEventAction.description = "Generate Nx event for the anomaly";
    nxEventAction.priority = 10;
    
    // Add these default actions for all anomaly types
    addResponseAction("UnknownVisitor", logAction);
    addResponseAction("UnknownVisitor", nxEventAction);
    
    addResponseAction("AbnormalActivity", logAction);
    addResponseAction("AbnormalActivity", nxEventAction);
    
    addResponseAction("GeneralAnomaly", logAction);
    addResponseAction("GeneralAnomaly", nxEventAction);
    
    // If SIP integration is enabled globally, add SIP call action for high-priority anomalies
    if (GlobalConfig::instance().enableSipIntegration) {
        ResponseAction sipAction;
        sipAction.type = ResponseAction::Type::SIP_CALL;
        sipAction.name = "SipNotification";
        sipAction.description = "Make SIP call to security personnel";
        sipAction.target = GlobalConfig::instance().alarmPhoneNumber;
        sipAction.priority = 20;
        sipAction.cooldownMs = 300000; // 5 minutes cooldown
        
        // Add SIP call for unknown visitor (higher priority anomaly)
        addResponseAction("UnknownVisitor", sipAction);
    }
    
    // Initialize CURL for HTTP requests
    curl_global_init(CURL_GLOBAL_ALL);
}

ResponseProtocol::~ResponseProtocol() {
    // Clean up CURL
    curl_global_cleanup();
}

void ResponseProtocol::configure(std::shared_ptr<DeviceConfig> config) {
    m_config = config;
    
    // Reconfigure response actions based on new settings
    // Here you might reload actions from a config file or database
}

bool ResponseProtocol::processAnomaly(const FrameAnalysisResult& result) {
    if (!result.isAnomaly) {
        return false;
    }
    
    // Get current time
    auto now = std::chrono::system_clock::now();
    
    // Clean up old anomalies first
    cleanupAnomalies();
    
    std::lock_guard<std::mutex> lock(m_anomalyMutex);
    
    // Get or create tracker for this anomaly type
    auto it = m_activeAnomalies.find(result.anomalyType);
    if (it == m_activeAnomalies.end()) {
        // New anomaly
        AnomalyTracker tracker;
        tracker.anomalyType = result.anomalyType;
        tracker.initialScore = result.anomalyScore;
        tracker.firstDetectedTime = now;
        tracker.lastDetectedTime = now;
        tracker.verified = false;
        tracker.responded = false;
        
        m_activeAnomalies[result.anomalyType] = tracker;
        it = m_activeAnomalies.find(result.anomalyType);
    } else {
        // Existing anomaly, update
        it->second.consecutiveDetections++;
        it->second.lastDetectedTime = now;
        
        // If anomaly score is higher, update it
        if (result.anomalyScore > it->second.initialScore) {
            it->second.initialScore = result.anomalyScore;
        }
    }
    
    // Try to verify the anomaly
    bool verified = verifyAnomaly(result, it->second);
    
    // If verified and not responded yet, trigger responses
    if (verified && !it->second.responded) {
        triggerResponses(result, it->second);
        it->second.responded = true;
        return true;
    }
    
    return false;
}

void ResponseProtocol::addResponseAction(const std::string& anomalyType, const ResponseAction& action) {
    // Add or update the action
    bool exists = false;
    for (auto& existingAction : m_responseActions[anomalyType]) {
        if (existingAction.name == action.name) {
            // Update existing action
            existingAction = action;
            exists = true;
            break;
        }
    }
    
    if (!exists) {
        // Add new action
        m_responseActions[anomalyType].push_back(action);
        
        // Sort by priority (highest first)
        std::sort(m_responseActions[anomalyType].begin(), m_responseActions[anomalyType].end(),
            [](const ResponseAction& a, const ResponseAction& b) {
                return a.priority > b.priority;
            });
    }
}

void ResponseProtocol::removeResponseAction(const std::string& anomalyType, const std::string& actionName) {
    auto it = m_responseActions.find(anomalyType);
    if (it != m_responseActions.end()) {
        auto& actions = it->second;
        actions.erase(
            std::remove_if(actions.begin(), actions.end(),
                [&actionName](const ResponseAction& action) {
                    return action.name == actionName;
                }),
            actions.end());
    }
}

void ResponseProtocol::setNxEventCallback(NxEventCallback callback) {
    m_nxEventCallback = callback;
}

// Private helper methods
bool ResponseProtocol::verifyAnomaly(const FrameAnalysisResult& result, AnomalyTracker& tracker) {
    // Simple verification logic - can be expanded
    
    // If already verified, keep that state
    if (tracker.verified) {
        return true;
    }
    
    // Verify based on confidence score and repetition
    if (result.anomalyScore > 0.85) {
        // High confidence, verify immediately
        tracker.verified = true;
        return true;
    } else if (result.anomalyScore > 0.7 && tracker.consecutiveDetections >= 2) {
        // Medium confidence with repetition
        tracker.verified = true;
        return true;
    } else if (tracker.consecutiveDetections >= 3) {
        // Lower confidence but persistent
        tracker.verified = true;
        return true;
    }
    
    // Check duration - if anomaly persists for more than 30 seconds
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        tracker.lastDetectedTime - tracker.firstDetectedTime).count();
        
    if (duration > 30) {
        tracker.verified = true;
        return true;
    }
    
    return false;
}

void ResponseProtocol::triggerResponses(const FrameAnalysisResult& result, const AnomalyTracker& tracker) {
    // Get the appropriate response actions for this anomaly type
    auto it = m_responseActions.find(tracker.anomalyType);
    if (it == m_responseActions.end()) {
        // If no specific actions defined, use general anomaly actions
        it = m_responseActions.find("GeneralAnomaly");
        if (it == m_responseActions.end()) {
            // No actions defined at all
            std::cerr << "No response actions defined for anomaly type: " << tracker.anomalyType << std::endl;
            return;
        }
    }
    
    // Current time for cooldown checking
    auto now = std::chrono::system_clock::now();
    
    // Execute each action in priority order
    for (auto& action : it->second) {
        // Check cooldown
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - action.lastTriggeredTime).count();
            
        if (elapsed < action.cooldownMs) {
            // Action is in cooldown, skip
            continue;
        }
        
        // Execute the action
        bool success = executeAction(action, result);
        
        if (success) {
            // Update last triggered time
            action.lastTriggeredTime = now;
        }
    }
}

bool ResponseProtocol::executeAction(const ResponseAction& action, const FrameAnalysisResult& result) {
    // Execute based on action type
    switch (action.type) {
        case ResponseAction::Type::LOG_ONLY:
            // Just log the event
            std::cout << "[NxAgentResponse] Anomaly detected: " << result.anomalyType 
                      << " - " << result.anomalyDescription
                      << " (Score: " << result.anomalyScore << ")" << std::endl;
            return true;
            
        case ResponseAction::Type::NX_EVENT:
            // Generate NX event via callback
            if (m_nxEventCallback) {
                m_nxEventCallback(result);
                return true;
            } else {
                std::cerr << "NX event callback not set" << std::endl;
                return false;
            }
            
        case ResponseAction::Type::HTTP_REQUEST:
            // Make HTTP request in a separate thread to avoid blocking
            if (!action.target.empty()) {
                // Create payload with anomaly details
                std::string payload = action.payload;
                if (payload.empty()) {
                    payload = "{"
                        "\"anomalyType\":\"" + result.anomalyType + "\","
                        "\"description\":\"" + result.anomalyDescription + "\","
                        "\"score\":" + std::to_string(result.anomalyScore) + ","
                        "\"deviceId\":\"" + m_deviceId + "\","
                        "\"timestamp\":" + std::to_string(result.timestampUs) +
                        "}";
                }
                
                // Launch async HTTP request
                std::thread([this, url = action.target, payload]() {
                    this->sendHttpRequest(url, payload);
                }).detach();
                
                return true;
            }
            return false;
            
        case ResponseAction::Type::SIP_CALL:
            // Make SIP call if enabled
            if (GlobalConfig::instance().enableSipIntegration && !action.target.empty()) {
                // Format message
                std::string message = "Anomaly detected on camera " + m_deviceId + 
                                     ". Type: " + result.anomalyType;
                
                // Launch in separate thread
                std::thread([this, number = action.target, message]() {
                    this->makeSipCall(number, message);
                }).detach();
                
                return true;
            }
            return false;
            
        case ResponseAction::Type::EXECUTE_COMMAND:
            // Execute local command
            if (!action.target.empty()) {
                // Launch in separate thread
                std::thread([this, command = action.target]() {
                    this->executeCommand(command);
                }).detach();
                
                return true;
            }
            return false;
            
        default:
            std::cerr << "Unknown action type" << std::endl;
            return false;
    }
}

bool ResponseProtocol::sendHttpRequest(const std::string& url, const std::string& payload) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return false;
    }
    
    std::string response;
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // Set POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    
    // Set response callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Set headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "HTTP request failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    return true;
}

bool ResponseProtocol::makeSipCall(const std::string& number, const std::string& message) {
    // This is a placeholder for SIP call functionality
    // In a real implementation, this would integrate with a SIP library like PJSIP
    
    std::cout << "[NxAgentResponse] Would make SIP call to " << number 
              << " with message: " << message << std::endl;
              
    // For integration with actual SIP library, this method would:
    // 1. Check if SIP client is initialized/connected
    // 2. Place call to the target number
    // 3. When connected, play message (text-to-speech or pre-recorded)
    // 4. Handle call completion
    
    // For now, just simulate success
    return true;
}

bool ResponseProtocol::executeCommand(const std::string& command) {
    // Execute local system command
    // NOTE: This should be used with caution as it can be a security risk
    // In a production environment, you should validate commands carefully
    
    std::cout << "[NxAgentResponse] Executing command: " << command << std::endl;
    
    int result = std::system(command.c_str());
    
    if (result != 0) {
        std::cerr << "Command execution failed with code " << result << std::endl;
        return false;
    }
    
    return true;
}

void ResponseProtocol::cleanupAnomalies() {
    std::lock_guard<std::mutex> lock(m_anomalyMutex);
    
    auto now = std::chrono::system_clock::now();
    auto it = m_activeAnomalies.begin();
    
    while (it != m_activeAnomalies.end()) {
        // If last detection was more than 2 minutes ago, remove it
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastDetectedTime).count();
            
        if (elapsed > 120) {
            it = m_activeAnomalies.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace nx_agent
