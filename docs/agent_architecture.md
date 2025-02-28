# NX Agent Enhanced Architecture

## Overview

The NX Agent plugin has been enhanced with advanced AI capabilities, transforming it from a simple anomaly detection system into a comprehensive AI-powered security agent. This document outlines the architecture of the enhanced system, explaining how the components work together to provide intelligent security monitoring.

## System Components

The enhanced NX Agent system consists of the following major components:

```
┌─────────────────────────────────────────────────────────────────┐
│                       NX Agent System                           │
│                                                                 │
│  ┌───────────────┐   ┌───────────────┐   ┌───────────────────┐  │
│  │ Metadata      │   │ Anomaly       │   │ Response          │  │
│  │ Analyzer      │   │ Detector      │   │ Protocol          │  │
│  └───────┬───────┘   └───────┬───────┘   └─────────┬─────────┘  │
│          │                   │                     │            │
│          ▼                   ▼                     ▼            │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    NX Agent System                         │  │
│  │                                                            │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────────────┐   │  │
│  │  │ LLM        │  │ Reasoning  │  │ Strategy           │   │  │
│  │  │ Manager    │  │ System     │  │ Manager            │   │  │
│  │  └────────────┘  └────────────┘  └────────────────────┘   │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Core Components

1. **NxAgentPlugin**: The main entry point for the Nx Meta VMS system. It creates the engine and manages the plugin lifecycle.

2. **NxAgentEngine**: Responsible for creating device agents for each camera and managing global plugin data.

3. **NxAgentDeviceAgent**: Processes video frames from a single camera, coordinates the analysis components, and reports results back to the VMS.

### Processing Components

4. **MetadataAnalyzer**: Extracts metadata from video frames, including object detection and tracking.

5. **AnomalyDetector**: Identifies unusual patterns and behaviors based on learned baselines.

6. **ResponseProtocol**: Manages responses to detected anomalies, including generating events and alerts.

### AI Components

7. **NxAgentSystem**: The central coordinator for all AI capabilities, integrating the following components:

   - **LLMManager**: Manages communication with Large Language Models for reasoning and analysis.

   - **ReasoningSystem**: Provides advanced reasoning capabilities, including:
     - Knowledge management
     - Goal-oriented planning
     - Action selection and execution

   - **StrategyManager**: Handles strategic planning and subject tracking:
     - Cross-camera correlation
     - Subject tracking and prediction
     - Security incident management

## Data Flow

1. Video frames are received by the `NxAgentDeviceAgent` from the VMS.

2. The frames are processed by the `MetadataAnalyzer` to extract objects and metadata.

3. The `AnomalyDetector` analyzes the metadata to identify potential anomalies.

4. The `NxAgentSystem` receives the processed data and performs advanced analysis:
   - The `ReasoningSystem` evaluates the situation using the LLM
   - The `StrategyManager` tracks subjects across cameras and maintains situational awareness
   - The `LLMManager` provides reasoning capabilities through API calls to external LLM services

5. When anomalies or security incidents are detected, the `ResponseProtocol` generates appropriate events and alerts.

6. Results are reported back to the VMS through metadata packets.

## Configuration System

The system uses a hierarchical configuration approach:

1. **GlobalConfig**: Manages global settings for the entire plugin, including:
   - LLM API settings (API key, model, endpoint)
   - Storage and diagnostic settings
   - SIP/notification settings

2. **DeviceConfig**: Contains settings specific to each camera, including:
   - Detection thresholds and regions
   - Learning parameters
   - AI reasoning settings
   - Business hours and scheduling

## AI Reasoning Process

The enhanced system uses a cognitive cycle for reasoning:

1. **Perception**: Process sensory input from cameras
2. **Cognition**: Update understanding of the environment
3. **Action Selection**: Choose appropriate actions based on goals
4. **Execution**: Carry out selected actions
5. **Reflection**: Evaluate performance and adapt strategies

This cycle runs continuously, allowing the system to maintain situational awareness and respond intelligently to changing conditions.

## Cross-Camera Correlation

One of the key enhancements is the ability to correlate information across multiple cameras:

1. The `StrategyManager` maintains a global view of all monitored areas
2. Subjects are tracked as they move between camera views
3. The system can predict where subjects might appear next
4. Suspicious behavior is analyzed in the context of the entire monitored area

## Integration with VMS

The plugin integrates with the Nx Meta VMS through:

1. Object metadata for detected entities
2. Event metadata for anomalies and incidents
3. Custom attributes for enhanced information
4. Status events for system monitoring

## Conclusion

The enhanced NX Agent architecture transforms a basic video analytics plugin into a comprehensive AI-powered security system. By integrating LLM capabilities with traditional computer vision, the system provides intelligent monitoring, reasoning, and response capabilities that go far beyond simple anomaly detection.
