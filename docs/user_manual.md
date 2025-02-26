# NX Agent AI Security Guard User Manual

This manual provides instructions for security operators and administrators on using the NX Agent AI Security Guard plugin with Network Optix Nx Meta VMS.

## Table of Contents

1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Everyday Operation](#everyday-operation)
   - [Understanding the Interface](#understanding-the-interface)
   - [Types of Alerts](#types-of-alerts)
   - [Responding to Alerts](#responding-to-alerts)
4. [Working with Anomaly Detection](#working-with-anomaly-detection)
   - [Learning Mode](#learning-mode)
   - [Tuning Sensitivity](#tuning-sensitivity)
   - [Managing False Alarms](#managing-false-alarms)
5. [Camera Setup Guide](#camera-setup-guide)
   - [Optimal Camera Placement](#optimal-camera-placement)
   - [Configuration Templates](#configuration-templates)
6. [Custom Response Procedures](#custom-response-procedures)
7. [Best Practices](#best-practices)
8. [FAQ](#faq)

## Introduction

The NX Agent AI Security Guard is an intelligent video analytics plugin that transforms your surveillance system from passive monitoring to active security. It uses advanced AI techniques to:

- Learn normal patterns of activity in your environment
- Detect unusual events that may indicate security threats
- Alert security personnel to potential issues
- Execute automated response procedures

This manual will help you get the most out of the NX Agent plugin, whether you're a security operator responding to alerts or an administrator configuring the system.

## Getting Started

### First-Time Setup

After installation, the plugin requires initial configuration:

1. **Enable the Plugin**: An administrator must first enable the plugin in Nx Meta Server Settings.

2. **Configure Cameras**: Enable the plugin on individual cameras and configure detection settings.

3. **Learning Period**: When first enabled, the plugin enters a learning phase to understand normal activity patterns. This typically lasts 8-12 hours.

4. **Set Up Responses**: Configure how the system should respond to detected anomalies using Nx Meta's Event Rules.

### Quick Start for Operators

If the plugin has already been set up by your administrator:

1. Familiarize yourself with the Alarm Layout in Nx Meta Desktop Client.
2. Understand what types of alerts may appear (see [Types of Alerts](#types-of-alerts)).
3. Know your organization's procedures for responding to different alerts.
4. Be prepared to provide feedback on false alarms to help improve the system.

## Everyday Operation

### Understanding the Interface

The NX Agent plugin integrates seamlessly with the Nx Meta interface. Here's how you'll interact with it:

#### Alarm Layout

When an anomaly is detected, the relevant camera may automatically appear in the Alarm Layout (depending on your configuration). Look for:

- A red border around the camera tile
- Bounding boxes highlighting objects of interest
- Text labels indicating object types and confidence scores

#### Event Log

All anomalies are logged in the Nx Meta Event Log. To view:

1. Click on the Event Log button in the Nx Meta Desktop Client
2. Look for events with the source "NX Agent AI Security Guard"
3. Click on an event to see details and jump to the relevant video time

#### Object Search

Objects detected by NX Agent can be found using Nx Meta's Object Search:

1. Go to the Search tab
2. Select "Objects" search type
3. Choose the object type (e.g., "Person" or "Vehicle")
4. Filter by time or camera
5. Browse the results to find specific objects

### Types of Alerts

The plugin generates several types of alerts:

#### Anomaly Detected

General unusual activity that deviates from normal patterns. This could include:
- Unexpected motion during off-hours
- Unusual traffic patterns
- Significant changes in occupancy or activity levels

#### Unknown Visitor

An unrecognized person who has remained in the scene longer than the configured threshold time. This alert is particularly useful for:
- Detecting loitering behavior
- Identifying unauthorized personnel in secure areas
- Noticing prolonged surveillance or reconnaissance activities

#### Abnormal Activity

Specific suspicious activities identified by the system, such as:
- Loitering in restricted areas
- Unusual motion patterns (like pacing or erratic movement)
- After-hours presence in normally vacant areas

#### Status Events

These indicate changes in the plugin's operational state:
- "Learning Mode Active" - Initial learning phase
- "Learning Complete" - Transition to detection mode
- "Model Updated" - Continuous learning has updated detection parameters

### Responding to Alerts

When an alert is triggered, follow these general steps:

1. **Assess the Situation**
   - Review the live video to understand what triggered the alert
   - Note the type of anomaly and its severity
   - Check for related events or multiple cameras showing anomalies

2. **Verify the Alert**
   - Determine if this is a genuine security concern or a false alarm
   - Check related cameras or other security systems if available
   - Consider context (time of day, business operations, etc.)

3. **Take Appropriate Action**
   - Follow your organization's specific procedures for the type of alert
   - Document your observations and actions taken
   - Escalate to appropriate personnel if needed

4. **Provide Feedback**
   - If the alert was a false alarm, note the circumstances
   - Report persistent false alarm patterns to your administrator
   - Suggest adjustment of sensitivity if alerts are too frequent or too rare

## Working with Anomaly Detection

### Learning Mode

When first enabled on a camera, the plugin enters a learning phase:

- During this time, it observes "normal" activity to establish a baseline
- The plugin will not generate anomaly alerts during initial learning
- A status event "Learning Mode Active" will be shown
- Learning typically takes 8-12 hours to complete

After the initial learning phase, the plugin automatically transitions to detection mode. If continuous learning is enabled, the system will continue to refine its understanding of normal patterns over time.

### Tuning Sensitivity

If you're receiving too many or too few alerts, the sensitivity can be adjusted:

1. **Anomaly Threshold** (0.0-1.0):
   - Lower values (e.g., 0.5) result in more alerts, capturing subtle anomalies
   - Higher values (e.g., 0.8) reduce alerts, focusing on more obvious deviations
   - Start at 0.7 and adjust based on performance

2. **Minimum Confidence Thresholds**:
   - These control the minimum confidence required for object detection
   - Raising these reduces false detections but may miss some objects
   - Typical values are 0.6-0.7 for a good balance

3. **Unknown Visitor Threshold**:
   - This is the time (in seconds) before an unknown person is flagged
   - Shorter times are more sensitive but may generate more false alerts
   - Default is 300 seconds (5 minutes)

### Managing False Alarms

False alarms are inevitable in any security system. Here's how to minimize them:

1. **Identify Patterns**:
   - Note the circumstances of false alarms (time, location, activity)
   - Look for common triggers like lighting changes, reflections, or regular deliveries

2. **Adjust Configuration**:
   - Define regions of interest to exclude areas that commonly trigger false alarms
   - Set business hours accurately to reflect when activity is expected
   - Increase confidence thresholds for specific detection types

3. **Environment Adjustments**:
   - Improve lighting consistency
   - Remove or relocate objects that cause confusing reflections or shadows
   - Consider camera repositioning if needed

4. **Continuous Improvement**:
   - The system learns over time if continuous learning is enabled
   - Regular feedback helps improve performance
   - Periodic review of settings helps optimize the system

## Camera Setup Guide

### Optimal Camera Placement

For best results with NX Agent, consider these camera placement guidelines:

1. **Field of View**:
   - Ensure key areas are clearly visible
   - Avoid excessive coverage of public spaces or high-traffic areas unless specifically monitoring them
   - Position to capture faces when possible

2. **Lighting Considerations**:
   - Ensure consistent lighting throughout the day
   - Avoid direct sunlight on the camera or in the field of view
   - Provide sufficient lighting for night operation
   - Consider IR or low-light cameras for 24/7 monitoring

3. **Camera Height and Angle**:
   - Mount at 2.5-3m (8-10ft) height for best perspective
   - Angle downward at approximately 30-45 degrees
   - Avoid extreme top-down views that make object recognition difficult

4. **Coverage Strategy**:
   - Focus on entry/exit points
   - Cover chokepoints where people must pass
   - Ensure restricted areas have complete coverage
   - Consider overlapping coverage for critical areas

### Configuration Templates

These templates provide starting points for different scenarios:

#### Entry Monitoring

```
Anomaly Threshold: 0.65
Enable Unknown Visitor Detection: Yes
Unknown Visitor Threshold: 180 seconds (3 minutes)
Business Hours: Set to facility operating hours
```

This configuration focuses on identifying unauthorized or lingering visitors at entry points.

#### After-Hours Security

```
Anomaly Threshold: 0.6
Enable Unknown Visitor Detection: Yes
Unknown Visitor Threshold: 60 seconds (1 minute)
Business Hours: Set to facility operating hours
```

This more sensitive configuration is appropriate for detecting any activity outside business hours.

#### Warehouse/Storage Areas

```
Anomaly Threshold: 0.7
Enable Unknown Visitor Detection: Yes
Unknown Visitor Threshold: 300 seconds (5 minutes)
Detection Regions: Focus on high-value storage areas
```

This balanced configuration monitors for unusual dwell time near valuable inventory.

## Custom Response Procedures

The NX Agent plugin can trigger various automated responses when anomalies are detected:

### Nx Meta Built-in Responses

These are configured through the Nx Meta Event Rules:

1. **Display on Alarm Layout**: Show the camera feed in the alarm pane
2. **Play Sound**: Alert operators with an audible notification
3. **Send Email/SMS**: Notify security personnel
4. **Start Recording**: Ensure high-quality recording of the event
5. **Execute HTTP Request**: Trigger external systems

### SIP Integration (Phone Calls)

If configured, high-priority alerts can trigger automated phone calls:

1. The system calls the designated security number
2. A text-to-speech message describes the alert
3. The recipient can acknowledge or escalate

### Creating Custom SOPs

For effective incident response, create Standard Operating Procedures that define:

1. **Alert Categorization**:
   - Priority levels based on alert type and location
   - Required response times

2. **Verification Steps**:
   - How to confirm real incidents
   - Additional cameras or systems to check

3. **Response Actions**:
   - Who to notify for different alerts
   - When to dispatch security personnel
   - When to contact law enforcement
   - Communication protocols

4. **Documentation**:
   - How to record incidents
   - Required follow-up information
   - Reporting procedures

## Best Practices

### Deployment Strategy

- Start with a few critical cameras before expanding
- Use different sensitivity settings for different areas
- Test thoroughly before relying on the system
- Create clear response procedures for all alert types

### Maintenance

- Regularly review and optimize settings based on performance
- Clean camera lenses and adjust positioning as needed
- Update plugin when new versions are available
- Back up configuration settings periodically

### Operator Training

- Ensure all operators understand how to interpret alerts
- Provide clear procedures for different types of anomalies
- Encourage feedback on system performance
- Conduct periodic drills to test response procedures

### Continuous Improvement

- Review false alarm patterns monthly
- Adjust sensitivity based on seasonal changes
- Update business hours when operations change
- Refine response procedures based on real incidents

## FAQ

### How long does the learning phase take?

The initial learning phase typically takes 8-12 hours of operation to collect sufficient data. For environments with complex patterns, administrators may extend this period.

### What happens if the camera view changes?

Significant changes to the camera view (repositioning, field of view changes) will require retraining. You can trigger this by:
1. Going to camera settings
2. Disabling and re-enabling the plugin, or
3. Clicking the "Reset Learning" button if available

### Why am I getting false alarms?

False alarms can occur due to:
- Environmental factors (lighting changes, shadows, reflections)
- Regular but infrequent activities the system hasn't learned
- Sensitivity settings that are too aggressive
- Improper region of interest configuration

### Can the system detect specific objects or actions?

The base plugin focuses on general anomaly detection and unknown visitor identification. It can detect people and vehicles, but doesn't recognize specific actions like fighting or running by default. These may be available as add-on modules.

### What happens during power or network outages?

The plugin automatically saves its learned models periodically. After an outage, it will:
1. Reload the last saved model
2. Continue monitoring using that baseline
3. Resume learning if continuous learning is enabled

### How many cameras can use the plugin simultaneously?

This depends on your server hardware. As a general guideline:
- Entry-level server: 4-8 cameras
- Mid-range server: 8-16 cameras
- High-performance server: 16+ cameras

### Can I adjust sensitivity for different times of day?

Yes, using multiple event rules with different schedules. For example:
1. Create a "Business Hours" rule with standard sensitivity
2. Create an "After Hours" rule with increased sensitivity
3. Schedule each rule appropriately

### How do I completely disable alerts temporarily?

To temporarily disable alerts without losing learned patterns:
1. Go to the Event Rules in Nx Meta
2. Locate the rules associated with NX Agent alerts
3. Disable these rules temporarily
4. Re-enable when monitoring should resume
