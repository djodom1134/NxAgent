// nx_agent_strategy.cpp
#include "nx_agent_strategy.h"
#include "nx_agent_config.h"
#include "nx_agent_metadata.h"
#include "nx_agent_llm.h"
#include "nx_agent_utils.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>

namespace nx_agent {

using json = nlohmann::json;

// TrackedSubject implementation
void TrackedSubject::update(const std::string& cameraId, const DetectedObject& obj) {
    // Update last seen time
    lastSeen = std::chrono::system_clock::now();
    isActive = true;
    
    // Create a position record
    PositionRecord record;
    record.cameraId = cameraId;
    record.timestamp = lastSeen;
    
    // Calculate normalized center position
    float centerX = obj.boundingBox.x + obj.boundingBox.width / 2.0f;
    float centerY = obj.boundingBox.y + obj.boundingBox.height / 2.0f;
    
    // Frame dimensions should be known for proper normalization
    // For now, assume default resolution
    const int DEFAULT_WIDTH = 1920;
    const int DEFAULT_HEIGHT = 1080;
    
    record.normalizedPosition = {
        centerX / DEFAULT_WIDTH,
        centerY / DEFAULT_HEIGHT
    };
    
    // World position would require calibration data
    // For now, just use normalized position
    record.worldPosition = record.normalizedPosition;
    
    // Add to position history
    positionHistory.push_back(record);
    
    // Track camera appearances
    auto& appearances = cameraAppearances[cameraId];
    appearances.push_back(lastSeen);
    
    // Update attributes from object
    for (const auto& attr : obj.attributes) {
        attributes[attr.first] = attr.second;
    }
    
    // Update threat score (placeholder implementation)
    // In a real system, this would consider various factors
    if (attributes.find("recognitionStatus") != attributes.end() && 
        attributes["recognitionStatus"] == "unknown") {
        threatScore = std::min(threatScore + 0.05f, 1.0f);
    }
}

std::vector<std::pair<float, float>> TrackedSubject::getPathLine() const {
    std::vector<std::pair<float, float>> path;
    
    for (const auto& pos : positionHistory) {
        path.push_back(pos.normalizedPosition);
    }
    
    return path;
}

std::pair<float, float> TrackedSubject::predictNextPosition(float secondsAhead) const {
    // Need at least two positions to predict
    if (positionHistory.size() < 2) {
        // Return last known position if available
        if (!positionHistory.empty()) {
            return positionHistory.back().normalizedPosition;
        }
        return {0.5f, 0.5f}; // Default to center if no history
    }
    
    // Get last two positions
    const auto& lastPos = positionHistory.back();
    
    // Find previous position from same camera if possible
    const auto& lastCamera = lastPos.cameraId;
    PositionRecord prevPos;
    bool foundPrev = false;
    
    // Search for the most recent previous position from the same camera
    for (auto it = positionHistory.rbegin() + 1; it != positionHistory.rend(); ++it) {
        if (it->cameraId == lastCamera) {
            prevPos = *it;
            foundPrev = true;
            break;
        }
    }
    
    // If no previous position from same camera, use the most recent position
    if (!foundPrev) {
        prevPos = *(positionHistory.rbegin() + 1);
    }
    
    // Calculate time difference in seconds
    auto timeDiff = std::chrono::duration_cast<std::chrono::duration<float>>(
        lastPos.timestamp - prevPos.timestamp).count();
    
    // Avoid division by zero
    if (timeDiff < 0.001f) {
        timeDiff = 0.001f;
    }
    
    // Calculate velocity
    float vx = (lastPos.normalizedPosition.first - prevPos.normalizedPosition.first) / timeDiff;
    float vy = (lastPos.normalizedPosition.second - prevPos.normalizedPosition.second) / timeDiff;
    
    // Predict position
    float predictedX = lastPos.normalizedPosition.first + vx * secondsAhead;
    float predictedY = lastPos.normalizedPosition.second + vy * secondsAhead;
    
    // Clamp to valid range (0.0-1.0)
    predictedX = std::max(0.0f, std::min(1.0f, predictedX));
    predictedY = std::max(0.0f, std::min(1.0f, predictedY));
    
    return {predictedX, predictedY};
}

float TrackedSubject::calculateTrajectoryAngle() const {
    // Need at least two positions to calculate trajectory
    if (positionHistory.size() < 2) {
        return 0.0f;
    }
    
    // Get last two positions
    const auto& lastPos = positionHistory.back();
    
    // Find previous position from same camera if possible
    const auto& lastCamera = lastPos.cameraId;
    PositionRecord prevPos;
    bool foundPrev = false;
    
    // Search for the most recent previous position from the same camera
    for (auto it = positionHistory.rbegin() + 1; it != positionHistory.rend(); ++it) {
        if (it->cameraId == lastCamera) {
            prevPos = *it;
            foundPrev = true;
            break;
        }
    }
    
    // If no previous position from same camera, use the most recent position
    if (!foundPrev) {
        prevPos = *(positionHistory.rbegin() + 1);
    }
    
    // Calculate delta
    float dx = lastPos.normalizedPosition.first - prevPos.normalizedPosition.first;
    float dy = lastPos.normalizedPosition.second - prevPos.normalizedPosition.second;
    
    // Calculate angle (in radians, 0 = east, counter-clockwise)
    return std::atan2(-dy, dx); // Negative dy because screen coordinates have y-axis flipped
}

float TrackedSubject::calculateSpeed() const {
    // Need at least two positions to calculate speed
    if (positionHistory.size() < 2) {
        return 0.0f;
    }
    
    // Get last two positions
    const auto& lastPos = positionHistory.back();
    
    // Find previous position from same camera if possible
    const auto& lastCamera = lastPos.cameraId;
    PositionRecord prevPos;
    bool foundPrev = false;
    
    // Search for the most recent previous position from the same camera
    for (auto it = positionHistory.rbegin() + 1; it != positionHistory.rend(); ++it) {
        if (it->cameraId == lastCamera) {
            prevPos = *it;
            foundPrev = true;
            break;
        }
    }
    
    // If no previous position from same camera, use the most recent position
    if (!foundPrev) {
        prevPos = *(positionHistory.rbegin() + 1);
    }
    
    // Calculate time difference in seconds
    auto timeDiff = std::chrono::duration_cast<std::chrono::duration<float>>(
        lastPos.timestamp - prevPos.timestamp).count();
    
    // Avoid division by zero
    if (timeDiff < 0.001f) {
        timeDiff = 0.001f;
    }
    
    // Calculate distance (normalized)
    float dx = lastPos.normalizedPosition.first - prevPos.normalizedPosition.first;
    float dy = lastPos.normalizedPosition.second - prevPos.normalizedPosition.second;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // Calculate speed (normalized units per second)
    return distance / timeDiff;
}

std::vector<std::string> TrackedSubject::predictNextCameras(
    const std::map<std::string, CameraInfo>& cameras) const {
    
    std::vector<std::string> predictedCameras;
    
    // Need position history to predict
    if (positionHistory.empty()) {
        return predictedCameras;
    }
    
    // Get current camera
    const std::string& currentCameraId = positionHistory.back().cameraId;
    
    // Check if camera exists in the map
    auto currentCameraIt = cameras.find(currentCameraId);
    if (currentCameraIt == cameras.end()) {
        return predictedCameras;
    }
    
    // Get adjacent cameras
    const auto& adjacentCameras = currentCameraIt->second.adjacentCameras;
    
    // If no adjacent cameras defined, return empty list
    if (adjacentCameras.empty()) {
        return predictedCameras;
    }
    
    // Calculate trajectory and predict position
    float trajectoryAngle = calculateTrajectoryAngle();
    auto nextPos = predictNextPosition();
    
    // Check if predicted position is near edge of frame
    bool nearEdge = false;
    if (nextPos.first < 0.1f || nextPos.first > 0.9f ||
        nextPos.second < 0.1f || nextPos.second > 0.9f) {
        nearEdge = true;
    }
    
    // If near edge, predict which adjacent camera the subject might appear in next
    if (nearEdge) {
        // For now, use a simple heuristic based on which edge the subject is near
        if (nextPos.first < 0.1f) { // Left edge
            // Find cameras to the left
            for (const auto& adjCamId : adjacentCameras) {
                auto it = cameras.find(adjCamId);
                if (it != cameras.end() && 
                    it->second.position.x < currentCameraIt->second.position.x) {
                    predictedCameras.push_back(adjCamId);
                }
            }
        } else if (nextPos.first > 0.9f) { // Right edge
            // Find cameras to the right
            for (const auto& adjCamId : adjacentCameras) {
                auto it = cameras.find(adjCamId);
                if (it != cameras.end() && 
                    it->second.position.x > currentCameraIt->second.position.x) {
                    predictedCameras.push_back(adjCamId);
                }
            }
        }
        
        if (nextPos.second < 0.1f) { // Top edge
            // Find cameras above
            for (const auto& adjCamId : adjacentCameras) {
                auto it = cameras.find(adjCamId);
                if (it != cameras.end() && 
                    it->second.position.y < currentCameraIt->second.position.y) {
                    predictedCameras.push_back(adjCamId);
                }
            }
        } else if (nextPos.second > 0.9f) { // Bottom edge
            // Find cameras below
            for (const auto& adjCamId : adjacentCameras) {
                auto it = cameras.find(adjCamId);
                if (it != cameras.end() && 
                    it->second.position.y > currentCameraIt->second.position.y) {
                    predictedCameras.push_back(adjCamId);
                }
            }
        }
    }
    
    // Remove duplicates
    std::sort(predictedCameras.begin(), predictedCameras.end());
    predictedCameras.erase(
        std::unique(predictedCameras.begin(), predictedCameras.end()),
        predictedCameras.end()
    );
    
    return predictedCameras;
}

// SecurityIncident implementation
void SecurityIncident::addResponseAction(const std::string& actionType, 
                                       const std::string& description,
                                       const std::string& initiatedBy) {
    ResponseAction action;
    action.actionType = actionType;
    action.description = description;
    action.timestamp = std::chrono::system_clock::now();
    action.initiatedBy = initiatedBy;
    action.isComplete = false;
    
    responseActions.push_back(action);
    updateTime = action.timestamp;
}

void SecurityIncident::updateStatus(Status newStatus, const std::string& updatedBy) {
    status = newStatus;
    updateTime = std::chrono::system_clock::now();
    
    // Add a response action to track the status change
    std::string description = "Incident status changed to ";
    switch (newStatus) {
        case Status::NEW:
            description += "NEW";
            break;
        case Status::INVESTIGATING:
            description += "INVESTIGATING";
            break;
        case Status::CONFIRMED:
            description += "CONFIRMED";
            break;
        case Status::FALSE_ALARM:
            description += "FALSE_ALARM";
            break;
        case Status::RESOLVED:
            description += "RESOLVED";
            break;
        default:
            description += "UNKNOWN";
            break;
    }
    
    addResponseAction("STATUS_CHANGE", description, updatedBy);
    
    // Set resolve time if resolved
    if (newStatus == Status::RESOLVED || newStatus == Status::FALSE_ALARM) {
        resolveTime = updateTime;
    }
}

std::chrono::seconds SecurityIncident::estimateTimeToResolution() const {
    // This would typically use historical data on similar incidents
    // For now, use a simple heuristic based on severity
    switch (severity) {
        case Severity::LOW:
            return std::chrono::minutes(15);
        case Severity::MEDIUM:
            return std::chrono::minutes(30);
        case Severity::HIGH:
            return std::chrono::hours(1);
        case Severity::CRITICAL:
            return std::chrono::hours(2);
        default:
            return std::chrono::minutes(30);
    }
}

std::vector<std::string> SecurityIncident::getRecommendedActions() const {
    std::vector<std::string> actions;
    
    // Basic recommendations based on incident type
    switch (type) {
        case Type::UNKNOWN_VISITOR:
            actions.push_back("Verify visitor identity");
            actions.push_back("Check access authorization");
            actions.push_back("Monitor visitor movements");
            break;
        
        case Type::LOITERING:
            actions.push_back("Monitor subject behavior");
            actions.push_back("Verify if subject has legitimate business");
            actions.push_back("Check adjacent cameras");
            break;
        
        case Type::INTRUSION:
            actions.push_back("Verify intrusion detection");
            actions.push_back("Alert security personnel");
            actions.push_back("Initiate area lockdown");
            actions.push_back("Track intruder movements");
            break;
        
        case Type::CROWD_FORMATION:
            actions.push_back("Monitor crowd size and behavior");
            actions.push_back("Check for authorized gathering");
            actions.push_back("Alert security if crowd grows");
            break;
        
        case Type::UNUSUAL_MOVEMENT:
            actions.push_back("Continue tracking subject");
            actions.push_back("Monitor behavior for further anomalies");
            actions.push_back("Check for correlated activities");
            break;
        
        case Type::SUSPICIOUS_BEHAVIOR:
            actions.push_back("Closely observe behavior");
            actions.push_back("Check for associated objects or activities");
            actions.push_back("Prepare for intervention if behavior escalates");
            break;
        
        case Type::ABANDONED_OBJECT:
            actions.push_back("Verify object is unattended");
            actions.push_back("Track when and who left the object");
            actions.push_back("Assess potential threat");
            break;
        
        case Type::TRACKING_LOST:
            actions.push_back("Check adjacent cameras");
            actions.push_back("Review last known direction");
            actions.push_back("Set up alerts for subject reappearance");
            break;
        
        case Type::SYSTEM_ALERT:
            actions.push_back("Verify alert details");
            actions.push_back("Check system status");
            actions.push_back("Follow system alert protocol");
            break;
        
        default:
            actions.push_back("Investigate incident details");
            actions.push_back("Follow standard protocols");
            break;
    }
    
    // Add severity-specific actions
    switch (severity) {
        case Severity::HIGH:
        case Severity::CRITICAL:
            actions.push_back("Escalate to supervisor");
            actions.push_back("Prepare immediate response team");
            break;
        
        default:
            break;
    }
    
    return actions;
}

// StrategicPlan implementation
void StrategicPlan::addMonitoringStrategy(const MonitoringStrategy& strategy) {
    monitoringStrategies.push_back(strategy);
    updateTime = std::chrono::system_clock::now();
}

void StrategicPlan::addAction(const std::string& description, 
                             int priority, 
                             std::chrono::system_clock::time_point dueTime,
                             const std::string& assignedTo) {
    Action action;
    action.actionId = "ACT-" + std::to_string(actions.size() + 1);
    action.description = description;
    action.priority = priority;
    action.isComplete = false;
    action.dueTime = dueTime;
    action.assignedTo = assignedTo;
    
    actions.push_back(action);
    updateTime = std::chrono::system_clock::now();
    
    // Sort actions by priority (higher first)
    std::sort(actions.begin(), actions.end(), 
              [](const Action& a, const Action& b) {
                  return a.priority > b.priority;
              });
}

void StrategicPlan::updateStatus(Status newStatus) {
    status = newStatus;
    updateTime = std::chrono::system_clock::now();
}

bool StrategicPlan::isComplete() const {
    if (status == Status::COMPLETED || status == Status::CANCELLED) {
        return true;
    }
    
    // Check if all actions are complete
    return std::all_of(actions.begin(), actions.end(), 
                      [](const Action& action) { return action.isComplete; });
}

StrategicPlan::Action StrategicPlan::getNextAction() const {
    // Find the first incomplete action
    for (const auto& action : actions) {
        if (!action.isComplete) {
            return action;
        }
    }
    
    // Return empty action if all complete
    return {};
}

// MonitoringStrategy implementation
std::vector<std::string> MonitoringStrategy::getCamerasToWatch(
    const std::map<std::string, CameraInfo>& cameras,
    const TrackedSubject& subject) const {
    
    // If specific cameras are already defined, use those
    if (!cameraIds.empty()) {
        std::vector<std::string> result(cameraIds.begin(), cameraIds.end());
        return result;
    }
    
    // Otherwise, predict based on subject history
    auto predictedCameras = subject.predictNextCameras(cameras);
    
    // If no predictions, use the last known camera
    if (predictedCameras.empty() && !subject.positionHistory.empty()) {
        predictedCameras.push_back(subject.positionHistory.back().cameraId);
    }
    
    return predictedCameras;
}

// StrategyManager implementation
StrategyManager::StrategyManager(const std::string& systemId)
    : m_systemId(systemId)
{
}

StrategyManager::~StrategyManager() {
    // Save state if needed
}

void StrategyManager::initialize(std::shared_ptr<LLMManager> llmManager) {
    m_llmManager = llmManager;
}

void StrategyManager::configure(const nlohmann::json& config) {
    // Configure system
    if (config.contains("systemId")) {
        m_systemId = config["systemId"].get<std::string>();
    }
    
    // Configure cameras if provided
    if (config.contains("cameras") && config["cameras"].is_array()) {
        for (const auto& cameraJson : config["cameras"]) {
            CameraInfo camera;
            camera.deviceId = cameraJson["deviceId"].get<std::string>();
            camera.name = cameraJson.value("name", camera.deviceId);
            camera.location = cameraJson.value("location", "");
            camera.isActive = cameraJson.value("isActive", true);
            
            // Parse position
            if (cameraJson.contains("position")) {
                camera.position.x = cameraJson["position"].value("x", 0.0f);
                camera.position.y = cameraJson["position"].value("y", 0.0f);
                camera.position.z = cameraJson["position"].value("z", 0.0f);
                camera.position.mapReference = cameraJson["position"].value("mapReference", "");
            }
            
            // Parse coverage
            camera.viewAngle = cameraJson.value("viewAngle", 90.0f);
            camera.viewDistance = cameraJson.value("viewDistance", 10.0f);
            
            // Parse coverage area
            if (cameraJson.contains("coverageArea") && cameraJson["coverageArea"].is_array()) {
                for (const auto& point : cameraJson["coverageArea"]) {
                    if (point.is_array() && point.size() >= 2) {
                        camera.coverageArea.push_back({point[0].get<float>(), point[1].get<float>()});
                    }
                }
            }
            
            // Parse adjacent cameras
            if (cameraJson.contains("adjacentCameras") && cameraJson["adjacentCameras"].is_array()) {
                for (const auto& adjCam : cameraJson["adjacentCameras"]) {
                    camera.adjacentCameras.insert(adjCam.get<std::string>());
                }
            }
            
            registerCamera(camera);
        }
    }
}

void StrategyManager::registerCamera(const CameraInfo& camera) {
    std::lock_guard<std::mutex> lock(m_cameraMutex);
    m_cameras[camera.deviceId] = camera;
}

void StrategyManager::updateCameraStatus(const std::string& cameraId, bool isActive) {
    std::lock_guard<std::mutex> lock(m_cameraMutex);
    
    auto it = m_cameras.find(cameraId);
    if (it != m_cameras.end()) {
        it->second.isActive = isActive;
    }
}

void StrategyManager::processAnalysisResult(const std::string& cameraId, const FrameAnalysisResult& result) {
    // Process detected objects
    for (const auto& obj : result.objects) {
        updateTrackedSubject(cameraId, obj);
    }
    
    // Check for anomalies
    if (result.isAnomaly) {
        // Determine incident type
        SecurityIncident::Type incidentType = SecurityIncident::Type::SUSPICIOUS_BEHAVIOR;
        
        if (result.anomalyType == "UnknownVisitor") {
            incidentType = SecurityIncident::Type::UNKNOWN_VISITOR;
        } else if (result.anomalyType == "Loitering") {
            incidentType = SecurityIncident::Type::LOITERING;
        } else if (result.anomalyType == "Intrusion") {
            incidentType = SecurityIncident::Type::INTRUSION;
        } else if (result.anomalyType == "CrowdFormation") {
            incidentType = SecurityIncident::Type::CROWD_FORMATION;
        } else if (result.anomalyType == "AbnormalMovement") {
            incidentType = SecurityIncident::Type::UNUSUAL_MOVEMENT;
        } else if (result.anomalyType == "AbandonedObject") {
            incidentType = SecurityIncident::Type::ABANDONED_OBJECT;
        }
        
        // Determine severity
        SecurityIncident::Severity severity;
        if (result.anomalyScore > 0.85f) {
            severity = SecurityIncident::Severity::CRITICAL;
        } else if (result.anomalyScore > 0.7f) {
            severity = SecurityIncident::Severity::HIGH;
        } else if (result.anomalyScore > 0.5f) {
            severity = SecurityIncident::Severity::MEDIUM;
        } else {
            severity = SecurityIncident::Severity::LOW;
        }
        
        // Create an incident
        createIncident(incidentType, severity, cameraId, result.anomalyDescription);
    }
    
    // Update monitoring strategies based on current state
    updateMonitoringStrategies();
    
    // Check for cross-camera correlations
    checkCrossCameraCorrelations();
    
    // Clean up old data
    cleanupOldData();
    
    // Update global security state
    updateSecurityState();
}

bool StrategyManager::updateTrackedSubject(const std::string& cameraId, const DetectedObject& obj) {
    // Only track people for now
    if (obj.typeId != "person" && obj.typeId != "vehicle") {
        return false;
    }
    
    // Try to match with existing subject
    std::string subjectId = matchObjectToSubject(obj);
    
    // If no match, create new subject
    if (subjectId.empty()) {
        subjectId = createTrackedSubject(cameraId, obj);
    }
    
    // Update subject with new detection
    std::lock_guard<std::mutex> lock(m_subjectMutex);
    auto it = m_subjects.find(subjectId);
    if (it != m_subjects.end()) {
        it->second.update(cameraId, obj);
        return true;
    }
    
    return false;
}

std::string StrategyManager::createIncident(SecurityIncident::Type type,
                                         SecurityIncident::Severity severity,
                                         const std::string& cameraId,
                                         const std::string& description) {
    SecurityIncident incident;
    incident.incidentId = generateUniqueId("INC");
    incident.type = type;
    incident.severity = severity;
    incident.status = SecurityIncident::Status::NEW;
    incident.primaryCameraId = cameraId;
    incident.description = description;
    incident.startTime = std::chrono::system_clock::now();
    incident.updateTime = incident.startTime;
    
    // Add initial response action
    incident.addResponseAction("INCIDENT_CREATED", "Incident created automatically by system");
    
    // Add to incidents map
    {
        std::lock_guard<std::mutex> lock(m_incidentMutex);
        m_incidents[incident.incidentId] = incident;
    }
    
    // Generate an initial plan for the incident
    generatePlan(incident.incidentId);
    
    return incident.incidentId;
}

bool StrategyManager::updateIncident(const std::string& incidentId, 
                                  SecurityIncident::Status status,
                                  const std::string& updatedBy) {
    std::lock_guard<std::mutex> lock(m_incidentMutex);
    
    auto it = m_incidents.find(incidentId);
    if (it == m_incidents.end()) {
        return false;
    }
    
    it->second.updateStatus(status, updatedBy);
    
    // Update associated plans if incident is resolved
    if (status == SecurityIncident::Status::RESOLVED || 
        status == SecurityIncident::Status::FALSE_ALARM) {
        
        std::lock_guard<std::mutex> planLock(m_planMutex);
        
        for (auto& planPair : m_plans) {
            if (planPair.second.incidentId == incidentId) {
                planPair.second.updateStatus(StrategicPlan::Status::COMPLETED);
            }
        }
    }
    
    return true;
}

std::string StrategyManager::generatePlan(const std::string& incidentId) {
    std::lock_guard<std::mutex> incidentLock(m_incidentMutex);
    
    auto it = m_incidents.find(incidentId);
    if (it == m_incidents.end()) {
        return "";
    }
    
    StrategicPlan plan = generatePlanWithLLM(it->second);
    
    // Add to plans map
    {
        std::lock_guard<std::mutex> planLock(m_planMutex);
        m_plans[plan.planId] = plan;
    }
    
    return plan.planId;
}

bool StrategyManager::updatePlan(const std::string& planId, StrategicPlan::Status status) {
    std::lock_guard<std::mutex> lock(m_planMutex);
    
    auto it = m_plans.find(planId);
    if (it == m_plans.end()) {
        return false;
    }
    
    it->second.updateStatus(status);
    return true;
}

std::vector<SecurityIncident> StrategyManager::getActiveIncidents() const {
    std::vector<SecurityIncident> activeIncidents;
    
    std::lock_guard<std::mutex> lock(m_incidentMutex);
    
    for (const auto& pair : m_incidents) {
        const auto& incident = pair.second;
        if (incident.status != SecurityIncident::Status::RESOLVED && 
            incident.status != SecurityIncident::Status::FALSE_ALARM) {
            activeIncidents.push_back(incident);
        }
    }
    
    // Sort by severity (highest first) and then by time (newest first)
    std::sort(activeIncidents.begin(), activeIncidents.end(), 
              [](const SecurityIncident& a, const SecurityIncident& b) {
                  if (a.severity != b.severity) {
                      return static_cast<int>(a.severity) > static_cast<int>(b.severity);
                  }
                  return a.startTime > b.startTime;
              });
    
    return activeIncidents;
}

std::vector<StrategicPlan> StrategyManager::getActivePlans() const {
    std::vector<StrategicPlan> activePlans;
    
    std::lock_guard<std::mutex> lock(m_planMutex);
    
    for (const auto& pair : m_plans) {
        const auto& plan = pair.second;
        if (plan.status == StrategicPlan::Status::ACTIVE) {
            activePlans.push_back(plan);
        }
    }
    
    return activePlans;
}

std::vector<TrackedSubject> StrategyManager::getTrackedSubjects() const {
    std::vector<TrackedSubject> subjects;
    
    std::lock_guard<std::mutex> lock(m_subjectMutex);
    
    for (const auto& pair : m_subjects) {
        subjects.push_back(pair.second);
    }
    
    // Sort by threat score (highest first)
    std::sort(subjects.begin(), subjects.end(), 
              [](const TrackedSubject& a, const TrackedSubject& b) {
                  return a.threatScore > b.threatScore;
              });
    
    return subjects;
}

TrackedSubject::PositionRecord StrategyManager::predictSubjectPosition(
    const std::string& subjectId, 
    std::chrono::seconds timeAhead) {
    
    TrackedSubject::PositionRecord result;
    
    std::lock_guard<std::mutex> lock(m_subjectMutex);
    
    auto it = m_subjects.find(subjectId);
    if (it == m_subjects.end() || it->second.positionHistory.empty()) {
        return result;
    }
    
    const auto& subject = it->second;
    
    // Get last known position
    const auto& lastPos = subject.positionHistory.back();
    result = lastPos;
    
    // If not enough history to predict, return last known position
    if (subject.positionHistory.size() < 2) {
        return result;
    }
    
    // Calculate predicted position
    auto predictedPos = subject.predictNextPosition(timeAhead.count());
    
    result.normalizedPosition = predictedPos;
    result.worldPosition = predictedPos; // Would convert to world coordinates with proper calibration
    result.timestamp = std::chrono::system_clock::now() + timeAhead;
    
    return result;
}

std::string StrategyManager::getRecommendedCamera() const {
    // This would typically use a complex algorithm to determine which camera
    // to watch based on current incidents, subjects, and overall state
    // For now, use a simple heuristic based on active incidents
    
    std::vector<SecurityIncident> activeIncidents = getActiveIncidents();
    
    if (!activeIncidents.empty()) {
        // Return camera with highest severity incident
        return activeIncidents[0].primaryCameraId;
    }
    
    // If no active incidents, check tracked subjects
    std::vector<TrackedSubject> subjects = getTrackedSubjects();
    
    if (!subjects.empty() && !subjects[0].positionHistory.empty()) {
        // Return camera with highest threat subject
        return subjects[0].positionHistory.back().cameraId;
    }
    
    // If no subjects, return any active camera
    std::lock_guard<std::mutex> lock(m_cameraMutex);
    
    for (const auto& pair : m_cameras) {
        if (pair.second.isActive) {
            return pair.first;
        }
    }
    
    return "";
}

std::string StrategyManager::generateSituationReport() {
    // Use LLM to generate a comprehensive situation report
    if (!m_llmManager) {
        return "LLM Manager not initialized";
    }
    
    LLMRequest request("SYSTEM", LLMRequest::Type::SITUATION_ASSESSMENT, LLMRequest::Priority::MEDIUM);
    
    // Add active incidents
    auto incidents = getActiveIncidents();
    for (const auto& incident : incidents) {
        ContextItem item;
        item.type = ContextItem::Type::ANOMALY_DETECTION;
        item.description = "Incident: " + incident.incidentId + " - " + incident.description;
        item.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
            incident.startTime.time_since_epoch()).count();
        item.confidence = 1.0f;
        
        // Add metadata
        json metadata;
        metadata["incidentId"] = incident.incidentId;
        metadata["type"] = static_cast<int>(incident.type);
        metadata["severity"] = static_cast<int>(incident.severity);
        metadata["status"] = static_cast<int>(incident.status);
        metadata["cameraId"] = incident.primaryCameraId;
        item.metadata = metadata;
        
        request.addContextItem(item);
    }
    
    // Add tracked subjects
    auto subjects = getTrackedSubjects();
    for (const auto& subject : subjects) {
        if (!subject.isActive) {
            continue;
        }
        
        ContextItem item;
        item.type = ContextItem::Type::OBJECT_DETECTION;
        item.description = "Subject: " + subject.trackId + " - " + subject.type;
        item.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
            subject.lastSeen.time_since_epoch()).count();
        item.confidence = 1.0f;
        
        // Add metadata
        json metadata;
        metadata["subjectId"] = subject.trackId;
        metadata["type"] = subject.type;
        metadata["threatScore"] = subject.threatScore;
        metadata["status"] = subject.status;
        
        if (!subject.positionHistory.empty()) {
            metadata["currentCamera"] = subject.positionHistory.back().cameraId;
        }
        
        item.metadata = metadata;
        
        request.addContextItem(item);
    }
    
    // Add camera status
    {
        std::lock_guard<std::mutex> lock(m_cameraMutex);
        
        for (const auto& pair : m_cameras) {
            ContextItem item;
            item.type = ContextItem::Type::ENVIRONMENT_INFO;
            item.description = "Camera: " + pair.first + " - " + pair.second.name;
            item.timestampUs = TimeUtils::getCurrentTimestampUs();
            item.confidence = 1.0f;
            
            // Add metadata
            json metadata;
            metadata["cameraId"] = pair.first;
            metadata["name"] = pair.second.name;
            metadata["location"] = pair.second.location;
            metadata["isActive"] = pair.second.isActive;
            item.metadata = metadata;
            
            request.addContextItem(item);
        }
    }
    
    // Send request to LLM
    auto future = m_llmManager->submitRequest(request);
    auto response = future.get();
    
    if (!response.success) {
        return "Failed to generate situation report: " + response.errorMessage;
    }
    
    return response.reasoning;
}

std::string StrategyManager::generateUniqueId(const std::string& prefix) const {
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

std::string StrategyManager::matchObjectToSubject(const DetectedObject& obj) {
    std::lock_guard<std::mutex> lock(m_subjectMutex);
    
    // If object has a track ID, try to match directly
    if (!obj.trackId.empty()) {
        auto it = m_subjects.find(obj.trackId);
        if (it != m_subjects.end()) {
            return obj.trackId;
        }
    }
    
    // TODO: Implement more sophisticated matching logic
    // This would typically use appearance similarity, position prediction, etc.
    
    return ""; // No match found
}

std::string StrategyManager::createTrackedSubject(const std::string& cameraId, const DetectedObject& obj) {
    TrackedSubject subject;
    
    // Use object's track ID if available, otherwise generate one
    if (!obj.trackId.empty()) {
        subject.trackId = obj.trackId;
    } else {
        subject.trackId = generateUniqueId("SUBJ");
    }
    
    subject.type = obj.typeId;
    subject.firstSeen = std::chrono::system_clock::now();
    subject.lastSeen = subject.firstSeen;
    subject.isActive = true;
    subject.threatScore = 0.0f;
    subject.status = "tracking";
    
    // Copy attributes
    for (const auto& attr : obj.attributes) {
        subject.attributes[attr.first] = attr.second;
    }
    
    // Update with initial detection
    subject.update(cameraId, obj);
    
    // Add to subjects map
    {
        std::lock_guard<std::mutex> lock(m_subjectMutex);
        m_subjects[subject.trackId] = subject;
    }
    
    return subject.trackId;
}

StrategicPlan StrategyManager::generatePlanWithLLM(const SecurityIncident& incident) {
    // Create a new plan
    StrategicPlan plan;
    plan.planId = generateUniqueId("PLAN");
    plan.incidentId = incident.incidentId;
    plan.createTime = std::chrono::system_clock::now();
    plan.updateTime = plan.createTime;
    plan.description = "Response plan for " + incident.description;
    plan.status = StrategicPlan::Status::ACTIVE;
    
    // Create default monitoring strategy
    MonitoringStrategy monitoringStrategy;
    monitoringStrategy.subjectId = "";  // Apply to all subjects in the incident
    monitoringStrategy.type = MonitoringStrategy::Type::ACTIVE;
    monitoringStrategy.priorityScore = 0.7f;
    monitoringStrategy.cameraIds = {incident.primaryCameraId};
    
    // Add adjacent cameras if available
    std::vector<std::string> adjacentCameras = getAdjacentCameras(incident.primaryCameraId);
    monitoringStrategy.cameraIds.insert(adjacentCameras.begin(), adjacentCameras.end());
    
    monitoringStrategy.startTime = std::chrono::system_clock::now();
    monitoringStrategy.updateTime = monitoringStrategy.startTime;
    monitoringStrategy.duration = std::chrono::minutes(30);  // Default monitoring duration
    monitoringStrategy.reason = "Incident response";
    monitoringStrategy.samplingRate = 5;  // 5 fps
    monitoringStrategy.enablePrediction = true;
    monitoringStrategy.alertOnLoss = true;
    monitoringStrategy.crossCameraTracking = true;
    
    plan.addMonitoringStrategy(monitoringStrategy);
    
    // Add default actions if LLM is not available
    if (!m_llmManager) {
        // Add basic actions based on incident type
        auto recommendedActions = incident.getRecommendedActions();
        
        auto now = std::chrono::system_clock::now();
        for (size_t i = 0; i < recommendedActions.size(); ++i) {
            int priority = 10 - static_cast<int>(i); // Higher index = lower priority
            auto dueTime = now + std::chrono::minutes(i * 5);
            plan.addAction(recommendedActions[i], priority, dueTime);
        }
        
        return plan;
    }
    
    // Use LLM to generate a comprehensive plan
    LLMRequest request(incident.primaryCameraId, LLMRequest::Type::RESPONSE_PLANNING, LLMRequest::Priority::HIGH);
    
    // Add incident details
    ContextItem incidentItem;
    incidentItem.type = ContextItem::Type::ANOMALY_DETECTION;
    incidentItem.description = "Incident: " + incident.incidentId + " - " + incident.description;
    incidentItem.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
        incident.startTime.time_since_epoch()).count();
    incidentItem.confidence = 1.0f;
    
    // Add metadata
    json metadata;
    metadata["incidentId"] = incident.incidentId;
    metadata["type"] = static_cast<int>(incident.type);
    metadata["severity"] = static_cast<int>(incident.severity);
    metadata["status"] = static_cast<int>(incident.status);
    metadata["cameraId"] = incident.primaryCameraId;
    incidentItem.metadata = metadata;
    
    request.addContextItem(incidentItem);
    
    // Add context about related subjects
    for (const auto& subjectId : incident.relatedSubjectIds) {
        std::lock_guard<std::mutex> lock(m_subjectMutex);
        
        auto it = m_subjects.find(subjectId);
        if (it != m_subjects.end()) {
            const auto& subject = it->second;
            
            ContextItem subjectItem;
            subjectItem.type = ContextItem::Type::OBJECT_DETECTION;
            subjectItem.description = "Subject: " + subject.trackId + " - " + subject.type;
            subjectItem.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
                subject.lastSeen.time_since_epoch()).count();
            subjectItem.confidence = 1.0f;
            
            // Add metadata
            json subjectMetadata;
            subjectMetadata["subjectId"] = subject.trackId;
            subjectMetadata["type"] = subject.type;
            subjectMetadata["threatScore"] = subject.threatScore;
            subjectMetadata["status"] = subject.status;
            
            if (!subject.positionHistory.empty()) {
                subjectMetadata["currentCamera"] = subject.positionHistory.back().cameraId;
            }
            
            subjectItem.metadata = subjectMetadata;
            
            request.addContextItem(subjectItem);
        }
    }
    
    // Send request to LLM
    auto future = m_llmManager->submitRequest(request);
    auto response = future.get();
    
    if (response.success && !response.actions.empty()) {
        // Add LLM-generated actions to the plan
        auto now = std::chrono::system_clock::now();
        
        for (size_t i = 0; i < response.actions.size(); ++i) {
            const auto& action = response.actions[i];
            
            // Skip monitor actions (already handled by monitoring strategy)
            if (action.type == LLMResponse::Action::Type::MONITOR) {
                continue;
            }
            
            // Convert LLM action to plan action
            int priority = 10 - static_cast<int>(i); // Higher index = lower priority
            auto dueTime = now + std::chrono::minutes(i * 5);
            
            plan.addAction(action.description, priority, dueTime);
        }
    } else {
        // Fallback to default actions if LLM fails
        auto recommendedActions = incident.getRecommendedActions();
        
        auto now = std::chrono::system_clock::now();
        for (size_t i = 0; i < recommendedActions.size(); ++i) {
            int priority = 10 - static_cast<int>(i); // Higher index = lower priority
            auto dueTime = now + std::chrono::minutes(i * 5);
            plan.addAction(recommendedActions[i], priority, dueTime);
        }
    }
    
    return plan;
}

std::vector<std::string> StrategyManager::getAdjacentCameras(const std::string& cameraId) const {
    std::lock_guard<std::mutex> lock(m_cameraMutex);
    
    std::vector<std::string> adjacentCameras;
    
    auto it = m_cameras.find(cameraId);
    if (it != m_cameras.end()) {
        for (const auto& adjCamId : it->second.adjacentCameras) {
            adjacentCameras.push_back(adjCamId);
        }
    }
    
    return adjacentCameras;
}

float StrategyManager::calculateThreatScore(const TrackedSubject& subject) const {
    float score = subject.threatScore;
    
    // Adjust based on active incidents
    {
        std::lock_guard<std::mutex> lock(m_incidentMutex);
        
        for (const auto& pair : m_incidents) {
            const auto& incident = pair.second;
            
            // Skip inactive incidents
            if (incident.status == SecurityIncident::Status::RESOLVED || 
                incident.status == SecurityIncident::Status::FALSE_ALARM) {
                continue;
            }
            
            // Check if subject is related to this incident
            auto it = std::find(incident.relatedSubjectIds.begin(), 
                               incident.relatedSubjectIds.end(), 
                               subject.trackId);
            
            if (it != incident.relatedSubjectIds.end()) {
                // Boost score based on incident severity
                switch (incident.severity) {
                    case SecurityIncident::Severity::CRITICAL:
                        score += 0.3f;
                        break;
                    case SecurityIncident::Severity::HIGH:
                        score += 0.2f;
                        break;
                    case SecurityIncident::Severity::MEDIUM:
                        score += 0.1f;
                        break;
                    case SecurityIncident::Severity::LOW:
                        score += 0.05f;
                        break;
                }
            }
        }
    }
    
    // Clamp to valid range
    return std::max(0.0f, std::min(1.0f, score));
}

void StrategyManager::updateMonitoringStrategies() {
    // This would update all monitoring strategies based on current state
    // For now, just a placeholder
}

void StrategyManager::checkCrossCameraCorrelations() {
    // This would look for correlations between activities on different cameras
    // For now, just a placeholder
}

void StrategyManager::cleanupOldData() {
    auto now = std::chrono::system_clock::now();
    
    // Clean up inactive subjects
    {
        std::lock_guard<std::mutex> lock(m_subjectMutex);
        
        auto it = m_subjects.begin();
        while (it != m_subjects.end()) {
            const auto& subject = it->second;
            
            // Remove subjects not seen for over 10 minutes
            auto timeSinceLastSeen = std::chrono::duration_cast<std::chrono::minutes>(
                now - subject.lastSeen).count();
            
            if (timeSinceLastSeen > 10) {
                it = m_subjects.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Clean up old incidents (keep for historical reference but mark as inactive)
    {
        std::lock_guard<std::mutex> lock(m_incidentMutex);
        
        for (auto& pair : m_incidents) {
            auto& incident = pair.second;
            
            // If incident is active but hasn't been updated in over 30 minutes, mark as resolved
            if (incident.status != SecurityIncident::Status::RESOLVED && 
                incident.status != SecurityIncident::Status::FALSE_ALARM) {
                
                auto timeSinceUpdate = std::chrono::duration_cast<std::chrono::minutes>(
                    now - incident.updateTime).count();
                
                if (timeSinceUpdate > 30) {
                    incident.updateStatus(SecurityIncident::Status::RESOLVED, "system_timeout");
                }
            }
        }
    }
    
    // Clean up old plans
    {
        std::lock_guard<std::mutex> lock(m_planMutex);
        
        auto it = m_plans.begin();
        while (it != m_plans.end()) {
            const auto& plan = it->second;
            
            // Remove plans older than 1 day and not active
            auto timeSinceCreation = std::chrono::duration_cast<std::chrono::hours>(
                now - plan.createTime).count();
            
            if (timeSinceCreation > 24 && plan.status != StrategicPlan::Status::ACTIVE) {
                it = m_plans.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void StrategyManager::updateSecurityState() {
    // This would update the overall security state based on current situation
    // For now, just a placeholder
}

} // namespace nx_agent
