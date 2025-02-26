// nx_agent_llm.cpp
#include "nx_agent_llm.h"
#include "nx_agent_metadata.h"
#include "nx_agent_config.h"
#include "nx_agent_utils.h"

#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

namespace nx_agent {

using json = nlohmann::json;

// Callback for CURL response data
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

// ContextItem implementation
ContextItem ContextItem::fromDetectedObject(const DetectedObject& obj) {
    ContextItem item;
    item.type = Type::OBJECT_DETECTION;
    
    std::ostringstream desc;
    desc << "Detected " << obj.typeId << " with confidence " 
         << std::fixed << std::setprecision(2) << obj.confidence;
    
    // Add position information
    desc << " at position [x:" << obj.boundingBox.x 
         << ", y:" << obj.boundingBox.y 
         << ", width:" << obj.boundingBox.width 
         << ", height:" << obj.boundingBox.height << "]";
    
    // Add recognition status if available
    auto it = obj.attributes.find("recognitionStatus");
    if (it != obj.attributes.end()) {
        desc << " (Recognition: " << it->second << ")";
    }
    
    item.description = desc.str();
    item.timestampUs = obj.timestampUs;
    item.confidence = obj.confidence;
    
    // Add all attributes to metadata
    json metadata;
    metadata["objectType"] = obj.typeId;
    metadata["trackId"] = obj.trackId;
    metadata["boundingBox"] = {
        {"x", obj.boundingBox.x},
        {"y", obj.boundingBox.y},
        {"width", obj.boundingBox.width},
        {"height", obj.boundingBox.height}
    };
    
    for (const auto& attr : obj.attributes) {
        metadata["attributes"][attr.first] = attr.second;
    }
    
    item.metadata = metadata;
    
    return item;
}

ContextItem ContextItem::fromAnalysisResult(const FrameAnalysisResult& result) {
    ContextItem item;
    
    if (result.isAnomaly) {
        item.type = Type::ANOMALY_DETECTION;
        item.description = "Anomaly detected: " + result.anomalyType + " - " + result.anomalyDescription;
        item.confidence = result.anomalyScore;
    } else if (result.motionInfo.overallMotionLevel > 0.05f) {
        item.type = Type::MOTION_EVENT;
        item.description = "Motion detected with level " + 
                          std::to_string(result.motionInfo.overallMotionLevel);
        item.confidence = result.motionInfo.overallMotionLevel;
    } else {
        item.type = Type::ENVIRONMENT_INFO;
        item.description = "Normal scene activity";
        item.confidence = 1.0f - result.anomalyScore;
    }
    
    item.timestampUs = result.timestampUs;
    
    // Add metadata
    json metadata;
    metadata["timestampUs"] = result.timestampUs;
    metadata["timeFormatted"] = TimeUtils::formatTimestamp(result.timestampUs);
    metadata["anomalyScore"] = result.anomalyScore;
    metadata["anomalyType"] = result.anomalyType;
    metadata["anomalyDescription"] = result.anomalyDescription;
    metadata["isAnomaly"] = result.isAnomaly;
    metadata["motionLevel"] = result.motionInfo.overallMotionLevel;
    
    // Object counts
    int personCount = 0;
    int unknownPersonCount = 0;
    int vehicleCount = 0;
    
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
    
    metadata["objectCounts"] = {
        {"person", personCount},
        {"unknownPerson", unknownPersonCount},
        {"vehicle", vehicleCount},
        {"total", result.objects.size()}
    };
    
    item.metadata = metadata;
    
    return item;
}

std::string ContextItem::toString() const {
    std::ostringstream oss;
    
    // Format timestamp
    std::string timeStr = TimeUtils::formatTimestamp(timestampUs);
    
    // Format type
    std::string typeStr;
    switch (type) {
        case Type::OBJECT_DETECTION:
            typeStr = "OBJECT";
            break;
        case Type::MOTION_EVENT:
            typeStr = "MOTION";
            break;
        case Type::ANOMALY_DETECTION:
            typeStr = "ANOMALY";
            break;
        case Type::ENVIRONMENT_INFO:
            typeStr = "INFO";
            break;
        case Type::HISTORICAL_PATTERN:
            typeStr = "PATTERN";
            break;
        case Type::CROSS_CAMERA_INFO:
            typeStr = "CROSS-CAM";
            break;
        case Type::SYSTEM_EVENT:
            typeStr = "SYSTEM";
            break;
        default:
            typeStr = "UNKNOWN";
    }
    
    oss << "[" << timeStr << "] [" << typeStr << "] " << description;
    
    return oss.str();
}

// LLMRequest implementation
LLMRequest::LLMRequest(const std::string& deviceId, Type type, Priority priority)
    : deviceId(deviceId),
      requestType(type),
      priority(priority),
      requestTimeUs(TimeUtils::getCurrentTimestampUs())
{
}

void LLMRequest::addContextItem(const ContextItem& item) {
    contextItems.push_back(item);
}

std::string LLMRequest::generatePrompt() const {
    std::ostringstream oss;
    
    // Add request type
    switch (requestType) {
        case Type::ANOMALY_ANALYSIS:
            oss << "TASK: Analyze the anomaly detected in the security camera and provide context.\n\n";
            break;
        case Type::SITUATION_ASSESSMENT:
            oss << "TASK: Assess the overall situation in the security camera view.\n\n";
            break;
        case Type::RESPONSE_PLANNING:
            oss << "TASK: Plan an appropriate response to the situation in the security camera.\n\n";
            break;
        case Type::PREDICTIVE_ANALYSIS:
            oss << "TASK: Predict potential future behavior based on the observed activity.\n\n";
            break;
        case Type::CROSS_CAMERA_ANALYSIS:
            oss << "TASK: Analyze information from multiple cameras to understand the overall security situation.\n\n";
            break;
    }
    
    // Add current time
    oss << "CURRENT TIME: " << TimeUtils::formatTimestamp(requestTimeUs) << "\n\n";
    
    // Add context items
    oss << "CONTEXT:\n";
    
    // Sort by timestamp (oldest first)
    auto sortedItems = contextItems;
    std::sort(sortedItems.begin(), sortedItems.end(), 
              [](const ContextItem& a, const ContextItem& b) {
                  return a.timestampUs < b.timestampUs;
              });
    
    for (const auto& item : sortedItems) {
        oss << "- " << item.toString() << "\n";
    }
    
    // Add specific instructions based on request type
    oss << "\nINSTRUCTIONS:\n";
    
    switch (requestType) {
        case Type::ANOMALY_ANALYSIS:
            oss << "1. Analyze the anomaly described in the context.\n"
                << "2. Determine the potential security implications.\n"
                << "3. Assess whether this might be a false alarm or a genuine security concern.\n"
                << "4. Provide reasoning for your assessment.\n"
                << "5. Recommend whether this requires human attention.\n";
            break;
        case Type::SITUATION_ASSESSMENT:
            oss << "1. Assess the overall situation in the camera view.\n"
                << "2. Identify any potential security concerns.\n"
                << "3. Consider the time of day and normal patterns for this location.\n"
                << "4. Determine the level of concern (Normal, Low, Medium, High).\n"
                << "5. Provide reasoning for your assessment.\n";
            break;
        case Type::RESPONSE_PLANNING:
            oss << "1. Analyze the security situation described in the context.\n"
                << "2. Determine the appropriate security response level.\n"
                << "3. Suggest specific actions that should be taken.\n"
                << "4. Prioritize these actions.\n"
                << "5. Provide reasoning for your recommendations.\n";
            break;
        case Type::PREDICTIVE_ANALYSIS:
            oss << "1. Analyze the patterns of behavior described in the context.\n"
                << "2. Predict what might happen next based on these patterns.\n"
                << "3. Identify potential security implications of these predictions.\n"
                << "4. Assign confidence levels to your predictions.\n"
                << "5. Suggest what to monitor or look for to confirm your predictions.\n";
            break;
        case Type::CROSS_CAMERA_ANALYSIS:
            oss << "1. Analyze information from multiple cameras to understand the overall situation.\n"
                << "2. Identify any connections or patterns across different camera views.\n"
                << "3. Determine if there are coordinated activities happening.\n"
                << "4. Assess the overall security implications.\n"
                << "5. Recommend cameras to focus on and what to look for.\n";
            break;
    }
    
    oss << "\nOUTPUT FORMAT:\n"
        << "Provide your response in JSON format with the following structure:\n"
        << "{\n"
        << "  \"reasoning\": \"Your detailed analysis and reasoning\",\n"
        << "  \"confidenceScore\": 0.0-1.0,\n"
        << "  \"actions\": [\n"
        << "    {\n"
        << "      \"type\": \"One of: MONITOR, ALERT, TRACK, ANALYZE_FURTHER, CROSS_REFERENCE, PREDICT, RECOMMEND\",\n"
        << "      \"description\": \"Description of the action\",\n"
        << "      \"confidence\": 0.0-1.0,\n"
        << "      \"parameters\": {}\n"
        << "    }\n"
        << "  ]\n"
        << "}\n";
    
    return oss.str();
}

// LLMResponse implementation
LLMResponse LLMResponse::parseFromLLM(const std::string& llmOutput) {
    LLMResponse response;
    response.responseTimeUs = TimeUtils::getCurrentTimestampUs();
    
    try {
        // Extract JSON from the LLM output
        // Look for JSON blocks between ``` or just try to parse the whole thing
        std::string jsonStr = llmOutput;
        
        // Try to find JSON between code blocks
        std::regex jsonRegex("```(?:json)?\\s*(\\{.*?\\})\\s*```");
        std::smatch match;
        if (std::regex_search(llmOutput, match, jsonRegex)) {
            jsonStr = match[1].str();
        }
        
        // Parse the JSON
        json j = json::parse(jsonStr);
        
        // Extract fields
        response.reasoning = j["reasoning"].get<std::string>();
        response.confidenceScore = j["confidenceScore"].get<float>();
        
        // Parse actions
        for (const auto& actionJson : j["actions"]) {
            Action action;
            
            // Parse action type
            std::string typeStr = actionJson["type"].get<std::string>();
            if (typeStr == "MONITOR") {
                action.type = Action::Type::MONITOR;
            } else if (typeStr == "ALERT") {
                action.type = Action::Type::ALERT;
            } else if (typeStr == "TRACK") {
                action.type = Action::Type::TRACK;
            } else if (typeStr == "ANALYZE_FURTHER") {
                action.type = Action::Type::ANALYZE_FURTHER;
            } else if (typeStr == "CROSS_REFERENCE") {
                action.type = Action::Type::CROSS_REFERENCE;
            } else if (typeStr == "PREDICT") {
                action.type = Action::Type::PREDICT;
            } else if (typeStr == "RECOMMEND") {
                action.type = Action::Type::RECOMMEND;
            } else {
                // Default to MONITOR if unknown
                action.type = Action::Type::MONITOR;
            }
            
            action.description = actionJson["description"].get<std::string>();
            action.confidence = actionJson["confidence"].get<float>();
            
            if (actionJson.contains("parameters")) {
                action.parameters = actionJson["parameters"];
            }
            
            response.actions.push_back(action);
        }
        
        response.success = true;
    } catch (const std::exception& e) {
        response.success = false;
        response.errorMessage = "Failed to parse LLM response: " + std::string(e.what());
        Logger::error("LLMResponse", response.errorMessage);
    }
    
    return response;
}

// LLMManager implementation
LLMManager::LLMManager()
    : m_modelName("claude-3-haiku-20240307"),
      m_apiEndpoint("https://api.anthropic.com/v1/messages"),
      m_maxTokens(4096),
      m_temperature(0.7f),
      m_running(false)
{
    // Initialize CURL globally
    curl_global_init(CURL_GLOBAL_ALL);
}

LLMManager::~LLMManager() {
    // Stop the worker thread
    stop();
    
    // Clean up CURL
    curl_global_cleanup();
}

bool LLMManager::initialize(const std::string& apiKey, const std::string& modelName) {
    m_apiKey = apiKey;
    m_modelName = modelName;
    
    // Start the worker thread
    start();
    
    return true;
}

std::future<LLMResponse> LLMManager::submitRequest(const LLMRequest& request) {
    // Create a promise and future
    auto promise = std::make_shared<std::promise<LLMResponse>>();
    std::future<LLMResponse> future = promise->get_future();
    
    // Create a copy of the request
    LLMRequest requestCopy = request;
    
    // Add callback to fulfill the promise
    requestCopy.callback = [promise](const std::string& response) {
        promise->set_value(LLMResponse::parseFromLLM(response));
    };
    
    // Add to queue
    submitRequestWithCallback(std::move(requestCopy));
    
    return future;
}

void LLMManager::submitRequestWithCallback(LLMRequest request) {
    // Add to queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_requestQueue.push(std::move(request));
    }
    
    // Notify worker thread
    m_queueCondition.notify_one();
}

LLMResponse LLMManager::processAnalysisResult(const std::string& deviceId, const FrameAnalysisResult& result) {
    // Create a request for anomaly analysis
    LLMRequest request(deviceId, LLMRequest::Type::ANOMALY_ANALYSIS);
    
    // Add current result as context
    request.addContextItem(ContextItem::fromAnalysisResult(result));
    
    // Add all objects as context
    for (const auto& obj : result.objects) {
        request.addContextItem(ContextItem::fromDetectedObject(obj));
    }
    
    // Submit the request and wait for response
    auto future = submitRequest(request);
    return future.get();
}

LLMResponse LLMManager::generateResponsePlan(const std::string& deviceId, const FrameAnalysisResult& result) {
    // Create a request for response planning
    LLMRequest request(deviceId, LLMRequest::Type::RESPONSE_PLANNING);
    
    // Add current result as context
    request.addContextItem(ContextItem::fromAnalysisResult(result));
    
    // Add all objects as context
    for (const auto& obj : result.objects) {
        request.addContextItem(ContextItem::fromDetectedObject(obj));
    }
    
    // Submit the request and wait for response
    auto future = submitRequest(request);
    return future.get();
}

LLMResponse LLMManager::predictBehavior(const std::string& deviceId, const std::vector<FrameAnalysisResult>& history) {
    // Create a request for predictive analysis
    LLMRequest request(deviceId, LLMRequest::Type::PREDICTIVE_ANALYSIS);
    
    // Add history as context
    for (const auto& result : history) {
        request.addContextItem(ContextItem::fromAnalysisResult(result));
    }
    
    // Submit the request and wait for response
    auto future = submitRequest(request);
    return future.get();
}

void LLMManager::start() {
    if (m_running) {
        return;
    }
    
    m_running = true;
    m_workerThread = std::thread(&LLMManager::workerFunction, this);
}

void LLMManager::stop() {
    if (!m_running) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_running = false;
    }
    
    m_queueCondition.notify_all();
    
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void LLMManager::configure(const nlohmann::json& config) {
    // Update configuration
    if (config.contains("apiKey")) {
        m_apiKey = config["apiKey"].get<std::string>();
    }
    
    if (config.contains("modelName")) {
        m_modelName = config["modelName"].get<std::string>();
    }
    
    if (config.contains("apiEndpoint")) {
        m_apiEndpoint = config["apiEndpoint"].get<std::string>();
    }
    
    if (config.contains("maxTokens")) {
        m_maxTokens = config["maxTokens"].get<int>();
    }
    
    if (config.contains("temperature")) {
        m_temperature = config["temperature"].get<float>();
    }
}

void LLMManager::workerFunction() {
    while (m_running) {
        LLMRequest request;
        
        // Get a request from the queue
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            // Wait for a request or stop signal
            m_queueCondition.wait(lock, [this]() {
                return !m_requestQueue.empty() || !m_running;
            });
            
            // Check if we should stop
            if (!m_running) {
                break;
            }
            
            // Get the next request
            request = std::move(m_requestQueue.front());
            m_requestQueue.pop();
        }
        
        // Process the request
        try {
            // Generate prompt
            std::string prompt = request.generatePrompt();
            
            // Send to LLM
            std::string response = sendRequest(prompt);
            
            // Call callback
            if (request.callback) {
                request.callback(response);
            }
        } catch (const std::exception& e) {
            Logger::error("LLMManager", "Error processing request: " + std::string(e.what()));
            
            // Call callback with error
            if (request.callback) {
                request.callback("{\"error\": \"" + std::string(e.what()) + "\"}");
            }
        }
    }
}

std::string LLMManager::sendRequest(const std::string& prompt) {
    // Prepare request to Anthropic API
    json requestJson = {
        {"model", m_modelName},
        {"max_tokens", m_maxTokens},
        {"temperature", m_temperature},
        {"messages", json::array({{
            {"role", "user"},
            {"content", prompt}
        }})}
    };
    
    // Add system prompt based on request type
    std::string systemPrompt = "You are an AI security analyst integrated with a surveillance system. Analyze provided information and respond with actionable security insights.";
    
    requestJson["system"] = systemPrompt;
    
    // Convert to string
    std::string requestData = requestJson.dump();
    
    // Send HTTP request
    std::string response = makeHttpRequest(m_apiEndpoint, requestData);
    
    // Parse response to extract content
    try {
        json responseJson = json::parse(response);
        if (responseJson.contains("content") && 
            responseJson["content"].is_array() && 
            !responseJson["content"].empty() && 
            responseJson["content"][0].contains("text")) {
            
            return responseJson["content"][0]["text"].get<std::string>();
        }
    } catch (const std::exception& e) {
        Logger::error("LLMManager", "Error parsing API response: " + std::string(e.what()));
    }
    
    return response;
}

std::string LLMManager::makeHttpRequest(const std::string& url, const std::string& data) {
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }
    
    try {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        // Set headers
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string authHeader = "X-API-Key: " + m_apiKey;
        headers = curl_slist_append(headers, authHeader.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        // Set POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        
        // Set write callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        // Perform request
        CURLcode res = curl_easy_perform(curl);
        
        // Check for errors
        if (res != CURLE_OK) {
            throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));
        }
        
        // Check HTTP status code
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        
        if (httpCode < 200 || httpCode >= 300) {
            throw std::runtime_error("HTTP error: " + std::to_string(httpCode) + "\nResponse: " + response);
        }
        
        // Clean up
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        return response;
    } catch (...) {
        // Ensure cleanup even if error occurs
        curl_easy_cleanup(curl);
        throw;
    }
}

LLMResponse LLMManager::processLLMResponse(const std::string& response, const LLMRequest& request) {
    return LLMResponse::parseFromLLM(response);
}

std::string LLMManager::generateSystemPrompt(const LLMRequest::Type& requestType) const {
    switch (requestType) {
        case LLMRequest::Type::ANOMALY_ANALYSIS:
            return "You are an AI security analyst specializing in anomaly detection. "
                   "Analyze security camera anomalies and provide clear assessment of threats.";
        
        case LLMRequest::Type::SITUATION_ASSESSMENT:
            return "You are an AI security situation analyst. "
                   "Assess overall security situations from camera feeds and provide comprehensive situation awareness.";
        
        case LLMRequest::Type::RESPONSE_PLANNING:
            return "You are an AI security response planner. "
                   "Create strategic response plans for security situations that balance caution with appropriate action.";
        
        case LLMRequest::Type::PREDICTIVE_ANALYSIS:
            return "You are an AI security predictive analyst. "
                   "Predict future behaviors and potential security implications based on observed patterns.";
        
        case LLMRequest::Type::CROSS_CAMERA_ANALYSIS:
            return "You are an AI security correlation specialist. "
                   "Analyze information across multiple cameras to identify connections and coordinated activities.";
        
        default:
            return "You are an AI security analyst integrated with a surveillance system. "
                   "Analyze provided information and respond with actionable security insights.";
    }
}

// ContextManager implementation
ContextManager::ContextManager(const std::string& deviceId)
    : m_deviceId(deviceId)
{
}

void ContextManager::addContextItem(const ContextItem& item) {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    
    m_contextItems.push_back(item);
    
    // Maintain maximum size
    if (m_contextItems.size() > MAX_CONTEXT_ITEMS) {
        m_contextItems.erase(m_contextItems.begin());
    }
}

std::vector<ContextItem> ContextManager::getRecentContext(int maxItems) const {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    
    if (maxItems >= m_contextItems.size()) {
        return m_contextItems;
    }
    
    return std::vector<ContextItem>(
        m_contextItems.end() - maxItems,
        m_contextItems.end()
    );
}

std::vector<ContextItem> ContextManager::getContextForTimeRange(int64_t startTimeUs, int64_t endTimeUs) const {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    
    std::vector<ContextItem> result;
    
    for (const auto& item : m_contextItems) {
        if (item.timestampUs >= startTimeUs && item.timestampUs <= endTimeUs) {
            result.push_back(item);
        }
    }
    
    return result;
}

std::vector<ContextItem> ContextManager::getContextForObject(const std::string& objectId) const {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    
    std::vector<ContextItem> result;
    
    for (const auto& item : m_contextItems) {
        if (item.type == ContextItem::Type::OBJECT_DETECTION) {
            if (item.metadata.contains("trackId") && 
                item.metadata["trackId"].get<std::string>() == objectId) {
                result.push_back(item);
            }
        }
    }
    
    return result;
}

void ContextManager::clearOldContext(int64_t olderThanUs) {
    std::lock_guard<std::mutex> lock(m_contextMutex);
    
    m_contextItems.erase(
        std::remove_if(
            m_contextItems.begin(),
            m_contextItems.end(),
            [olderThanUs](const ContextItem& item) {
                return item.timestampUs < olderThanUs;
            }
        ),
        m_contextItems.end()
    );
}

} // namespace nx_agent
