// nx_agent_reasoning.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <deque>
#include <chrono>
#include <functional>
#include <future>
#include <nlohmann/json.hpp>

namespace nx_agent {

// Forward declarations
class DeviceConfig;
class LLMManager;
class ContextManager;
class StrategyManager;
struct FrameAnalysisResult;
struct LLMResponse;
struct LLMRequest;
struct SecurityIncident;
struct TrackedSubject;

/**
 * Represents a fact or belief about the environment
 */
struct KnowledgeItem {
    enum class Type {
        OBSERVATION,     // Direct observation
        INFERENCE,       // Inferred from observations
        PREDICTION,      // Predicted future state
        HISTORICAL_FACT, // Known historical information
        CONTEXTUAL_INFO, // Background information
        META_KNOWLEDGE   // Knowledge about knowledge (confidence, etc.)
    };
    
    Type type;
    std::string content;
    float confidence;
    int64_t timestampUs;
    std::string source;
    std::vector<std::string> relatedItems; // IDs of related knowledge items
    
    // Create a unique ID for this item
    std::string id;
    
    // Check if the item is still valid
    bool isValid(int64_t currentTimeUs, int64_t timeoutUs = 60000000) const {
        return (currentTimeUs - timestampUs) < timeoutUs;
    }
    
    // Convert to JSON
    nlohmann::json toJson() const;
    
    // Create from JSON
    static KnowledgeItem fromJson(const nlohmann::json& json);
};

/**
 * Represents a goal or objective for the system
 */
struct Goal {
    enum class Type {
        MONITOR,          // Passive monitoring
        DETECT,           // Detect specific conditions
        TRACK,            // Track an entity
        VERIFY,           // Verify a hypothesis
        RESPOND,          // Respond to a situation
        PREVENT,          // Prevent an outcome
        OPTIMIZE,         // Optimize system performance
    };
    
    enum class Status {
        PENDING,          // Not yet started
        IN_PROGRESS,      // Currently being pursued
        ACHIEVED,         // Successfully completed
        FAILED,           // Failed to achieve
        ABANDONED         // Deliberately stopped pursuing
    };
    
    enum class Priority {
        CRITICAL,         // Highest priority
        HIGH,             // High priority
        MEDIUM,           // Medium priority
        LOW,              // Low priority
        BACKGROUND        // Lowest priority, background task
    };
    
    std::string id;
    Type type;
    std::string description;
    Status status;
    Priority priority;
    int64_t creationTimeUs;
    int64_t deadlineUs;  // When the goal needs to be achieved by
    std::string parentGoalId; // ID of parent goal if part of hierarchy
    std::vector<std::string> subGoalIds; // IDs of sub-goals
    std::vector<std::string> dependsOnGoalIds; // Goals that must be completed first
    nlohmann::json parameters; // Additional parameters for the goal
    
    // Progress from 0.0 to 1.0
    float progress;
    
    // Last update time
    int64_t lastUpdateTimeUs;
    
    // Result if completed
    std::string resultDescription;
    
    // Convert to JSON
    nlohmann::json toJson() const;
    
    // Create from JSON
    static Goal fromJson(const nlohmann::json& json);
    
    // Check if the goal is active
    bool isActive() const {
        return status == Status::PENDING || status == Status::IN_PROGRESS;
    }
    
    // Check if the goal is completed (succeeded or failed)
    bool isCompleted() const {
        return status == Status::ACHIEVED || status == Status::FAILED || status == Status::ABANDONED;
    }
    
    // Check if the goal is achievable within deadline
    bool isAchievableByDeadline(int64_t currentTimeUs) const {
        // If no deadline specified, assume achievable
        if (deadlineUs == 0) {
            return true;
        }
        
        return currentTimeUs < deadlineUs;
    }
};

/**
 * Represents the agent's thought process
 */
struct Reasoning {
    enum class Type {
        PERCEPTION,       // Processing sensory input
        SITUATION_ASSESSMENT, // Understanding the current situation
        PLANNING,         // Planning actions
        DECISION_MAKING,  // Making decisions
        SELF_REFLECTION,  // Assessing own performance
        META_COGNITIVE    // Thinking about thinking
    };
    
    std::string id;
    Type type;
    std::string description;
    std::vector<std::string> inputs; // IDs of knowledge items used as input
    std::vector<std::string> outputs; // IDs of knowledge items generated
    int64_t startTimeUs;
    int64_t endTimeUs;
    float confidence;
    std::vector<std::string> alternativeConsidered; // Alternative conclusions considered
    std::string reasoning; // Step-by-step reasoning process
    
    // Convert to JSON
    nlohmann::json toJson() const;
    
    // Create from JSON
    static Reasoning fromJson(const nlohmann::json& json);
};

/**
 * Represents an action the agent can take
 */
struct Action {
    enum class Type {
        // Camera control actions
        FOCUS_CAMERA,     // Focus attention on a specific camera
        ADJUST_ANALYSIS,  // Adjust analysis parameters
        
        // Alerting actions
        GENERATE_ALERT,   // Create an alert for operators
        SUPPRESS_ALERT,   // Suppress a potential alert
        
        // Investigation actions
        GATHER_CONTEXT,   // Gather more contextual information
        VERIFY_ANOMALY,   // Verify if an anomaly is genuine
        CORRELATE_EVENTS, // Look for correlations between events
        
        // Response actions
        INITIATE_RESPONSE, // Initiate a response protocol
        TRACK_SUBJECT,    // Begin tracking a subject
        COORDINATE_SYSTEM, // Coordinate with other systems
        
        // Internal actions
        UPDATE_MODEL,     // Update internal models
        LOG_INFORMATION,  // Log information for future reference
        REQUEST_ASSISTANCE // Request human assistance
    };
    
    enum class Status {
        PENDING,         // Not yet started
        IN_PROGRESS,     // Currently being executed
        COMPLETED,       // Successfully completed
        FAILED,          // Failed to complete
        CANCELLED        // Deliberately cancelled
    };
    
    std::string id;
    Type type;
    std::string description;
    Status status;
    std::string goalId;  // ID of the goal this action supports
    int64_t creationTimeUs;
    int64_t startTimeUs;
    int64_t completionTimeUs;
    float priority;      // 0.0-1.0, higher is more important
    float expectedUtility; // Expected utility of this action
    nlohmann::json parameters; // Parameters for the action
    std::string result;  // Result of the action
    
    // Convert to JSON
    nlohmann::json toJson() const;
    
    // Create from JSON
    static Action fromJson(const nlohmann::json& json);
    
    // Check if action is complete
    bool isComplete() const {
        return status == Status::COMPLETED || status == Status::FAILED || status == Status::CANCELLED;
    }
};

/**
 * Central reasoning system that coordinates perception, cognition, and action
 */
class ReasoningSystem {
public:
    ReasoningSystem(const std::string& systemId);
    ~ReasoningSystem();
    
    // Initialize the system with required managers
    bool initialize(std::shared_ptr<LLMManager> llmManager,
                   std::shared_ptr<ContextManager> contextManager, 
                   std::shared_ptr<StrategyManager> strategyManager);
    
    // Process a new frame analysis result
    void processAnalysisResult(const std::string& deviceId, const FrameAnalysisResult& result);
    
    // Generate a cognitive status update
    std::string generateCognitiveStatus();
    
    // Add a goal to the system
    std::string addGoal(Goal::Type type, 
                        const std::string& description,
                        Goal::Priority priority = Goal::Priority::MEDIUM);
    
    // Update a goal's status
    bool updateGoalStatus(const std::string& goalId, Goal::Status status);
    
    // Get all active goals
    std::vector<Goal> getActiveGoals() const;
    
    // Add a knowledge item
    std::string addKnowledgeItem(KnowledgeItem::Type type,
                                const std::string& content,
                                float confidence,
                                const std::string& source);
    
    // Query knowledge items
    std::vector<KnowledgeItem> queryKnowledge(const std::string& query, int maxResults = 10) const;
    
    // Add a reasoning step
    std::string addReasoning(Reasoning::Type type,
                            const std::string& description,
                            const std::vector<std::string>& inputs);
    
    // Create and execute an action
    std::string createAction(Action::Type type,
                            const std::string& description,
                            const std::string& goalId,
                            float priority,
                            const nlohmann::json& parameters = nlohmann::json());
    
    // Get ongoing actions
    std::vector<Action> getOngoingActions() const;
    
    // Execute cognitive cycle
    void executeCognitiveCycle();
    
private:
    std::string m_systemId;
    std::shared_ptr<LLMManager> m_llmManager;
    std::shared_ptr<ContextManager> m_contextManager;
    std::shared_ptr<StrategyManager> m_strategyManager;
    
    // Knowledge base
    std::map<std::string, KnowledgeItem> m_knowledgeItems;
    mutable std::mutex m_knowledgeMutex;
    
    // Goals
    std::map<std::string, Goal> m_goals;
    mutable std::mutex m_goalMutex;
    
    // Reasoning steps
    std::map<std::string, Reasoning> m_reasoningSteps;
    mutable std::mutex m_reasoningMutex;
    
    // Actions
    std::map<std::string, Action> m_actions;
    mutable std::mutex m_actionMutex;
    
    // Queue of pending tasks
    struct Task {
        enum class Type {
            PROCESS_ANALYSIS,
            UPDATE_KNOWLEDGE,
            EVALUATE_GOALS,
            SELECT_ACTIONS,
            EXECUTE_ACTION,
            REFLECT
        };
        
        Type type;
        nlohmann::json parameters;
        int64_t creationTimeUs;
        int priority;
    };
    
    std::queue<Task> m_taskQueue;
    std::mutex m_taskMutex;
    
    // Recent cognitive states for reflection
    std::deque<nlohmann::json> m_recentStates;
    std::mutex m_stateMutex;
    
    // Background worker thread
    std::thread m_workerThread;
    bool m_running;
    std::condition_variable m_taskCondition;
    
    // Execute task by type
    void executeTask(const Task& task);
    
    // Worker thread function
    void workerFunction();
    
    // Generate a unique ID
    std::string generateUniqueId(const std::string& prefix) const;
    
    // Clean up old data
    void cleanupOldData();
    
    // Perception phase - process new information
    void perceive(const std::string& deviceId, const FrameAnalysisResult& result);
    
    // Cognition phase - update understanding of the world
    void cognize();
    
    // Acting phase - select and execute actions
    void act();
    
    // Reflection phase - assess performance and adapt
    void reflect();
    
    // Perception functions
    void extractFacts(const std::string& deviceId, const FrameAnalysisResult& result);
    void updateSituationModel(const FrameAnalysisResult& result);
    
    // Cognition functions
    void assessSituation();
    void identifyThreats();
    void updateGoals();
    
    // Acting functions
    void planActions();
    void selectBestAction();
    bool executeAction(const Action& action);
    
    // Reflection functions
    void evaluatePerformance();
    void updateStrategies();
    
    // Generate an LLM-assisted reasoning step
    Reasoning generateReasoningWithLLM(Reasoning::Type type, 
                                      const std::string& description,
                                      const std::vector<KnowledgeItem>& relevantKnowledge);
    
    // Use LLM to assess a situation
    nlohmann::json assessSituationWithLLM(const std::vector<KnowledgeItem>& relevantKnowledge);
    
    // Use LLM to plan actions
    std::vector<Action> planActionsWithLLM(const Goal& goal, 
                                          const std::vector<KnowledgeItem>& relevantKnowledge);
    
    // Use LLM to reflect on performance
    nlohmann::json reflectWithLLM(const std::vector<nlohmann::json>& recentStates);
};

} // namespace nx_agent
