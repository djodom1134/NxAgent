// nx_agent_strategy.h
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <chrono>
#include <nlohmann/json.hpp>

namespace nx_agent {

// Forward declarations
class DeviceConfig;
struct FrameAnalysisResult;
struct LLMResponse;
class LLMManager;
class ContextManager;
struct DetectedObject;

/**
 * Represents a camera in a multi-camera setup
 */
struct CameraInfo {
    std::string deviceId;
    std::string name;
    std::string location;
    bool isActive;
    std::set<std::string> adjacentCameras;
    
    // Spatial information
    struct Position {
        float x;
        float y;
        float z;
        std::string mapReference;
    } position;
    
    // Coverage details
    float viewAngle;
    float viewDistance;
    std::vector<std::pair<float, float>> coverageArea; // Normalized coordinates
};

/**
 * Represents a tracked subject across cameras
 */
struct TrackedSubject {
    std::string trackId;
    std::string type; // "person", "vehicle", etc.
    std::map<std::string, std::vector<std::chrono::system_clock::time_point>> cameraAppearances;
    std::map<std::string, std::string> attributes; // Descriptive attributes
    std::chrono::system_clock::time_point firstSeen;
    std::chrono::system_clock::time_point lastSeen;
    bool isActive;
    float threatScore;
    std::string status; // "tracking", "lost", "verified", etc.
    
    // Historical positions
    struct PositionRecord {
        std::string cameraId;
        std::chrono::system_clock::time_point timestamp;
        std::pair<float, float> normalizedPosition; // In frame coordinates (0.0-1.0)
        std::pair<float, float> worldPosition; // If mapped to real-world coordinates
    };
    
    std::vector<PositionRecord> positionHistory;
    
    // Update from a detected object
    void update(const std::string& cameraId, const DetectedObject& obj);
    
    // Calculate path line for visualization
    std::vector<std::pair<float, float>> getPathLine() const;
    
    // Predict next position
    std::pair<float, float> predictNextPosition(float secondsAhead = 5.0f) const;
    
    // Calculate trajectory
    float calculateTrajectoryAngle() const; // In radians, 0 = east, counter-clockwise
    float calculateSpeed() const; // In relative units (pixels/sec)
    
    // Find cameras where subject might appear next
    std::vector<std::string> predictNextCameras(
        const std::map<std::string, CameraInfo>& cameras) const;
};

/**
 * Represents a security incident requiring response
 */
struct SecurityIncident {
    enum class Type {
        UNKNOWN_VISITOR,
        LOITERING,
        INTRUSION,
        CROWD_FORMATION,
        UNUSUAL_MOVEMENT,
        SUSPICIOUS_BEHAVIOR,
        ABANDONED_OBJECT,
        TRACKING_LOST,
        SYSTEM_ALERT
    };
    
    enum class Severity {
        LOW,
        MEDIUM,
        HIGH,
        CRITICAL
    };
    
    enum class Status {
        NEW,
        INVESTIGATING,
        CONFIRMED,
        FALSE_ALARM,
        RESOLVED
    };
    
    std::string incidentId;
    Type type;
    Severity severity;
    Status status;
    std::string primaryCameraId;
    std::vector<std::string> relatedCameraIds;
    std::vector<std::string> relatedSubjectIds;
    std::string description;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point updateTime;
    std::chrono::system_clock::time_point resolveTime;
    
    // Response information
    struct ResponseAction {
        std::string actionType;
        std::string description;
        std::chrono::system_clock::time_point timestamp;
        std::string initiatedBy; // "system", "operator", etc.
        bool isComplete;
        std::string outcome;
    };
    
    std::vector<ResponseAction> responseActions;
    std::string assignedOperator;
    
    // Add a response action
    void addResponseAction(const std::string& actionType, 
                           const std::string& description,
                           const std::string& initiatedBy = "system");
    
    // Update incident status
    void updateStatus(Status newStatus, const std::string& updatedBy = "system");
    
    // Calculate estimated time to resolution based on similar past incidents
    std::chrono::seconds estimateTimeToResolution() const;
    
    // Get recommended next actions
    std::vector<std::string> getRecommendedActions() const;
};

/**
 * Strategy for monitoring a subject
 */
struct MonitoringStrategy {
    enum class Type {
        PASSIVE,    // Normal monitoring
        ACTIVE,     // Increased attention
        PRIORITY,   // High priority monitoring
        TRACKING    // Active tracking across cameras
    };
    
    std::string subjectId;
    Type type;
    float priorityScore;
    std::set<std::string> cameraIds;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point updateTime;
    std::chrono::minutes duration;  // How long to maintain this strategy
    std::string reason;
    
    // Specific monitoring parameters
    int samplingRate; // Frames per second to analyze
    bool enablePrediction;
    bool alertOnLoss;
    bool crossCameraTracking;
    
    // Get cameras to watch for subject
    std::vector<std::string> getCamerasToWatch(
        const std::map<std::string, CameraInfo>& cameras,
        const TrackedSubject& subject) const;
};

/**
 * Strategic plan for security response
 */
struct StrategicPlan {
    std::string planId;
    std::string incidentId; // Related incident if any
    std::chrono::system_clock::time_point createTime;
    std::chrono::system_clock::time_point updateTime;
    std::string description;
    
    // Plan elements
    std::vector<MonitoringStrategy> monitoringStrategies;
    
    struct Action {
        std::string actionId;
        std::string description;
        int priority; // 1-10, higher is more important
        bool isComplete;
        std::chrono::system_clock::time_point dueTime;
        std::string assignedTo; // "system", "operator", etc.
    };
    
    std::vector<Action> actions;
    
    // Plan status
    enum class Status {
        DRAFT,
        ACTIVE,
        COMPLETED,
        CANCELLED
    };
    
    Status status = Status::DRAFT;
    
    // Add a monitoring strategy
    void addMonitoringStrategy(const MonitoringStrategy& strategy);
    
    // Add an action
    void addAction(const std::string& description, 
                  int priority, 
                  std::chrono::system_clock::time_point dueTime,
                  const std::string& assignedTo = "system");
    
    // Update plan status
    void updateStatus(Status newStatus);
    
    // Check if plan is complete
    bool isComplete() const;
    
    // Get next action
    Action getNextAction() const;
};

/**
 * Main class for strategic planning and execution
 */
class StrategyManager {
public:
    StrategyManager(const std::string& systemId);
    ~StrategyManager();
    
    // Initialize and configure
    void initialize(std::shared_ptr<LLMManager> llmManager);
    void configure(const nlohmann::json& config);
    
    // Register a camera
    void registerCamera(const CameraInfo& camera);
    
    // Update camera status
    void updateCameraStatus(const std::string& cameraId, bool isActive);
    
    // Process a frame analysis result
    void processAnalysisResult(const std::string& cameraId, const FrameAnalysisResult& result);
    
    // Update subject tracking
    bool updateTrackedSubject(const std::string& cameraId, const DetectedObject& obj);
    
    // Create a new incident
    std::string createIncident(SecurityIncident::Type type,
                              SecurityIncident::Severity severity,
                              const std::string& cameraId,
                              const std::string& description);
    
    // Update an incident
    bool updateIncident(const std::string& incidentId, 
                       SecurityIncident::Status status,
                       const std::string& updatedBy = "system");
    
    // Generate a strategic plan for an incident
    std::string generatePlan(const std::string& incidentId);
    
    // Update a strategic plan
    bool updatePlan(const std::string& planId, StrategicPlan::Status status);
    
    // Get current active incidents
    std::vector<SecurityIncident> getActiveIncidents() const;
    
    // Get current active plans
    std::vector<StrategicPlan> getActivePlans() const;
    
    // Get tracked subjects
    std::vector<TrackedSubject> getTrackedSubjects() const;
    
    // Predict subject movement
    TrackedSubject::PositionRecord predictSubjectPosition(
        const std::string& subjectId, 
        std::chrono::seconds timeAhead);
    
    // Get recommended camera to watch based on current situation
    std::string getRecommendedCamera() const;
    
    // Generate a situation report
    std::string generateSituationReport();
    
private:
    std::string m_systemId;
    std::shared_ptr<LLMManager> m_llmManager;
    
    // Camera information
    std::map<std::string, CameraInfo> m_cameras;
    mutable std::mutex m_cameraMutex;
    
    // Tracked subjects
    std::map<std::string, TrackedSubject> m_subjects;
    mutable std::mutex m_subjectMutex;
    
    // Active incidents
    std::map<std::string, SecurityIncident> m_incidents;
    mutable std::mutex m_incidentMutex;
    
    // Strategic plans
    std::map<std::string, StrategicPlan> m_plans;
    mutable std::mutex m_planMutex;
    
    // Generate a unique ID
    std::string generateUniqueId(const std::string& prefix) const;
    
    // Match a detected object to an existing tracked subject
    std::string matchObjectToSubject(const DetectedObject& obj);
    
    // Create a new tracked subject from object
    std::string createTrackedSubject(const std::string& cameraId, const DetectedObject& obj);
    
    // Generate a strategic plan using LLM
    StrategicPlan generatePlanWithLLM(const SecurityIncident& incident);
    
    // Get adjacent cameras
    std::vector<std::string> getAdjacentCameras(const std::string& cameraId) const;
    
    // Calculate threat score for a subject
    float calculateThreatScore(const TrackedSubject& subject) const;
    
    // Update monitoring strategies based on current situation
    void updateMonitoringStrategies();
    
    // Check for cross-camera correlations
    void checkCrossCameraCorrelations();
    
    // Clean up old data
    void cleanupOldData();
    
    // Update global security state
    void updateSecurityState();
};

} // namespace nx_agent
