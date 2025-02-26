# NX Agent Operator Manual

## Introduction

The NX Agent plugin is an advanced AI-powered security monitoring system that integrates with the Nx Meta VMS. This manual provides operators with the information needed to effectively use and manage the enhanced NX Agent system.

## System Overview

The NX Agent system combines traditional video analytics with advanced AI capabilities to provide:

- Object detection and tracking
- Anomaly detection based on learned patterns
- Advanced reasoning and situation assessment
- Cross-camera subject tracking
- Intelligent security recommendations

## User Interface

### Main Dashboard

The NX Agent interface is integrated into the Nx Meta VMS dashboard. Key elements include:

1. **Camera Views**: Live video feeds with AI-enhanced overlays
2. **Event Panel**: Real-time alerts and notifications
3. **Status Panel**: System status and AI processing information
4. **Analytics Dashboard**: Insights and statistics from the AI system

### AI Status Indicators

Each camera view includes AI status indicators:

- **Learning Mode**: Blue indicator showing the system is learning normal patterns
- **Monitoring Mode**: Green indicator showing normal monitoring
- **Alert Mode**: Red indicator showing an anomaly has been detected
- **AI Processing**: Purple indicator showing active AI reasoning

## Working with Events

### Event Types

The NX Agent system generates several types of events:

1. **Anomaly Detected**: General anomaly based on unusual patterns
2. **Unknown Visitor**: Unrecognized person detected in the scene
3. **Abnormal Activity**: Unusual behavior patterns detected
4. **Status Events**: System status updates and notifications

### Event Handling

When an event occurs:

1. The event appears in the Event Panel with details
2. The relevant camera view is highlighted
3. Object tracking overlays show the subject of interest
4. The AI reasoning provides context and recommendations

### Event Investigation

To investigate an event:

1. Click on the event in the Event Panel
2. Review the AI analysis and reasoning provided
3. Use the timeline to review footage before and after the event
4. Check cross-camera correlations if available
5. Review the AI recommendations for response

## AI Capabilities

### Learning Mode

When first deployed, the system operates in Learning Mode:

- Collects baseline data about normal activity
- Builds models of typical patterns and behaviors
- Automatically transitions to Monitoring Mode when sufficient data is collected
- Learning progress is displayed in the Status Panel

To manually control Learning Mode:

1. Go to Camera Settings > NX Agent > Learning
2. Enable or disable Learning Mode
3. Reset learning data if needed

### Reasoning System

The AI reasoning system provides:

- Context for detected anomalies
- Assessment of the security situation
- Recommendations for operator response
- Predictive insights about potential future events

To access reasoning insights:

1. Select an event or camera of interest
2. Open the AI Insights panel
3. Review the reasoning and recommendations provided

### Cross-Camera Tracking

The system can track subjects across multiple cameras:

1. When a subject moves between camera views, the tracking ID is maintained
2. The system predicts where the subject might appear next
3. Path visualization shows the subject's movement through the facility
4. Historical tracking data is available for investigation

## Configuration

### System Settings

Global system settings are configured in the Nx Meta VMS settings:

1. Go to System Settings > Plugins > NX Agent
2. Configure general settings:
   - Sensitivity level
   - Learning parameters
   - Notification settings

### Camera-Specific Settings

Each camera can have custom settings:

1. Go to Camera Settings > NX Agent
2. Configure camera-specific settings:
   - Detection regions
   - Minimum confidence thresholds
   - Anomaly detection parameters
   - AI reasoning settings

### AI Settings

Advanced AI settings are configured in the AI Settings panel:

1. Go to System Settings > Plugins > NX Agent > AI Settings
2. Configure AI parameters:
   - LLM integration settings
   - Reasoning intervals
   - Cross-camera correlation

## Best Practices

### Optimal Camera Placement

For best results:

- Position cameras to minimize occlusions
- Ensure adequate lighting for clear video
- Cover entry/exit points with multiple cameras when possible
- Maintain consistent camera positioning

### System Tuning

To optimize system performance:

1. **Sensitivity Adjustment**:
   - Increase sensitivity in high-security areas
   - Decrease sensitivity in busy public areas
   - Adjust based on false positive/negative rates

2. **Learning Period**:
   - Allow at least one week of learning for basic patterns
   - Include weekdays and weekends for complete patterns
   - Re-learn after significant changes to the environment

3. **AI Reasoning**:
   - Set shorter reasoning intervals for critical areas
   - Use longer intervals for less critical areas
   - Balance reasoning depth with system performance

### Alert Management

For effective alert handling:

1. Prioritize alerts based on AI-provided context
2. Review the reasoning behind each alert
3. Follow AI recommendations when appropriate
4. Provide feedback to improve system accuracy

## Troubleshooting

### Common Issues

1. **False Positives**:
   - Decrease sensitivity settings
   - Define exclusion zones for busy areas
   - Extend learning period for better baseline

2. **False Negatives**:
   - Increase sensitivity settings
   - Check camera positioning and lighting
   - Ensure detection regions cover critical areas

3. **System Performance**:
   - Adjust reasoning intervals
   - Limit cross-camera analysis to critical areas
   - Check network bandwidth and server resources

### Support Resources

For additional support:

- In-app help: Click the Help icon in the NX Agent panel
- Documentation: Full documentation available in the docs folder
- Support: Contact technical support through the Nx Meta support portal

## Advanced Features

### Security Recommendations

The AI system provides security recommendations based on its analysis:

1. Immediate response actions for detected threats
2. Long-term security improvements
3. Staffing and patrol recommendations
4. System configuration suggestions

### Cognitive Status Reports

Periodic cognitive status reports provide insights into the system's understanding:

1. Overall security situation assessment
2. Patterns and trends identified
3. Potential areas of concern
4. Confidence levels in current assessments

### Custom Goals

Operators can set custom goals for the AI system:

1. Go to AI Settings > Goals
2. Create a new goal with description and priority
3. The AI system will work toward achieving the goal
4. Progress and results are reported in the Goals panel

## Conclusion

The enhanced NX Agent system provides powerful AI capabilities that transform traditional video surveillance into an intelligent security assistant. By understanding the system's features and following best practices, operators can maximize the effectiveness of this advanced security tool.
