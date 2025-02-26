// nx_agent_reasoning.cpp
#include "nx_agent_reasoning.h"
#include "nx_agent_llm.h"
#include "nx_agent_strategy.h"
#include "nx_agent_metadata.h"
#include "nx_agent_utils.h"

#include <algorithm>
#include <random>
#include <sstream>
#include <chrono>
#include <regex>
#include <set>

namespace nx_agent {

using json = nlohmann::json;

// KnowledgeItem implementation
json KnowledgeItem::toJson() const {
    json j;
    j["id"] = id;
    j["type"] = static_cast<int>(type);
    j["content"] = content;
    j["confidence"] = confidence;
    j["timestampUs"] = timestampUs;
    j["source"] = source;
    j["relatedItems"] = relatedItems;
    return j;
}

KnowledgeItem KnowledgeItem::fromJson(const json& j) {
    KnowledgeItem item;
    item.id = j["id"];
    item.type = static_cast<Type>(j["type"].get<int>());
    item.content = j["content"];
    item.confidence = j["confidence"];
    item.timestampUs = j["timestampUs"];
    item.source = j["source"];
    
    if (j.contains("relatedItems") && j["relatedItems"].is_array()) {
        for (const auto& relatedItem : j["relatedItems"]) {
            item.relatedItems.push_back(relatedItem.get<std::string>());
        }
    }
    
    return item;
}

// Goal implementation
json Goal::toJson() const {
    json j;
    j["id"] = id;
    j["type"] = static_cast<int>(type);
    j["description"] = description;
    j["status"] = static_cast<int>(status);
    j["priority"] = static_cast<int>(priority);
    j["creationTimeUs"] = creationTimeUs;
    j["deadlineUs"] = deadlineUs;
    j["parentGoalId"] = parentGoalId;
    j["subGoalIds"] = subGoalIds;
    j["dependsOnGoalIds"] = dependsOnGoalIds;
    j["parameters"] = parameters;
    j["progress"] = progress;
    j["lastUpdateTimeUs"] = lastUpdateTimeUs;
    j["resultDescription"] = resultDescription;
    return j;
}

Goal Goal::fromJson(const json& j) {
    Goal goal;
    goal.id = j["id"];
    goal.type = static_cast<Type>(j["type"].get<int>());
    goal.description = j["description"];
    goal.status = static_cast<Status>(j["status"].get<int>());
    goal.priority = static_cast<Priority>(j["priority"].get<int>());
    goal.creationTimeUs = j["creationTimeUs"];
    goal.deadlineUs = j["deadlineUs"];
    goal.parentGoalId = j["parentGoalId"];
    
    if (j.contains("subGoalIds") && j["subGoalIds"].is_array()) {
        for (const auto& subGoalId : j["subGoalIds"]) {
            goal.subGoalIds.push_back(subGoalId.get<std::string>());
        }
    }
    
    if (j.contains("dependsOnGoalIds") && j["dependsOnGoalIds"].is_array()) {
        for (const auto& depGoalId : j["dependsOnGoalIds"]) {
            goal.dependsOnGoalIds.push_back(depGoalId.get<std::string>());
        }
    }
    
    goal.parameters = j["parameters"];
    goal.progress = j["progress"];
    goal.lastUpdateTimeUs = j["lastUpdateTimeUs"];
    goal.resultDescription = j["resultDescription"];
    
    return goal;
}

// Reasoning implementation
json Reasoning::toJson() const {
    json j;
    j["id"] = id;
    j["type"] = static_cast<int>(type);
    j["description"] = description;
    j["inputs"] = inputs;
    j["outputs"] = outputs;
    j["startTimeUs"] = startTimeUs;
    j["endTimeUs"] = endTimeUs;
    j["confidence"] = confidence;
    j["alternativeConsidered"] = alternativeConsidered;
    j["reasoning"] = reasoning;
    return j;
}

Reasoning Reasoning::fromJson(const json& j) {
    Reasoning reasoning;
    reasoning.id = j["id"];
    reasoning.type = static_cast<Type>(j["type"].get<int>());
    reasoning.description = j["description"];
    
    if (j.contains("inputs") && j["inputs"].is_array()) {
        for (const auto& input : j["inputs"]) {
            reasoning.inputs.push_back(input.get<std::string>());
        }
    }
    
    if (j.contains("outputs") && j["outputs"].is_array()) {
        for (const auto& output : j["outputs"]) {
            reasoning.outputs.push_back(output.get<std::string>());
        }
    }
    
    reasoning.startTimeUs = j["startTimeUs"];
    reasoning.endTimeUs = j["endTimeUs"];
    reasoning.confidence = j["confidence"];
    
    if (j.contains("alternativeConsidered") && j["alternativeConsidered"].is_array()) {
        for (const auto& alt : j["alternativeConsidered"]) {
            reasoning.alternativeConsidered.push_back(alt.get<std::string>());
        }
    }
    
    reasoning.reasoning = j["reasoning"];
    
    return reasoning;
}

// Action implementation
json Action::toJson() const {
    json j;
    j["id"] = id;
    j["type"] = static_cast<int>(type);
    j["description"] = description;
    j["status"] = static_cast<int>(status);
    j["goalId"] = goalId;
    j["creationTimeUs"] = creationTimeUs;
    j["startTimeUs"] = startTimeUs;
    j["completionTimeUs"] = completionTimeUs;
    j["priority"] = priority;
    j["expectedUtility"] = expectedUtility;
    j["parameters"] = parameters;
    j["result"] = result;
    return j;
}

Action Action::fromJson(const json& j) {
    Action action;
    action.id = j["id"];
    action.type = static_cast<Type>(j["type"].get<int>());
    action.description = j["description"];
    action.status = static_cast<Status>(j["status"].get<int>());
    action.goalId = j["goalId"];
    action.creationTimeUs = j["creationTimeUs"];
    action.startTimeUs = j["startTimeUs"];
    action.completionTimeUs = j["completionTimeUs"];
    action.priority = j["priority"];
    action.expectedUtility = j["expectedUtility"];
    action.parameters = j["parameters"];
    action.result = j["result"];
    return action;
}

// ReasoningSystem implementation
ReasoningSystem::ReasoningSystem(const std::string& systemId)
    : m_systemId(systemId),
      m_running(false)
{
}

ReasoningSystem::~ReasoningSystem() {
    // Stop worker thread
    m_running = false;
    m_taskCondition.notify_all();
    
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

bool ReasoningSystem::initialize(std::shared_ptr<LLMManager> llmManager,
                                std::shared_ptr<ContextManager> contextManager, 
                                std::shared_ptr<StrategyManager> strategyManager) {
    m_llmManager = llmManager;
    m_contextManager = contextManager;
    m_strategyManager = strategyManager;
    
    // Start worker thread
    m_running = true;
    m_workerThread = std::thread(&ReasoningSystem::workerFunction, this);
    
    // Add initial goals
    addGoal(Goal::Type::MONITOR, "Monitor security cameras for anomalies", Goal::Priority::MEDIUM);
    addGoal(Goal::Type::OPTIMIZE, "Optimize system performance and reduce false alarms", Goal::Priority::LOW);
    
    return true;
}

void ReasoningSystem::processAnalysisResult(const std::string& deviceId, const FrameAnalysisResult& result) {
    // Create a task to process the analysis result
    Task task;
    task.type = Task::Type::PROCESS_ANALYSIS;
    task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    task.priority = result.isAnomaly ? 10 : 5; // Higher priority for anomalies
    
    // Store relevant data in parameters
    task.parameters["deviceId"] = deviceId;
    task.parameters["timestampUs"] = result.timestampUs;
    task.parameters["isAnomaly"] = result.isAnomaly;
    task.parameters["anomalyScore"] = result.anomalyScore;
    task.parameters["anomalyType"] = result.anomalyType;
    task.parameters["anomalyDescription"] = result.anomalyDescription;
    
    // Add object information
    json objectsJson = json::array();
    for (const auto& obj : result.objects) {
        json objJson;
        objJson["typeId"] = obj.typeId;
        objJson["confidence"] = obj.confidence;
        objJson["trackId"] = obj.trackId;
        objJson["boundingBox"] = {
            {"x", obj.boundingBox.x},
            {"y", obj.boundingBox.y},
            {"width", obj.boundingBox.width},
            {"height", obj.boundingBox.height}
        };
        
        // Add attributes
        json attrsJson = json::object();
        for (const auto& attr : obj.attributes) {
            attrsJson[attr.first] = attr.second;
        }
        objJson["attributes"] = attrsJson;
        
        objectsJson.push_back(objJson);
    }
    task.parameters["objects"] = objectsJson;
    
    // Add motion information
    task.parameters["motionLevel"] = result.motionInfo.overallMotionLevel;
    
    // Add to task queue
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
    }
    
    // Notify worker thread
    m_taskCondition.notify_one();
}

std::string ReasoningSystem::generateCognitiveStatus() {
    // Get current time
    int64_t currentTimeUs = TimeUtils::getCurrentTimestampUs();
    
    // Collect active goals
    std::vector<Goal> activeGoals = getActiveGoals();
    
    // Collect ongoing actions
    std::vector<Action> ongoingActions = getOngoingActions();
    
    // Use LLM to generate a summary of current cognitive state
    if (m_llmManager) {
        LLMRequest request("SYSTEM", LLMRequest::Type::SITUATION_ASSESSMENT);
        
        // Add goals as context
        for (const auto& goal : activeGoals) {
            ContextItem item;
            item.type = ContextItem::Type::ENVIRONMENT_INFO;
            item.description = "Goal: " + goal.description + " (Priority: " + 
                              std::to_string(static_cast<int>(goal.priority)) + ")";
            item.timestampUs = goal.lastUpdateTimeUs;
            item.confidence = 1.0f;
            item.metadata = goal.toJson();
            
            request.addContextItem(item);
        }
        
        // Add actions as context
        for (const auto& action : ongoingActions) {
            ContextItem item;
            item.type = ContextItem::Type::ENVIRONMENT_INFO;
            item.description = "Action: " + action.description + " (Priority: " + 
                              std::to_string(action.priority) + ")";
            item.timestampUs = action.creationTimeUs;
            item.confidence = 1.0f;
            item.metadata = action.toJson();
            
            request.addContextItem(item);
        }
        
        // Add recent knowledge items
        std::vector<KnowledgeItem> recentKnowledge = queryKnowledge("", 10);
        for (const auto& knowledge : recentKnowledge) {
            ContextItem item;
            item.type = ContextItem::Type::ENVIRONMENT_INFO;
            item.description = "Knowledge: " + knowledge.content;
            item.timestampUs = knowledge.timestampUs;
            item.confidence = knowledge.confidence;
            item.metadata = knowledge.toJson();
            
            request.addContextItem(item);
        }
        
        // Send request to LLM
        auto future = m_llmManager->submitRequest(request);
        auto response = future.get();
        
        if (response.success) {
            return response.reasoning;
        }
    }
    
    // Fallback to generic status if LLM fails
    std::ostringstream oss;
    oss << "Cognitive Status at " << TimeUtils::formatTimestamp(currentTimeUs) << "\n\n";
    
    oss << "Active Goals (" << activeGoals.size() << "):\n";
    for (const auto& goal : activeGoals) {
        oss << "- " << goal.description << " (Priority: " 
            << static_cast<int>(goal.priority) << ", Progress: " 
            << (goal.progress * 100) << "%)\n";
    }
    
    oss << "\nOngoing Actions (" << ongoingActions.size() << "):\n";
    for (const auto& action : ongoingActions) {
        oss << "- " << action.description << " (Priority: " << action.priority << ")\n";
    }
    
    oss << "\nRecent Knowledge:\n";
    std::vector<KnowledgeItem> recentKnowledge = queryKnowledge("", 5);
    for (const auto& knowledge : recentKnowledge) {
        oss << "- " << knowledge.content << " (Confidence: " << knowledge.confidence << ")\n";
    }
    
    return oss.str();
}

std::string ReasoningSystem::addGoal(Goal::Type type, 
                                   const std::string& description,
                                   Goal::Priority priority) {
    Goal goal;
    goal.id = generateUniqueId("GOAL");
    goal.type = type;
    goal.description = description;
    goal.status = Goal::Status::PENDING;
    goal.priority = priority;
    goal.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    goal.lastUpdateTimeUs = goal.creationTimeUs;
    goal.progress = 0.0f;
    
    // Add to goals map
    {
        std::lock_guard<std::mutex> lock(m_goalMutex);
        m_goals[goal.id] = goal;
    }
    
    // Create a task to evaluate goals
    Task task;
    task.type = Task::Type::EVALUATE_GOALS;
    task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    task.priority = 5;
    
    // Add to task queue
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
    }
    
    // Notify worker thread
    m_taskCondition.notify_one();
    
    return goal.id;
}

bool ReasoningSystem::updateGoalStatus(const std::string& goalId, Goal::Status status) {
    std::lock_guard<std::mutex> lock(m_goalMutex);
    
    auto it = m_goals.find(goalId);
    if (it == m_goals.end()) {
        return false;
    }
    
    it->second.status = status;
    it->second.lastUpdateTimeUs = TimeUtils::getCurrentTimestampUs();
    
    if (status == Goal::Status::ACHIEVED || status == Goal::Status::FAILED) {
        it->second.progress = 1.0f;
    }
    
    return true;
}

std::vector<Goal> ReasoningSystem::getActiveGoals() const {
    std::vector<Goal> activeGoals;
    
    std::lock_guard<std::mutex> lock(m_goalMutex);
    
    for (const auto& pair : m_goals) {
        if (pair.second.isActive()) {
            activeGoals.push_back(pair.second);
        }
    }
    
    // Sort by priority (highest first)
    std::sort(activeGoals.begin(), activeGoals.end(), 
              [](const Goal& a, const Goal& b) {
                  return static_cast<int>(a.priority) < static_cast<int>(b.priority);
              });
    
    return activeGoals;
}

std::string ReasoningSystem::addKnowledgeItem(KnowledgeItem::Type type,
                                            const std::string& content,
                                            float confidence,
                                            const std::string& source) {
    KnowledgeItem item;
    item.id = generateUniqueId("KNOW");
    item.type = type;
    item.content = content;
    item.confidence = confidence;
    item.timestampUs = TimeUtils::getCurrentTimestampUs();
    item.source = source;
    
    // Add to knowledge base
    {
        std::lock_guard<std::mutex> lock(m_knowledgeMutex);
        m_knowledgeItems[item.id] = item;
    }
    
    // Create a task to update knowledge
    Task task;
    task.type = Task::Type::UPDATE_KNOWLEDGE;
    task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    task.priority = 3;
    task.parameters["knowledgeId"] = item.id;
    
    // Add to task queue
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
    }
    
    // Notify worker thread
    m_taskCondition.notify_one();
    
    return item.id;
}

std::vector<KnowledgeItem> ReasoningSystem::queryKnowledge(const std::string& query, int maxResults) const {
    std::vector<KnowledgeItem> results;
    
    std::lock_guard<std::mutex> lock(m_knowledgeMutex);
    
    // If query is empty, return most recent items
    if (query.empty()) {
        // Copy all items to a vector
        for (const auto& pair : m_knowledgeItems) {
            results.push_back(pair.second);
        }
        
        // Sort by timestamp (newest first)
        std::sort(results.begin(), results.end(), 
                 [](const KnowledgeItem& a, const KnowledgeItem& b) {
                     return a.timestampUs > b.timestampUs;
                 });
        
        // Limit to maxResults
        if (results.size() > maxResults) {
            results.resize(maxResults);
        }
        
        return results;
    }
    
    // Perform a simple keyword search
    // In a real system, this would use a more sophisticated search
    std::string lowerQuery = StringUtils::toLower(query);
    
    for (const auto& pair : m_knowledgeItems) {
        const auto& item = pair.second;
        std::string lowerContent = StringUtils::toLower(item.content);
        
        if (lowerContent.find(lowerQuery) != std::string::npos) {
            results.push_back(item);
        }
    }
    
    // Sort by relevance (simple exact match count)
    std::sort(results.begin(), results.end(), 
             [&lowerQuery](const KnowledgeItem& a, const KnowledgeItem& b) {
                 // Count occurrences of query in content
                 std::string lowerA = StringUtils::toLower(a.content);
                 std::string lowerB = StringUtils::toLower(b.content);
                 
                 size_t countA = 0;
                 size_t pos = 0;
                 while ((pos = lowerA.find(lowerQuery, pos)) != std::string::npos) {
                     ++countA;
                     pos += lowerQuery.length();
                 }
                 
                 size_t countB = 0;
                 pos = 0;
                 while ((pos = lowerB.find(lowerQuery, pos)) != std::string::npos) {
                     ++countB;
                     pos += lowerQuery.length();
                 }
                 
                 if (countA != countB) {
                     return countA > countB;
                 }
                 
                 // If equal occurrence count, sort by timestamp (newest first)
                 return a.timestampUs > b.timestampUs;
             });
    
    // Limit to maxResults
    if (results.size() > maxResults) {
        results.resize(maxResults);
    }
    
    return results;
}

std::string ReasoningSystem::addReasoning(Reasoning::Type type,
                                        const std::string& description,
                                        const std::vector<std::string>& inputs) {
    Reasoning reasoning;
    reasoning.id = generateUniqueId("REAS");
    reasoning.type = type;
    reasoning.description = description;
    reasoning.inputs = inputs;
    reasoning.startTimeUs = TimeUtils::getCurrentTimestampUs();
    reasoning.confidence = 0.0f; // Will be updated when reasoning completes
    
    // Add to reasoning map
    {
        std::lock_guard<std::mutex> lock(m_reasoningMutex);
        m_reasoningSteps[reasoning.id] = reasoning;
    }
    
    return reasoning.id;
}

std::string ReasoningSystem::createAction(Action::Type type,
                                        const std::string& description,
                                        const std::string& goalId,
                                        float priority,
                                        const json& parameters) {
    Action action;
    action.id = generateUniqueId("ACT");
    action.type = type;
    action.description = description;
    action.status = Action::Status::PENDING;
    action.goalId = goalId;
    action.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    action.startTimeUs = 0;
    action.completionTimeUs = 0;
    action.priority = priority;
    action.expectedUtility = 0.5f; // Default value, would be calculated in a real system
    action.parameters = parameters;
    
    // Add to actions map
    {
        std::lock_guard<std::mutex> lock(m_actionMutex);
        m_actions[action.id] = action;
    }
    
    // Create a task to execute the action
    Task task;
    task.type = Task::Type::EXECUTE_ACTION;
    task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    task.priority = static_cast<int>(priority * 10);
    task.parameters["actionId"] = action.id;
    
    // Add to task queue
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
    }
    
    // Notify worker thread
    m_taskCondition.notify_one();
    
    return action.id;
}

std::vector<Action> ReasoningSystem::getOngoingActions() const {
    std::vector<Action> ongoingActions;
    
    std::lock_guard<std::mutex> lock(m_actionMutex);
    
    for (const auto& pair : m_actions) {
        if (pair.second.status == Action::Status::PENDING ||
            pair.second.status == Action::Status::IN_PROGRESS) {
            ongoingActions.push_back(pair.second);
        }
    }
    
    // Sort by priority (highest first)
    std::sort(ongoingActions.begin(), ongoingActions.end(), 
              [](const Action& a, const Action& b) {
                  return a.priority > b.priority;
              });
    
    return ongoingActions;
}

void ReasoningSystem::executeCognitiveCycle() {
    // Create a reflection task
    Task task;
    task.type = Task::Type::REFLECT;
    task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    task.priority = 1; // Low priority
    
    // Add to task queue
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
    }
    
    // Notify worker thread
    m_taskCondition.notify_one();
}

void ReasoningSystem::executeTask(const Task& task) {
    switch (task.type) {
        case Task::Type::PROCESS_ANALYSIS:
            {
                std::string deviceId = task.parameters["deviceId"];
                
                // Reconstruct FrameAnalysisResult from task parameters
                FrameAnalysisResult result;
                result.timestampUs = task.parameters["timestampUs"];
                result.isAnomaly = task.parameters["isAnomaly"];
                result.anomalyScore = task.parameters["anomalyScore"];
                result.anomalyType = task.parameters["anomalyType"];
                result.anomalyDescription = task.parameters["anomalyDescription"];
                
                // Reconstruct objects
                if (task.parameters.contains("objects") && task.parameters["objects"].is_array()) {
                    for (const auto& objJson : task.parameters["objects"]) {
                        DetectedObject obj;
                        obj.typeId = objJson["typeId"];
                        obj.confidence = objJson["confidence"];
                        obj.trackId = objJson["trackId"];
                        
                        auto bbJson = objJson["boundingBox"];
                        obj.boundingBox = cv::Rect(
                            bbJson["x"], bbJson["y"], bbJson["width"], bbJson["height"]
                        );
                        
                        // Reconstruct attributes
                        if (objJson.contains("attributes") && objJson["attributes"].is_object()) {
                            for (auto it = objJson["attributes"].begin(); it != objJson["attributes"].end(); ++it) {
                                obj.attributes[it.key()] = it.value();
                            }
                        }
                        
                        result.objects.push_back(obj);
                    }
                }
                
                // Set motion level
                result.motionInfo.overallMotionLevel = task.parameters["motionLevel"];
                
                // Process the result
                perceive(deviceId, result);
            }
            break;
            
        case Task::Type::UPDATE_KNOWLEDGE:
            // Update knowledge base
            cognize();
            break;
            
        case Task::Type::EVALUATE_GOALS:
            // Update goals based on current knowledge
            updateGoals();
            break;
            
        case Task::Type::SELECT_ACTIONS:
            // Select and plan actions to achieve goals
            planActions();
            break;
            
        case Task::Type::EXECUTE_ACTION:
            {
                std::string actionId = task.parameters["actionId"];
                
                std::lock_guard<std::mutex> lock(m_actionMutex);
                auto it = m_actions.find(actionId);
                if (it != m_actions.end()) {
                    // Execute the action
                    Action action = it->second;
                    action.status = Action::Status::IN_PROGRESS;
                    action.startTimeUs = TimeUtils::getCurrentTimestampUs();
                    m_actions[actionId] = action;
                    
                    // Release lock before executing
                    lock.unlock();
                    
                    bool success = executeAction(action);
                    
                    // Update action status
                    std::lock_guard<std::mutex> updateLock(m_actionMutex);
                    it = m_actions.find(actionId);
                    if (it != m_actions.end()) {
                        it->second.status = success ? Action::Status::COMPLETED : Action::Status::FAILED;
                        it->second.completionTimeUs = TimeUtils::getCurrentTimestampUs();
                        
                        if (!success) {
                            it->second.result = "Action execution failed";
                        }
                    }
                }
            }
            break;
            
        case Task::Type::REFLECT:
            // Reflect on recent performance
            reflect();
            break;
    }
}

void ReasoningSystem::workerFunction() {
    while (m_running) {
        Task task;
        bool hasTask = false;
        
        // Get a task from the queue
        {
            std::unique_lock<std::mutex> lock(m_taskMutex);
            
            // Wait for a task or stop signal
            m_taskCondition.wait(lock, [this]() {
                return !m_taskQueue.empty() || !m_running;
            });
            
            // Check if we should stop
            if (!m_running) {
                break;
            }
            
            // Get the next task
            if (!m_taskQueue.empty()) {
                task = m_taskQueue.front();
                m_taskQueue.pop();
                hasTask = true;
            }
        }
        
        // Execute the task
        if (hasTask) {
            try {
                executeTask(task);
            } catch (const std::exception& e) {
                Logger::error("ReasoningSystem", "Error executing task: " + std::string(e.what()));
            }
        }
        
        // Clean up old data periodically
        static int64_t lastCleanupUs = 0;
        int64_t currentTimeUs = TimeUtils::getCurrentTimestampUs();
        if (currentTimeUs - lastCleanupUs > 60000000) { // 60 seconds
            cleanupOldData();
            lastCleanupUs = currentTimeUs;
        }
    }
}

std::string ReasoningSystem::generateUniqueId(const std::string& prefix) const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dis(1000, 9999);
    
    // Generate timestamp
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // Generate random digits
    int randomDigits = dis(gen);
    
    // Combine to make unique ID
    std::ostringstream oss;
    oss << prefix << "-" << nowMs << "-" << randomDigits;
    
    return oss.str();
}

void ReasoningSystem::cleanupOldData() {
    int64_t currentTimeUs = TimeUtils::getCurrentTimestampUs();
    
    // Clean up old knowledge items
    {
        std::lock_guard<std::mutex> lock(m_knowledgeMutex);
        
        auto it = m_knowledgeItems.begin();
        while (it != m_knowledgeItems.end()) {
            // Remove items older than 24 hours
            if (currentTimeUs - it->second.timestampUs > 86400000000) {
                it = m_knowledgeItems.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Clean up completed reasoning steps
    {
        std::lock_guard<std::mutex> lock(m_reasoningMutex);
        
        auto it = m_reasoningSteps.begin();
        while (it != m_reasoningSteps.end()) {
            // Remove completed reasoning steps older than 1 hour
            if (it->second.endTimeUs > 0 && currentTimeUs - it->second.endTimeUs > 3600000000) {
                it = m_reasoningSteps.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Clean up completed actions
    {
        std::lock_guard<std::mutex> lock(m_actionMutex);
        
        auto it = m_actions.begin();
        while (it != m_actions.end()) {
            // Remove completed actions older than 1 hour
            if (it->second.isComplete() && currentTimeUs - it->second.completionTimeUs > 3600000000) {
                it = m_actions.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Limit recent states
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        
        while (m_recentStates.size() > 100) {
            m_recentStates.pop_front();
        }
    }
}

void ReasoningSystem::perceive(const std::string& deviceId, const FrameAnalysisResult& result) {
    // Extract facts from the analysis result
    extractFacts(deviceId, result);
    
    // Update the situation model
    updateSituationModel(result);
    
    // Create situation assessment task
    Task task;
    task.type = Task::Type::UPDATE_KNOWLEDGE;
    task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    task.priority = result.isAnomaly ? 7 : 3;
    
    // Add to task queue
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
    }
    
    // Notify worker thread
    m_taskCondition.notify_one();
}

void ReasoningSystem::cognize() {
    // Assess the current situation
    assessSituation();
    
    // Identify potential threats
    identifyThreats();
    
    // Update goals based on current knowledge
    updateGoals();
    
    // Create action planning task
    Task task;
    task.type = Task::Type::SELECT_ACTIONS;
    task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    task.priority = 5;
    
    // Add to task queue
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
    }
    
    // Notify worker thread
    m_taskCondition.notify_one();
}

void ReasoningSystem::act() {
    // Plan actions to achieve goals
    planActions();
    
    // Select the best action
    selectBestAction();
}

void ReasoningSystem::reflect() {
    // Evaluate recent performance
    evaluatePerformance();
    
    // Update strategies based on performance
    updateStrategies();
    
    // Schedule next reflection
    int64_t currentTimeUs = TimeUtils::getCurrentTimestampUs();
    
    // Create a task for next reflection (in 5 minutes)
    Task task;
    task.type = Task::Type::REFLECT;
    task.creationTimeUs = currentTimeUs;
    task.priority = 1;
    task.parameters["scheduledTimeUs"] = currentTimeUs + 300000000;
    
    // Add to task queue
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
    }
}

void ReasoningSystem::extractFacts(const std::string& deviceId, const FrameAnalysisResult& result) {
    // Create knowledge items from the analysis result
    
    // Basic facts about the frame
    std::string frameFactId = addKnowledgeItem(
        KnowledgeItem::Type::OBSERVATION,
        "Frame analyzed from camera " + deviceId + " at " + TimeUtils::formatTimestamp(result.timestampUs),
        1.0f,
        "FrameAnalysis"
    );
    
    std::vector<std::string> relatedItems;
    relatedItems.push_back(frameFactId);
    
    // Motion level fact
    if (result.motionInfo.overallMotionLevel > 0.01f) {
        std::ostringstream oss;
        oss << "Motion detected in camera " << deviceId << " with level " 
            << std::fixed << std::setprecision(2) << result.motionInfo.overallMotionLevel;
        
        addKnowledgeItem(
            KnowledgeItem::Type::OBSERVATION,
            oss.str(),
            result.motionInfo.overallMotionLevel,
            "MotionDetection"
        );
    }
    
    // Object detection facts
    for (const auto& obj : result.objects) {
        std::ostringstream oss;
        oss << "Detected " << obj.typeId << " in camera " << deviceId 
            << " with confidence " << std::fixed << std::setprecision(2) << obj.confidence;
        
        // Add recognition status if available
        auto it = obj.attributes.find("recognitionStatus");
        if (it != obj.attributes.end()) {
            oss << " (" << it->second << ")";
        }
        
        addKnowledgeItem(
            KnowledgeItem::Type::OBSERVATION,
            oss.str(),
            obj.confidence,
            "ObjectDetection"
        );
    }
    
    // Anomaly detection fact
    if (result.isAnomaly) {
        std::ostringstream oss;
        oss << "Anomaly detected in camera " << deviceId << ": " 
            << result.anomalyType << " - " << result.anomalyDescription;
        
        addKnowledgeItem(
            KnowledgeItem::Type::OBSERVATION,
            oss.str(),
            result.anomalyScore,
            "AnomalyDetection"
        );
    }
}

void ReasoningSystem::updateSituationModel(const FrameAnalysisResult& result) {
    // This would update an internal model of the current situation
    // For now, we'll just add inferences to the knowledge base
    
    if (result.isAnomaly) {
        // Make inferences based on the anomaly type
        std::string inference;
        
        if (result.anomalyType == "UnknownVisitor") {
            inference = "Potential security concern: Unknown individual present in monitored area";
            
            addKnowledgeItem(
                KnowledgeItem::Type::INFERENCE,
                inference,
                result.anomalyScore * 0.8f, // Slightly lower confidence than direct observation
                "SituationAnalysis"
            );
        } else if (result.anomalyType == "Loitering") {
            inference = "Suspicious behavior: Subject lingering in area for extended period";
            
            addKnowledgeItem(
                KnowledgeItem::Type::INFERENCE,
                inference,
                result.anomalyScore * 0.8f,
                "SituationAnalysis"
            );
        } else if (result.anomalyType == "AbnormalActivity") {
            inference = "Unusual activity pattern detected: May indicate unauthorized access or behavior";
            
            addKnowledgeItem(
                KnowledgeItem::Type::INFERENCE,
                inference,
                result.anomalyScore * 0.7f,
                "SituationAnalysis"
            );
        }
    }
    
    // Check time of day context
    int64_t timestampUs = result.timestampUs > 0 ? result.timestampUs : TimeUtils::getCurrentTimestampUs();
    auto timePoint = std::chrono::system_clock::time_point(std::chrono::microseconds(timestampUs));
    std::time_t time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm* localTime = std::localtime(&time);
    
    int hour = localTime->tm_hour;
    bool isBusinessHours = (hour >= 9 && hour < 17); // Simplified business hours check
    bool isNighttime = (hour >= 22 || hour < 6);
    
    // Add time context inference
    if (isNighttime && result.motionInfo.overallMotionLevel > 0.1f) {
        addKnowledgeItem(
            KnowledgeItem::Type::INFERENCE,
            "Significant activity detected during nighttime hours - possible off-hours access",
            0.85f,
            "TimeContextAnalysis"
        );
    }
    
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
    
    // Make inferences based on object counts
    if (personCount > 5 && !isBusinessHours) {
        addKnowledgeItem(
            KnowledgeItem::Type::INFERENCE,
            "Unusual number of people detected outside business hours",
            0.75f,
            "OccupancyAnalysis"
        );
    }
    
    if (vehicleCount > 3 && isNighttime) {
        addKnowledgeItem(
            KnowledgeItem::Type::INFERENCE,
            "Multiple vehicles present during nighttime - unusual activity",
            0.8f,
            "VehicleAnalysis"
        );
    }
}

void ReasoningSystem::assessSituation() {
    // Get recent knowledge
    std::vector<KnowledgeItem> recentKnowledge = queryKnowledge("", 20);
    
    // If we have an LLM, use it for situation assessment
    if (m_llmManager && !recentKnowledge.empty()) {
        // Generate reasoning step
        std::string reasoningId = addReasoning(
            Reasoning::Type::SITUATION_ASSESSMENT,
            "Assess current security situation",
            {}
        );
        
        // Generate reasoning with LLM
        Reasoning reasoning = generateReasoningWithLLM(
            Reasoning::Type::SITUATION_ASSESSMENT,
            "What is the current security situation?",
            recentKnowledge
        );
        
        // Update reasoning step
        {
            std::lock_guard<std::mutex> lock(m_reasoningMutex);
            auto it = m_reasoningSteps.find(reasoningId);
            if (it != m_reasoningSteps.end()) {
                it->second.endTimeUs = TimeUtils::getCurrentTimestampUs();
                it->second.confidence = reasoning.confidence;
                it->second.reasoning = reasoning.reasoning;
                it->second.outputs = reasoning.outputs;
            }
        }
        
        // Use the reasoning to update knowledge
        for (const auto& outputId : reasoning.outputs) {
            std::lock_guard<std::mutex> lock(m_knowledgeMutex);
            auto it = m_knowledgeItems.find(outputId);
            if (it != m_knowledgeItems.end()) {
                // Create a task to update goals based on new knowledge
                Task task;
                task.type = Task::Type::EVALUATE_GOALS;
                task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
                task.priority = it->second.confidence > 0.8f ? 8 : 5;
                
                // Add to task queue
                {
                    std::lock_guard<std::mutex> taskLock(m_taskMutex);
                    m_taskQueue.push(task);
                }
                
                // Notify worker thread
                m_taskCondition.notify_one();
            }
        }
    } else {
        // Simple fallback if no LLM available
        int64_t currentTimeUs = TimeUtils::getCurrentTimestampUs();
        
        // Check for anomalies
        bool hasAnomaly = false;
        float maxAnomalyScore = 0.0f;
        std::string anomalyDescription;
        
        for (const auto& item : recentKnowledge) {
            if (item.content.find("Anomaly detected") != std::string::npos) {
                hasAnomaly = true;
                maxAnomalyScore = std::max(maxAnomalyScore, item.confidence);
                anomalyDescription = item.content;
            }
        }
        
        if (hasAnomaly) {
            // Add an assessment to knowledge
            addKnowledgeItem(
                KnowledgeItem::Type::INFERENCE,
                "Security situation assessment: Potential security issue detected. " + anomalyDescription,
                maxAnomalyScore * 0.9f,
                "SituationAssessment"
            );
            
            // Create a task to evaluate goals
            Task task;
            task.type = Task::Type::EVALUATE_GOALS;
            task.creationTimeUs = currentTimeUs;
            task.priority = 8;
            
            // Add to task queue
            {
                std::lock_guard<std::mutex> lock(m_taskMutex);
                m_taskQueue.push(task);
            }
            
            // Notify worker thread
            m_taskCondition.notify_one();
        } else {
            // No anomalies, add a normal assessment
            addKnowledgeItem(
                KnowledgeItem::Type::INFERENCE,
                "Security situation assessment: Normal operations, no significant issues detected.",
                0.9f,
                "SituationAssessment"
            );
        }
    }
}

void ReasoningSystem::identifyThreats() {
    // Get recent knowledge
    std::vector<KnowledgeItem> recentKnowledge = queryKnowledge("", 20);
    
    // Check for potential threats
    std::vector<std::string> threatIndicators = {
        "unknown", "unauthorized", "suspicious", "unusual", 
        "anomaly", "unusual activity", "unexpected"
    };
    
    float maxThreatScore = 0.0f;
    std::string threatDescription;
    
    for (const auto& item : recentKnowledge) {
        std::string lowerContent = StringUtils::toLower(item.content);
        
        for (const auto& indicator : threatIndicators) {
            if (lowerContent.find(indicator) != std::string::npos) {
                float threatScore = item.confidence * 0.8f;
                
                if (threatScore > maxThreatScore) {
                    maxThreatScore = threatScore;
                    threatDescription = item.content;
                }
                
                break;
            }
        }
    }
    
    if (maxThreatScore > 0.5f) {
        // Add threat assessment to knowledge
        addKnowledgeItem(
            KnowledgeItem::Type::INFERENCE,
            "Threat assessment: Potential security threat identified. " + threatDescription,
            maxThreatScore,
            "ThreatAnalysis"
        );
        
        // Create a task to evaluate goals
        Task task;
        task.type = Task::Type::EVALUATE_GOALS;
        task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
        task.priority = 9;
        
        // Add to task queue
        {
            std::lock_guard<std::mutex> lock(m_taskMutex);
            m_taskQueue.push(task);
        }
        
        // Notify worker thread
        m_taskCondition.notify_one();
    }
}

void ReasoningSystem::updateGoals() {
    // Get active goals
    std::vector<Goal> activeGoals = getActiveGoals();
    
    // Get recent knowledge
    std::vector<KnowledgeItem> recentKnowledge = queryKnowledge("", 20);
    
    // Check for security concerns
    bool hasThreat = false;
    bool hasAnomaly = false;
    float maxThreatScore = 0.0f;
    
    for (const auto& item : recentKnowledge) {
        std::string lowerContent = StringUtils::toLower(item.content);
        
        if (lowerContent.find("threat") != std::string::npos) {
            hasThreat = true;
            maxThreatScore = std::max(maxThreatScore, item.confidence);
        } else if (lowerContent.find("anomaly") != std::string::npos) {
            hasAnomaly = true;
            maxThreatScore = std::max(maxThreatScore, item.confidence);
        }
    }
    
    // Update existing goals or create new ones based on situation
    if (hasThreat || hasAnomaly) {
        // Look for existing investigate or respond goals
        bool hasInvestigateGoal = false;
        bool hasRespondGoal = false;
        
        for (const auto& goal : activeGoals) {
            if (goal.type == Goal::Type::VERIFY) {
                hasInvestigateGoal = true;
            } else if (goal.type == Goal::Type::RESPOND) {
                hasRespondGoal = true;
            }
        }
        
        // Create investigate goal if needed
        if (!hasInvestigateGoal) {
            addGoal(
                Goal::Type::VERIFY,
                "Investigate potential security concern",
                Goal::Priority::HIGH
            );
        }
        
        // Create respond goal if high confidence threat
        if (!hasRespondGoal && maxThreatScore > 0.7f) {
            addGoal(
                Goal::Type::RESPOND,
                "Respond to identified security threat",
                Goal::Priority::CRITICAL
            );
        }
    }
    
    // Update goal progress for ongoing goals
    {
        std::lock_guard<std::mutex> lock(m_goalMutex);
        
        for (auto& pair : m_goals) {
            auto& goal = pair.second;
            
            // Skip completed goals
            if (goal.isCompleted()) {
                continue;
            }
            
            // Check progress for different goal types
            switch (goal.type) {
                case Goal::Type::MONITOR:
                    // Monitoring is ongoing, so progress stays at current level
                    break;
                    
                case Goal::Type::DETECT:
                    // For detection goals, check if we've detected what we're looking for
                    // This is simplified and would need to check the specific detection criteria
                    if (hasAnomaly) {
                        goal.progress = 1.0f;
                        goal.status = Goal::Status::ACHIEVED;
                        goal.resultDescription = "Detection successful";
                    }
                    break;
                    
                case Goal::Type::VERIFY:
                    // For verification goals, check ongoing actions
                    {
                        // Count related actions
                        int totalActions = 0;
                        int completedActions = 0;
                        
                        std::lock_guard<std::mutex> actionLock(m_actionMutex);
                        for (const auto& actionPair : m_actions) {
                            if (actionPair.second.goalId == goal.id) {
                                totalActions++;
                                if (actionPair.second.isComplete()) {
                                    completedActions++;
                                }
                            }
                        }
                        
                        if (totalActions > 0) {
                            goal.progress = static_cast<float>(completedActions) / totalActions;
                            
                            if (completedActions == totalActions) {
                                goal.status = Goal::Status::ACHIEVED;
                                goal.resultDescription = "Verification complete";
                            }
                        }
                    }
                    break;
                    
                case Goal::Type::RESPOND:
                    // Similar to verification, check actions
                    {
                        // Count related actions
                        int totalActions = 0;
                        int completedActions = 0;
                        
                        std::lock_guard<std::mutex> actionLock(m_actionMutex);
                        for (const auto& actionPair : m_actions) {
                            if (actionPair.second.goalId == goal.id) {
                                totalActions++;
                                if (actionPair.second.isComplete()) {
                                    completedActions++;
                                }
                            }
                        }
                        
                        if (totalActions > 0) {
                            goal.progress = static_cast<float>(completedActions) / totalActions;
                            
                            if (completedActions == totalActions) {
                                goal.status = Goal::Status::ACHIEVED;
                                goal.resultDescription = "Response complete";
                            }
                        }
                    }
                    break;
                    
                default:
                    break;
            }
            
            // Update last update time
            goal.lastUpdateTimeUs = TimeUtils::getCurrentTimestampUs();
        }
    }
    
    // Create action planning task
    Task task;
    task.type = Task::Type::SELECT_ACTIONS;
    task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    task.priority = 6;
    
    // Add to task queue
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
    }
    
    // Notify worker thread
    m_taskCondition.notify_one();
}

void ReasoningSystem::planActions() {
    // Get active goals
    std::vector<Goal> activeGoals = getActiveGoals();
    
    if (activeGoals.empty()) {
        return;
    }
    
    // Focus on highest priority goal
    const Goal& highestPriorityGoal = activeGoals[0];
    
    // Get recent knowledge
    std::vector<KnowledgeItem> recentKnowledge = queryKnowledge("", 20);
    
    // If we have an LLM, use it for planning
    if (m_llmManager) {
        // Generate reasoning step
        std::string reasoningId = addReasoning(
            Reasoning::Type::PLANNING,
            "Plan actions for goal: " + highestPriorityGoal.description,
            {}
        );
        
        // Generate plans with LLM
        std::vector<Action> plannedActions = planActionsWithLLM(highestPriorityGoal, recentKnowledge);
        
        // Update reasoning step
        {
            std::lock_guard<std::mutex> lock(m_reasoningMutex);
            auto it = m_reasoningSteps.find(reasoningId);
            if (it != m_reasoningSteps.end()) {
                it->second.endTimeUs = TimeUtils::getCurrentTimestampUs();
                it->second.confidence = 0.9f; // High confidence in planning
                it->second.reasoning = "Planned " + std::to_string(plannedActions.size()) + 
                                      " actions for goal: " + highestPriorityGoal.description;
                
                // Add action IDs as outputs
                std::vector<std::string> outputs;
                for (const auto& action : plannedActions) {
                    outputs.push_back(action.id);
                }
                it->second.outputs = outputs;
            }
        }
        
        // Create tasks to execute the actions
        for (const auto& action : plannedActions) {
            Task task;
            task.type = Task::Type::EXECUTE_ACTION;
            task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
            task.priority = static_cast<int>(action.priority * 10);
            task.parameters["actionId"] = action.id;
            
            // Add to task queue
            {
                std::lock_guard<std::mutex> lock(m_taskMutex);
                m_taskQueue.push(task);
            }
        }
        
        // Notify worker thread
        m_taskCondition.notify_one();
    } else {
        // Simple fallback planning without LLM
        switch (highestPriorityGoal.type) {
            case Goal::Type::MONITOR:
                // For monitoring, create a focus action
                createAction(
                    Action::Type::FOCUS_CAMERA,
                    "Focus monitoring on active cameras",
                    highestPriorityGoal.id,
                    0.7f,
                    json{{"duration", 300}} // 5 minutes
                );
                break;
                
            case Goal::Type::VERIFY:
                // For verification, create investigation actions
                createAction(
                    Action::Type::VERIFY_ANOMALY,
                    "Verify reported anomaly",
                    highestPriorityGoal.id,
                    0.9f,
                    json{}
                );
                
                createAction(
                    Action::Type::GATHER_CONTEXT,
                    "Gather additional context",
                    highestPriorityGoal.id,
                    0.8f,
                    json{}
                );
                break;
                
            case Goal::Type::RESPOND:
                // For response, create alert and tracking actions
                createAction(
                    Action::Type::GENERATE_ALERT,
                    "Generate security alert for operators",
                    highestPriorityGoal.id,
                    0.95f,
                    json{{"priority", "high"}}
                );
                
                createAction(
                    Action::Type::TRACK_SUBJECT,
                    "Track suspicious subjects",
                    highestPriorityGoal.id,
                    0.9f,
                    json{}
                );
                break;
                
            default:
                // For other goals, create a generic action
                createAction(
                    Action::Type::LOG_INFORMATION,
                    "Log goal progress: " + highestPriorityGoal.description,
                    highestPriorityGoal.id,
                    0.5f,
                    json{}
                );
                break;
        }
    }
}

void ReasoningSystem::selectBestAction() {
    // Get ongoing actions
    std::vector<Action> ongoingActions = getOngoingActions();
    
    if (ongoingActions.empty()) {
        return;
    }
    
    // For now, we simply execute actions in priority order
    // In a more sophisticated system, we would consider other factors
    
    // The actions are already sorted by priority in getOngoingActions()
    const Action& highestPriorityAction = ongoingActions[0];
    
    // Create a task to execute the action
    Task task;
    task.type = Task::Type::EXECUTE_ACTION;
    task.creationTimeUs = TimeUtils::getCurrentTimestampUs();
    task.priority = static_cast<int>(highestPriorityAction.priority * 10);
    task.parameters["actionId"] = highestPriorityAction.id;
    
    // Add to task queue
    {
        std::lock_guard<std::mutex> lock(m_taskMutex);
        m_taskQueue.push(task);
    }
    
    // Notify worker thread
    m_taskCondition.notify_one();
}

bool ReasoningSystem::executeAction(const Action& action) {
    Logger::info("ReasoningSystem", "Executing action: " + action.description);
    
    // Execute action based on type
    switch (action.type) {
        case Action::Type::FOCUS_CAMERA:
            {
                // Focus on a specific camera
                // In a real system, this would interact with the VMS
                // For now, we just simulate the action
                
                // Get recommended camera from strategy manager
                std::string cameraId;
                if (m_strategyManager) {
                    cameraId = m_strategyManager->getRecommendedCamera();
                }
                
                // Log the action
                std::string result = "Focused monitoring on camera: " + 
                                    (cameraId.empty() ? "all cameras" : cameraId);
                
                Logger::info("ActionExecution", result);
                
                // Update action result
                std::lock_guard<std::mutex> lock(m_actionMutex);
                auto it = m_actions.find(action.id);
                if (it != m_actions.end()) {
                    it->second.result = result;
                }
                
                return true;
            }
            
        case Action::Type::ADJUST_ANALYSIS:
            {
                // Adjust analysis parameters
                // In a real system, this would update configuration
                
                // Add a knowledge item about the adjustment
                addKnowledgeItem(
                    KnowledgeItem::Type::META_KNOWLEDGE,
                    "Adjusted analysis parameters for optimized detection",
                    0.9f,
                    "ActionExecution"
                );
                
                return true;
            }
            
        case Action::Type::GENERATE_ALERT:
            {
                // Generate an alert for operators
                // In a real system, this would trigger an actual alert
                
                // Get alert priority from parameters
                std::string priority = "medium";
                if (action.parameters.contains("priority")) {
                    priority = action.parameters["priority"];
                }
                
                // Create alert message
                std::string alertMessage = "SECURITY ALERT (" + priority + "): ";
                
                // Look for threat information in recent knowledge
                std::vector<KnowledgeItem> threatKnowledge = queryKnowledge("threat", 3);
                if (!threatKnowledge.empty()) {
                    alertMessage += threatKnowledge[0].content;
                } else {
                    // Fall back to anomaly information
                    std::vector<KnowledgeItem> anomalyKnowledge = queryKnowledge("anomaly", 3);
                    if (!anomalyKnowledge.empty()) {
                        alertMessage += anomalyKnowledge[0].content;
                    } else {
                        alertMessage += "Potential security concern detected. Please verify.";
                    }
                }
                
                Logger::info("ActionExecution", "Generated alert: " + alertMessage);
                
                // Add knowledge item about the alert
                addKnowledgeItem(
                    KnowledgeItem::Type::OBSERVATION,
                    "Security alert generated: " + alertMessage,
                    0.95f,
                    "ActionExecution"
                );
                
                // Update action result
                std::lock_guard<std::mutex> lock(m_actionMutex);
                auto it = m_actions.find(action.id);
                if (it != m_actions.end()) {
                    it->second.result = "Alert generated: " + alertMessage;
                }
                
                return true;
            }
            
        case Action::Type::SUPPRESS_ALERT:
            {
                // Suppress a potential alert
                // This would typically be done for false alarms
                
                Logger::info("ActionExecution", "Suppressed alert to prevent false alarm");
                
                // Add knowledge item
                addKnowledgeItem(
                    KnowledgeItem::Type::META_KNOWLEDGE,
                    "Suppressed potential false alarm",
                    0.8f,
                    "ActionExecution"
                );
                
                return true;
            }
            
        case Action::Type::GATHER_CONTEXT:
            {
                // Gather more contextual information
                // In a real system, this might involve checking other cameras
                // or accessing additional data sources
                
                // If we have a strategy manager, use it to get additional context
                if (m_strategyManager) {
                    std::string report = m_strategyManager->generateSituationReport();
                    
                    // Add the report as knowledge
                    addKnowledgeItem(
                        KnowledgeItem::Type::CONTEXTUAL_INFO,
                        "Situation context: " + report,
                        0.85f,
                        "ContextGathering"
                    );
                    
                    // Update action result
                    std::lock_guard<std::mutex> lock(m_actionMutex);
                    auto it = m_actions.find(action.id);
                    if (it != m_actions.end()) {
                        it->second.result = "Gathered additional context";
                    }
                    
                    return true;
                }
                
                // Fallback if no strategy manager
                addKnowledgeItem(
                    KnowledgeItem::Type::CONTEXTUAL_INFO,
                    "Unable to gather additional context",
                    0.5f,
                    "ContextGathering"
                );
                
                return false;
            }
            
        case Action::Type::VERIFY_ANOMALY:
            {
                // Verify if an anomaly is genuine
                // In a real system, this might involve additional analysis
                
                // Look for recent anomaly knowledge
                std::vector<KnowledgeItem> anomalyKnowledge = queryKnowledge("anomaly", 5);
                
                if (anomalyKnowledge.empty()) {
                    Logger::warning("ActionExecution", "No anomalies found to verify");
                    return false;
                }
                
                // For simulation, we'll treat high confidence anomalies as verified
                bool verified = false;
                for (const auto& knowledge : anomalyKnowledge) {
                    if (knowledge.confidence > 0.8f) {
                        verified = true;
                        break;
                    }
                }
                
                // Add verification result as knowledge
                if (verified) {
                    addKnowledgeItem(
                        KnowledgeItem::Type::INFERENCE,
                        "Anomaly verification: The detected anomaly has been confirmed as genuine",
                        0.9f,
                        "AnomalyVerification"
                    );
                } else {
                    addKnowledgeItem(
                        KnowledgeItem::Type::INFERENCE,
                        "Anomaly verification: Unable to confirm the anomaly with high confidence",
                        0.7f,
                        "AnomalyVerification"
                    );
                }
                
                // Update action result
                std::lock_guard<std::mutex> lock(m_actionMutex);
                auto it = m_actions.find(action.id);
                if (it != m_actions.end()) {
                    it->second.result = verified ? 
                        "Anomaly verified as genuine" : 
                        "Unable to verify anomaly with high confidence";
                }
                
                return true;
            }
            
        case Action::Type::CORRELATE_EVENTS:
            {
                // Look for correlations between events
                // In a real system, this would involve pattern matching
                
                // For now, just add a knowledge item
                addKnowledgeItem(
                    KnowledgeItem::Type::INFERENCE,
                    "Event correlation analysis completed",
                    0.7f,
                    "EventCorrelation"
                );
                
                return true;
            }
            
        case Action::Type::INITIATE_RESPONSE:
            {
                // Initiate a response protocol
                // In a real system, this would trigger predefined protocols
                
                // If we have a strategy manager, create an incident
                if (m_strategyManager) {
                    // Look for threat information
                    std::vector<KnowledgeItem> threatKnowledge = queryKnowledge("threat", 3);
                    
                    SecurityIncident::Type incidentType = SecurityIncident::Type::SUSPICIOUS_BEHAVIOR;
                    SecurityIncident::Severity severity = SecurityIncident::Severity::MEDIUM;
                    std::string description = "Automated response to security concern";
                    std::string cameraId = "";
                    
                    if (!threatKnowledge.empty()) {
                        description = threatKnowledge[0].content;
                        severity = threatKnowledge[0].confidence > 0.8f ? 
                            SecurityIncident::Severity::HIGH : SecurityIncident::Severity::MEDIUM;
                    }
                    
                    // Create incident
                    std::string incidentId = m_strategyManager->createIncident(
                        incidentType, severity, cameraId, description
                    );
                    
                    if (!incidentId.empty()) {
                        Logger::info("ActionExecution", "Created incident: " + incidentId);
                        
                        // Update action result
                        std::lock_guard<std::mutex> lock(m_actionMutex);
                        auto it = m_actions.find(action.id);
                        if (it != m_actions.end()) {
                            it->second.result = "Initiated response protocol - Incident ID: " + incidentId;
                        }
                        
                        return true;
                    }
                }
                
                // Fallback if strategy manager not available or incident creation failed
                Logger::warning("ActionExecution", "Failed to initiate response protocol");
                return false;
            }
            
        case Action::Type::TRACK_SUBJECT:
            {
                // Begin tracking a subject
                // In a real system, this would set up tracking
                
                if (m_strategyManager) {
                    // Get tracked subjects
                    auto subjects = m_strategyManager->getTrackedSubjects();
                    
                    if (!subjects.empty()) {
                        // Focus on highest threat subject
                        auto& subject = subjects[0];
                        
                        Logger::info("ActionExecution", "Tracking subject: " + subject.trackId);
                        
                        // Add knowledge about the tracking
                        addKnowledgeItem(
                            KnowledgeItem::Type::OBSERVATION,
                            "Actively tracking subject with ID " + subject.trackId,
                            0.9f,
                            "SubjectTracking"
                        );
                        
                        // Update action result
                        std::lock_guard<std::mutex> lock(m_actionMutex);
                        auto it = m_actions.find(action.id);
                        if (it != m_actions.end()) {
                            it->second.result = "Tracking subject: " + subject.trackId;
                        }
                        
                        return true;
                    }
                }
                
                // No subjects to track
                Logger::warning("ActionExecution", "No subjects available for tracking");
                return false;
            }
            
        case Action::Type::COORDINATE_SYSTEM:
            {
                // Coordinate with other systems
                // This is a placeholder for integration with external systems
                
                Logger::info("ActionExecution", "Coordinating with external systems");
                
                // Add knowledge item
                addKnowledgeItem(
                    KnowledgeItem::Type::OBSERVATION,
                    "Coordinated response with external systems",
                    0.8f,
                    "SystemCoordination"
                );
                
                return true;
            }
            
        case Action::Type::UPDATE_MODEL:
            {
                // Update internal models
                // This would typically involve learning from recent events
                
                Logger::info("ActionExecution", "Updating internal models based on recent events");
                
                // Add knowledge item
                addKnowledgeItem(
                    KnowledgeItem::Type::META_KNOWLEDGE,
                    "Updated internal models for improved detection",
                    0.85f,
                    "ModelUpdate"
                );
                
                return true;
            }
            
        case Action::Type::LOG_INFORMATION:
            {
                // Log information for future reference
                
                std::string logMessage = "System log: ";
                if (action.parameters.contains("message")) {
                    logMessage += action.parameters["message"];
                } else {
                    logMessage += action.description;
                }
                
                Logger::info("ActionExecution", logMessage);
                
                // Update action result
                std::lock_guard<std::mutex> lock(m_actionMutex);
                auto it = m_actions.find(action.id);
                if (it != m_actions.end()) {
                    it->second.result = "Information logged: " + logMessage;
                }
                
                return true;
            }
            
        case Action::Type::REQUEST_ASSISTANCE:
            {
                // Request human assistance
                
                std::string message = "ASSISTANCE REQUIRED: ";
                if (action.parameters.contains("message")) {
                    message += action.parameters["message"];
                } else {
                    message += "Human operator assistance required for security situation";
                }
                
                Logger::info("ActionExecution", "Requesting assistance: " + message);
                
                // Add knowledge item
                addKnowledgeItem(
                    KnowledgeItem::Type::META_KNOWLEDGE,
                    "Requested human operator assistance: " + message,
                    0.9f,
                    "AssistanceRequest"
                );
                
                // Update action result
                std::lock_guard<std::mutex> lock(m_actionMutex);
                auto it = m_actions.find(action.id);
                if (it != m_actions.end()) {
                    it->second.result = "Assistance requested: " + message;
                }
                
                return true;
            }
            
        default:
            Logger::warning("ActionExecution", "Unknown action type: " + std::to_string(static_cast<int>(action.type)));
            return false;
    }
}

void ReasoningSystem::evaluatePerformance() {
    // Get system state for evaluation
    json systemState;
    
    // Add goals
    {
        std::lock_guard<std::mutex> lock(m_goalMutex);
        json goalsJson = json::array();
        
        for (const auto& pair : m_goals) {
            goalsJson.push_back(pair.second.toJson());
        }
        
        systemState["goals"] = goalsJson;
    }
    
    // Add actions
    {
        std::lock_guard<std::mutex> lock(m_actionMutex);
        json actionsJson = json::array();
        
        for (const auto& pair : m_actions) {
            actionsJson.push_back(pair.second.toJson());
        }
        
        systemState["actions"] = actionsJson;
    }
    
    // Add timestamp
    systemState["timestampUs"] = TimeUtils::getCurrentTimestampUs();
    
    // Add to recent states
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_recentStates.push_back(systemState);
        
        // Limit size
        while (m_recentStates.size() > 100) {
            m_recentStates.pop_front();
        }
    }
    
    // If we have an LLM, use it for reflection
    if (m_llmManager && m_recentStates.size() >= 5) {
        // Get recent states for reflection
        std::vector<json> recentStates;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            recentStates.assign(m_recentStates.end() - std::min(static_cast<size_t>(5), m_recentStates.size()),
                               m_recentStates.end());
        }
        
        // Use LLM for reflection
        json reflection = reflectWithLLM(recentStates);
        
        // Add reflection as knowledge
        if (reflection.contains("insights") && reflection["insights"].is_array()) {
            for (const auto& insight : reflection["insights"]) {
                if (insight.is_string()) {
                    addKnowledgeItem(
                        KnowledgeItem::Type::META_KNOWLEDGE,
                        insight,
                        0.8f,
                        "SystemReflection"
                    );
                }
            }
        }
        
        // Apply recommendations if available
        if (reflection.contains("recommendations") && reflection["recommendations"].is_array()) {
            for (const auto& recommendation : reflection["recommendations"]) {
                if (recommendation.is_string()) {
                    std::string recText = recommendation;
                    
                    Logger::info("SystemReflection", "Applying recommendation: " + recText);
                    
                    // Implement recommendation based on content
                    if (recText.find("goal") != std::string::npos && recText.find("create") != std::string::npos) {
                        // Create a new goal based on recommendation
                        addGoal(
                            Goal::Type::OPTIMIZE,
                            "Optimization goal from reflection: " + recText,
                            Goal::Priority::MEDIUM
                        );
                    } else if (recText.find("model") != std::string::npos && recText.find("update") != std::string::npos) {
                        // Create an action to update models
                        createAction(
                            Action::Type::UPDATE_MODEL,
                            "Update models based on reflection: " + recText,
                            "",
                            0.7f,
                            json{{"recommendation", recText}}
                        );
                    }
                }
            }
        }
    }
}

void ReasoningSystem::updateStrategies() {
    // This would update strategies based on reflection
    // For now, it's a placeholder
}

Reasoning ReasoningSystem::generateReasoningWithLLM(Reasoning::Type type, 
                                                  const std::string& description,
                                                  const std::vector<KnowledgeItem>& relevantKnowledge) {
    Reasoning reasoning;
    reasoning.id = generateUniqueId("REAS");
    reasoning.type = type;
    reasoning.description = description;
    reasoning.startTimeUs = TimeUtils::getCurrentTimestampUs();
    
    // Add knowledge item IDs as inputs
    for (const auto& item : relevantKnowledge) {
        reasoning.inputs.push_back(item.id);
    }
    
    // Create an LLM request
    LLMRequest request("SYSTEM", LLMRequest::Type::SITUATION_ASSESSMENT);
    
    // Add relevant knowledge as context
    for (const auto& item : relevantKnowledge) {
        ContextItem contextItem;
        contextItem.type = ContextItem::Type::ENVIRONMENT_INFO;
        contextItem.description = item.content;
        contextItem.timestampUs = item.timestampUs;
        contextItem.confidence = item.confidence;
        contextItem.metadata = item.toJson();
        
        request.addContextItem(contextItem);
    }
    
    // Add query as a context item
    ContextItem queryItem;
    queryItem.type = ContextItem::Type::ENVIRONMENT_INFO;
    queryItem.description = "Query: " + description;
    queryItem.timestampUs = reasoning.startTimeUs;
    queryItem.confidence = 1.0f;
    
    request.addContextItem(queryItem);
    
    // Send request to LLM
    auto future = m_llmManager->submitRequest(request);
    auto response = future.get();
    
    if (response.success) {
        reasoning.reasoning = response.reasoning;
        reasoning.confidence = response.confidenceScore;
        
        // Generate new knowledge items from the reasoning
        std::vector<std::string> outputIds;
        
        // Extract key insights
        std::vector<std::string> insights;
        std::string text = response.reasoning;
        
        // Simple extraction of sentences
        std::regex sentenceRegex("([^.!?]+[.!?])");
        auto sentencesBegin = std::sregex_iterator(text.begin(), text.end(), sentenceRegex);
        auto sentencesEnd = std::sregex_iterator();
        
        for (std::sregex_iterator i = sentencesBegin; i != sentencesEnd; ++i) {
            std::string sentence = (*i).str();
            sentence = StringUtils::trim(sentence);
            
            if (sentence.length() > 10) {
                insights.push_back(sentence);
            }
        }
        
        // Limit to top insights
        const int MAX_INSIGHTS = 3;
        if (insights.size() > MAX_INSIGHTS) {
            insights.resize(MAX_INSIGHTS);
        }
        
        // Create knowledge items for insights
        for (const auto& insight : insights) {
            KnowledgeItem newKnowledge;
            newKnowledge.id = generateUniqueId("KNOW");
            newKnowledge.type = KnowledgeItem::Type::INFERENCE;
            newKnowledge.content = insight;
            newKnowledge.confidence = reasoning.confidence * 0.9f; // Slightly lower confidence
            newKnowledge.timestampUs = TimeUtils::getCurrentTimestampUs();
            newKnowledge.source = "LLMReasoning";
            
            // Add related items
            newKnowledge.relatedItems = reasoning.inputs;
            
            // Add to knowledge base
            {
                std::lock_guard<std::mutex> lock(m_knowledgeMutex);
                m_knowledgeItems[newKnowledge.id] = newKnowledge;
            }
            
            outputIds.push_back(newKnowledge.id);
        }
        
        reasoning.outputs = outputIds;
    } else {
        // If LLM fails, create a simple reasoning
        reasoning.reasoning = "Failed to generate reasoning with LLM";
        reasoning.confidence = 0.2f;
    }
    
    reasoning.endTimeUs = TimeUtils::getCurrentTimestampUs();
    
    // Add to reasoning map
    {
        std::lock_guard<std::mutex> lock(m_reasoningMutex);
        m_reasoningSteps[reasoning.id] = reasoning;
    }
    
    return reasoning;
}

json ReasoningSystem::assessSituationWithLLM(const std::vector<KnowledgeItem>& relevantKnowledge) {
    // Create an LLM request
    LLMRequest request("SYSTEM", LLMRequest::Type::SITUATION_ASSESSMENT);
    
    // Add relevant knowledge as context
    for (const auto& item : relevantKnowledge) {
        ContextItem contextItem;
        contextItem.type = ContextItem::Type::ENVIRONMENT_INFO;
        contextItem.description = item.content;
        contextItem.timestampUs = item.timestampUs;
        contextItem.confidence = item.confidence;
        contextItem.metadata = item.toJson();
        
        request.addContextItem(contextItem);
    }
    
    // Send request to LLM
    auto future = m_llmManager->submitRequest(request);
    auto response = future.get();
    
    if (response.success) {
        // Create assessment JSON
        json assessment;
        assessment["situationAssessment"] = response.reasoning;
        assessment["confidence"] = response.confidenceScore;
        
        // Extract threat level
        assessment["threatLevel"] = "UNKNOWN";
        
        std::string lowerReasoning = StringUtils::toLower(response.reasoning);
        if (lowerReasoning.find("critical") != std::string::npos || 
            lowerReasoning.find("severe") != std::string::npos ||
            lowerReasoning.find("high threat") != std::string::npos) {
            assessment["threatLevel"] = "CRITICAL";
        } else if (lowerReasoning.find("high") != std::string::npos) {
            assessment["threatLevel"] = "HIGH";
        } else if (lowerReasoning.find("medium") != std::string::npos || 
                 lowerReasoning.find("moderate") != std::string::npos) {
            assessment["threatLevel"] = "MEDIUM";
        } else if (lowerReasoning.find("low") != std::string::npos || 
                 lowerReasoning.find("minor") != std::string::npos) {
            assessment["threatLevel"] = "LOW";
        } else if (lowerReasoning.find("normal") != std::string::npos || 
                 lowerReasoning.find("no threat") != std::string::npos ||
                 lowerReasoning.find("no concern") != std::string::npos) {
            assessment["threatLevel"] = "NORMAL";
        }
        
        // Add actions from response
        assessment["recommendedActions"] = json::array();
        for (const auto& action : response.actions) {
            assessment["recommendedActions"].push_back(action.description);
        }
        
        return assessment;
    }
    
    // Return empty assessment if LLM fails
    json emptyAssessment;
    emptyAssessment["situationAssessment"] = "Failed to assess situation";
    emptyAssessment["confidence"] = 0.1;
    emptyAssessment["threatLevel"] = "UNKNOWN";
    emptyAssessment["recommendedActions"] = json::array();
    
    return emptyAssessment;
}

std::vector<Action> ReasoningSystem::planActionsWithLLM(const Goal& goal, 
                                                      const std::vector<KnowledgeItem>& relevantKnowledge) {
    std::vector<Action> actions;
    
    // Create an LLM request
    LLMRequest request("SYSTEM", LLMRequest::Type::RESPONSE_PLANNING);
    
    // Add goal as context
    ContextItem goalItem;
    goalItem.type = ContextItem::Type::ENVIRONMENT_INFO;
    goalItem.description = "Goal: " + goal.description;
    goalItem.timestampUs = goal.creationTimeUs;
    goalItem.confidence = 1.0f;
    goalItem.metadata = goal.toJson();
    
    request.addContextItem(goalItem);
    
    // Add relevant knowledge as context
    for (const auto& item : relevantKnowledge) {
        ContextItem contextItem;
        contextItem.type = ContextItem::Type::ENVIRONMENT_INFO;
        contextItem.description = item.content;
        contextItem.timestampUs = item.timestampUs;
        contextItem.confidence = item.confidence;
        contextItem.metadata = item.toJson();
        
        request.addContextItem(contextItem);
    }
    
    // Send request to LLM
    auto future = m_llmManager->submitRequest(request);
    auto response = future.get();
    
    if (response.success) {
        // Convert LLM actions to system actions
        for (const auto& llmAction : response.actions) {
            Action action;
            action.id = generateUniqueId("ACT");
            action.description = llmAction.description;
            action.status = Action::Status::PENDING;
            action.goalId = goal.id;
            action.creationTimeUs = TimeUtils::getCurrentTimestampUs();
            action.startTimeUs = 0;
            action.completionTimeUs = 0;
            action.priority = llmAction.confidence;
            action.expectedUtility = llmAction.confidence;
            action.parameters = llmAction.parameters;
            
            // Map LLM action type to system action type
            switch (llmAction.type) {
                case LLMResponse::Action::Type::MONITOR:
                    action.type = Action::Type::FOCUS_CAMERA;
                    break;
                case LLMResponse::Action::Type::ALERT:
                    action.type = Action::Type::GENERATE_ALERT;
                    break;
                case LLMResponse::Action::Type::TRACK:
                    action.type = Action::Type::TRACK_SUBJECT;
                    break;
                case LLMResponse::Action::Type::ANALYZE_FURTHER:
                    action.type = Action::Type::GATHER_CONTEXT;
                    break;
                case LLMResponse::Action::Type::CROSS_REFERENCE:
                    action.type = Action::Type::CORRELATE_EVENTS;
                    break;
                case LLMResponse::Action::Type::PREDICT:
                    action.type = Action::Type::UPDATE_MODEL;
                    break;
                case LLMResponse::Action::Type::RECOMMEND:
                    action.type = Action::Type::REQUEST_ASSISTANCE;
                    break;
                default:
                    action.type = Action::Type::LOG_INFORMATION;
                    break;
            }
            
            // Add to actions map
            {
                std::lock_guard<std::mutex> lock(m_actionMutex);
                m_actions[action.id] = action;
            }
            
            actions.push_back(action);
        }
    } else {
        // If LLM fails, create default actions based on goal type
        switch (goal.type) {
            case Goal::Type::MONITOR:
                {
                    Action action;
                    action.id = generateUniqueId("ACT");
                    action.type = Action::Type::FOCUS_CAMERA;
                    action.description = "Focus monitoring on active cameras";
                    action.status = Action::Status::PENDING;
                    action.goalId = goal.id;
                    action.creationTimeUs = TimeUtils::getCurrentTimestampUs();
                    action.priority = 0.7f;
                    action.parameters = json{{"duration", 300}}; // 5 minutes
                    
                    // Add to actions map
                    {
                        std::lock_guard<std::mutex> lock(m_actionMutex);
                        m_actions[action.id] = action;
                    }
                    
                    actions.push_back(action);
                }
                break;
                
            case Goal::Type::VERIFY:
                {
                    Action action1;
                    action1.id = generateUniqueId("ACT");
                    action1.type = Action::Type::VERIFY_ANOMALY;
                    action1.description = "Verify reported anomaly";
                    action1.status = Action::Status::PENDING;
                    action1.goalId = goal.id;
                    action1.creationTimeUs = TimeUtils::getCurrentTimestampUs();
                    action1.priority = 0.9f;
                    
                    // Add to actions map
                    {
                        std::lock_guard<std::mutex> lock(m_actionMutex);
                        m_actions[action1.id] = action1;
                    }
                    
                    actions.push_back(action1);
                    
                    Action action2;
                    action2.id = generateUniqueId("ACT");
                    action2.type = Action::Type::GATHER_CONTEXT;
                    action2.description = "Gather additional context";
                    action2.status = Action::Status::PENDING;
                    action2.goalId = goal.id;
                    action2.creationTimeUs = TimeUtils::getCurrentTimestampUs();
                    action2.priority = 0.8f;
                    
                    // Add to actions map
                    {
                        std::lock_guard<std::mutex> lock(m_actionMutex);
                        m_actions[action2.id] = action2;
                    }
                    
                    actions.push_back(action2);
                }
                break;
                
            case Goal::Type::RESPOND:
                {
                    Action action1;
                    action1.id = generateUniqueId("ACT");
                    action1.type = Action::Type::GENERATE_ALERT;
                    action1.description = "Generate security alert for operators";
                    action1.status = Action::Status::PENDING;
                    action1.goalId = goal.id;
                    action1.creationTimeUs = TimeUtils::getCurrentTimestampUs();
                    action1.priority = 0.95f;
                    action1.parameters = json{{"priority", "high"}};
                    
                    // Add to actions map
                    {
                        std::lock_guard<std::mutex> lock(m_actionMutex);
                        m_actions[action1.id] = action1;
                    }
                    
                    actions.push_back(action1);
                    
                    Action action2;
                    action2.id = generateUniqueId("ACT");
                    action2.type = Action::Type::TRACK_SUBJECT;
                    action2.description = "Track suspicious subjects";
                    action2.status = Action::Status::PENDING;
                    action2.goalId = goal.id;
                    action2.creationTimeUs = TimeUtils::getCurrentTimestampUs();
                    action2.priority = 0.9f;
                    
                    // Add to actions map
                    {
                        std::lock_guard<std::mutex> lock(m_actionMutex);
                        m_actions[action2.id] = action2;
                    }
                    
                    actions.push_back(action2);
                }
                break;
                
            default:
                {
                    Action action;
                    action.id = generateUniqueId("ACT");
                    action.type = Action::Type::LOG_INFORMATION;
                    action.description = "Log goal progress: " + goal.description;
                    action.status = Action::Status::PENDING;
                    action.goalId = goal.id;
                    action.creationTimeUs = TimeUtils::getCurrentTimestampUs();
                    action.priority = 0.5f;
                    
                    // Add to actions map
                    {
                        std::lock_guard<std::mutex> lock(m_actionMutex);
                        m_actions[action.id] = action;
                    }
                    
                    actions.push_back(action);
                }
                break;
        }
    }
    
    return actions;
}

json ReasoningSystem::reflectWithLLM(const std::vector<json>& recentStates) {
    // Create an LLM request
    LLMRequest request("SYSTEM", LLMRequest::Type::SITUATION_ASSESSMENT);
    
    // Add recent states as context
    for (size_t i = 0; i < recentStates.size(); ++i) {
        const auto& state = recentStates[i];
        
        ContextItem stateItem;
        stateItem.type = ContextItem::Type::ENVIRONMENT_INFO;
        stateItem.description = "System state " + std::to_string(i + 1) + " of " + 
                               std::to_string(recentStates.size());
        stateItem.timestampUs = state.contains("timestampUs") ? 
                               state["timestampUs"].get<int64_t>() : 
                               TimeUtils::getCurrentTimestampUs();
        stateItem.confidence = 1.0f;
        stateItem.metadata = state;
        
        request.addContextItem(stateItem);
    }
    
    // Add reflection query
    ContextItem queryItem;
    queryItem.type = ContextItem::Type::ENVIRONMENT_INFO;
    queryItem.description = "Please analyze system performance and provide insights and recommendations for improvement.";
    queryItem.timestampUs = TimeUtils::getCurrentTimestampUs();
    queryItem.confidence = 1.0f;
    
    request.addContextItem(queryItem);

// Send request to LLM
    auto future = m_llmManager->submitRequest(request);
    auto response = future.get();
    
    if (response.success) {
        // Create reflection JSON
        json reflection;
        reflection["reflection"] = response.reasoning;
        reflection["confidence"] = response.confidenceScore;
        
        // Extract insights
        reflection["insights"] = json::array();
        
        // Simple extraction of sentences
        std::string text = response.reasoning;
        std::regex sentenceRegex("([^.!?]+[.!?])");
        auto sentencesBegin = std::sregex_iterator(text.begin(), text.end(), sentenceRegex);
        auto sentencesEnd = std::sregex_iterator();
        
        std::vector<std::string> sentences;
        for (std::sregex_iterator i = sentencesBegin; i != sentencesEnd; ++i) {
            std::string sentence = (*i).str();
            sentence = StringUtils::trim(sentence);
            
            if (sentence.length() > 10) {
                sentences.push_back(sentence);
            }
        }
        
        // Find sentences that appear to be insights
        for (const auto& sentence : sentences) {
            std::string lowerSentence = StringUtils::toLower(sentence);
            
            bool isInsight = false;
            std::vector<std::string> insightIndicators = {
                "suggest", "recommend", "could", "should", "might", "consider",
                "opportunity", "improve", "insight", "pattern", "notice", "observed",
                "perform", "efficiency", "effective", "optimize"
            };
            
            for (const auto& indicator : insightIndicators) {
                if (lowerSentence.find(indicator) != std::string::npos) {
                    isInsight = true;
                    break;
                }
            }
            
            if (isInsight) {
                reflection["insights"].push_back(sentence);
            }
        }
        
        // Limit insights to a reasonable number
        if (reflection["insights"].size() > 5) {
            json limitedInsights = json::array();
            for (size_t i = 0; i < 5; ++i) {
                limitedInsights.push_back(reflection["insights"][i]);
            }
            reflection["insights"] = limitedInsights;
        }
        
        // Add recommendations from actions
        reflection["recommendations"] = json::array();
        for (const auto& action : response.actions) {
            reflection["recommendations"].push_back(action.description);
        }
        
        return reflection;
    }
    
    // Return empty reflection if LLM fails
    json emptyReflection;
    emptyReflection["reflection"] = "Failed to generate reflection";
    emptyReflection["confidence"] = 0.1;
    emptyReflection["insights"] = json::array();
    emptyReflection["recommendations"] = json::array();
    
    return emptyReflection;
}

} // namespace nx_agent
