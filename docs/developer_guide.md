# NX Agent Developer Guide

This guide is intended for developers who want to extend, customize, or integrate with the NX Agent AI Security Guard plugin. It covers the technical architecture, API details, and development workflows.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Core Components](#core-components)
3. [Development Environment Setup](#development-environment-setup)
4. [Building and Debugging](#building-and-debugging)
5. [Extending the Plugin](#extending-the-plugin)
   - [Adding New Anomaly Types](#adding-new-anomaly-types)
   - [Integrating Custom ML Models](#integrating-custom-ml-models)
   - [Creating Custom Response Actions](#creating-custom-response-actions)
6. [Integration Points](#integration-points)
   - [Nx Meta API Integration](#nx-meta-api-integration)
   - [External System Integration](#external-system-integration)
7. [Performance Optimization](#performance-optimization)
8. [Testing and Quality Assurance](#testing-and-quality-assurance)
9. [Contributing Guidelines](#contributing-guidelines)

## Architecture Overview

The NX Agent plugin follows a modular architecture to enable extensibility while maintaining high performance. The key design principles include:

- **Separation of concerns**: Each component has a well-defined responsibility
- **Minimal dependencies**: Core components function independently
- **Efficient resource usage**: Optimized for real-time video processing
- **Thread safety**: Designed for concurrent processing of multiple video streams

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Nx Meta VMS Server                    │
└───────────────────────────┬─────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                   NX Agent Plugin                       │
│                                                         │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐  │
│  │ Metadata    │    │ Anomaly     │    │ Response    │  │
│  │ Analyzer    │───▶│ Detector    │───▶│ Protocol    │  │
│  └─────────────┘    └─────────────┘    └─────────────┘  │
│         │                  │                  │          │
│         ▼                  ▼                  ▼          │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐  │
│  │ Object      │    │ Learning    │    │ External    │  │
│  │ Detection   │    │ Models      │    │ Integrations│  │
│  └─────────────┘    └─────────────┘    └─────────────┘  │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

The plugin processes video frames through several stages:
1. Frame acquisition from Nx Meta
2. Metadata extraction and object detection
3. Anomaly detection based on learned patterns
4. Response execution for detected anomalies
5. Event generation back to Nx Meta

## Core Components

### NxAgentPlugin

The entry point for the plugin, responsible for initialization and creating the engine instance.

**Key Files:**
- `nx_agent_plugin.h/cpp`: Main plugin class definitions and implementation

### MetadataAnalyzer

Processes video frames to extract meaningful metadata including objects, motion patterns, and scene information.

**Key Files:**
- `nx_agent_metadata.h/cpp`: Metadata analysis implementation

### AnomalyDetector

Analyzes metadata to detect anomalies by comparing against learned normal patterns.

**Key Files:**
- `nx_agent_anomaly.h/cpp`: Anomaly detection implementation

### ResponseProtocol

Manages verification and response to detected anomalies.

**Key Files:**
- `nx_agent_response.h/cpp`: Response protocol implementation

### Configuration System

Manages plugin settings and camera-specific configurations.

**Key Files:**
- `nx_agent_config.h/cpp`: Configuration system implementation

### Utility Functions

Common utility functions and helpers used throughout the plugin.

**Key Files:**
- `nx_agent_utils.h/cpp`: Utility functions implementation

## Development Environment Setup

### Prerequisites

- C++17 compatible compiler (GCC 7+, MSVC 2019+, Clang 6+)
- CMake 3.15 or higher
- Nx Meta SDK (available from Network Optix Developer Portal)
- OpenCV 4.2 or higher
- nlohmann-json library
- libcurl development files
- Git version control

### Linux Setup

```bash
# Install development tools
sudo apt update
sudo apt install -y build-essential cmake git

# Install dependencies
sudo apt install -y libopencv-dev libcurl4-openssl-dev nlohmann-json3-dev

# Clone repository
git clone https://github.com/nx-agent/nx-agent-plugin.git
cd nx-agent-plugin

# Create build directory
mkdir build && cd build

# Configure and build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Windows Setup

1. Install Visual Studio 2019 or later with C++ development tools

2. Install CMake 3.15 or higher

3. Install vcpkg package manager:
   ```cmd
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   bootstrap-vcpkg.bat
   vcpkg integrate install
   ```

4. Install dependencies:
   ```cmd
   vcpkg install opencv:x64-windows nlohmann-json:x64-windows curl:x64-windows
   ```

5. Clone repository:
   ```cmd
   git clone https://github.com/nx-agent/nx-agent-plugin.git
   cd nx-agent-plugin
   ```

6. Configure and build with Visual Studio:
   ```cmd
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=[path\to\vcpkg]\scripts\buildsystems\vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug
   cmake --build . --config Debug
   ```

## Building and Debugging

### Building for Development

To build the plugin with debugging symbols and additional logging:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DNX_AGENT_ENABLE_LOGGING=ON ..
make
```

### Debugging with Nx Meta Server

1. Start Nx Meta Server with debugging enabled:

   **Linux:**
   ```bash
   sudo systemctl stop networkoptix-metavms
   sudo NX_METADATA_SDK_DEBUG=1 /opt/networkoptix-metavms/mediaserver/bin/nx_server
   ```

   **Windows:**
   ```cmd
   net stop "Nx Meta Server"
   set NX_METADATA_SDK_DEBUG=1
   "C:\Program Files\Network Optix\Nx Meta\bin\nx_server.exe"
   ```

2. Connect your debugger to the running process.

3. Set breakpoints in your plugin code.

### Using the Test Framework

The plugin includes a standalone test framework to verify functionality without running the full Nx Meta Server:

```bash
cd build
make nx_agent_test
./tests/nx_agent_test
```

## Extending the Plugin

### Adding New Anomaly Types

To add a new type of anomaly detection:

1. Define the new anomaly type in `nx_agent_anomaly.h`:
   ```cpp
   // Example: Add Crowd Detection
   class CrowdDetector {
   public:
       bool detectCrowdAnomaly(const FrameAnalysisResult& result);
       // ...
   };
   ```

2. Implement the detection logic in `nx_agent_anomaly.cpp`.

3. Integrate with the main `AnomalyDetector` class:
   ```cpp
   // In AnomalyDetector::detectAnomaly
   bool crowdAnomaly = m_crowdDetector.detectCrowdAnomaly(result);
   if (crowdAnomaly) {
       result.anomalyType = "CrowdAnomaly";
       result.anomalyDescription = "Unusual crowd formation detected";
       result.isAnomaly = true;
   }
   ```

4. Update the plugin manifest to include the new event type in `NxAgentDeviceAgent::manifestString()`.

### Integrating Custom ML Models

To integrate a custom machine learning model:

1. Create a new model wrapper class:
   ```cpp
   class CustomMLModel {
   public:
       CustomMLModel();
       ~CustomMLModel();
       
       bool initialize(const std::string& modelPath);
       float predict(const cv::Mat& frame);
       
   private:
       // Model-specific members
   };
   ```

2. Implement model loading and inference in your wrapper class.

3. Integrate with the `MetadataAnalyzer` or `AnomalyDetector` as appropriate:
   ```cpp
   // In MetadataAnalyzer
   m_customModel = std::make_unique<CustomMLModel>();
   m_customModel->initialize("/path/to/model.pb");
   
   // In processing pipeline
   float prediction = m_customModel->predict(frame);
   result.anomalyScore = std::max(result.anomalyScore, prediction);
   ```

### Creating Custom Response Actions

To add a new response action type:

1. Add the new action type to the `ResponseAction::Type` enum in `nx_agent_response.h`:
   ```cpp
   enum class Type {
       LOG_ONLY,
       NX_EVENT,
       HTTP_REQUEST,
       SIP_CALL,
       EXECUTE_COMMAND,
       CUSTOM_ACTION   // New action type
   };
   ```

2. Implement the action handler in the `ResponseProtocol::executeAction` method:
   ```cpp
   bool ResponseProtocol::executeAction(const ResponseAction& action, 
                                       const FrameAnalysisResult& result) {
       // Existing code...
       
       case ResponseAction::Type::CUSTOM_ACTION:
           return executeCustomAction(action, result);
           
       // Rest of the method...
   }
   
   bool ResponseProtocol::executeCustomAction(const ResponseAction& action,
                                            const FrameAnalysisResult& result) {
       // Custom action implementation
       return true;
   }
   ```

## Integration Points

### Nx Meta API Integration

The plugin integrates with Nx Meta through the Metadata SDK. Key integration points:

1. **Video Frame Processing**:
   - `NxAgentDeviceAgent::processVideoFrame`: Processes video frames from Nx Meta

2. **Event Generation**:
   - `generateAnomalyEvent`: Creates and pushes events back to Nx Meta

3. **Object Metadata**:
   - `reportObjects`: Sends detected objects back to Nx Meta for display and search

4. **Settings Management**:
   - `doSetupAnalytics`: Processes settings from Nx Meta

### External System Integration

The plugin can integrate with external systems through:

1. **HTTP Webhooks**:
   - `ResponseProtocol::sendHttpRequest`: Sends HTTP requests to external systems

2. **SIP/VoIP**:
   - `ResponseProtocol::makeSipCall`: Initiates SIP calls for notifications

3. **Command Execution**:
   - `ResponseProtocol::executeCommand`: Runs local commands or scripts

4. **Custom Integration**:
   - Extend the `ResponseProtocol` class to add custom integration methods

## Performance Optimization

### Frame Processing Optimization

1. **Selective Processing**:
   - Process only every Nth frame to reduce CPU load
   - Skip processing when no motion is detected

2. **Region of Interest**:
   - Only analyze relevant parts of the frame
   - Exclude areas that frequently cause false positives

3. **Resolution Scaling**:
   - Scale down frames for analysis when full resolution isn't needed
   - Use different resolutions for different analysis stages

### Memory Management

1. **Buffer Management**:
   - Reuse buffers for frame processing
   - Limit the size of historical data kept in memory

2. **Model Optimization**:
   - Use quantized models when possible
   - Consider model pruning techniques

3. **Resource Cleanup**:
   - Ensure proper cleanup of OpenCV and other resources
   - Monitor memory usage with tools like Valgrind or Visual Studio Memory Profiler

### Multi-threading

1. **Thread Safety**:
   - Use appropriate synchronization for shared resources
   - Prefer thread-local storage when possible

2. **Parallel Processing**:
   - Process multiple cameras in separate threads
   - Divide analysis pipeline into parallel stages when appropriate

## Testing and Quality Assurance

### Unit Testing

The plugin includes unit tests for core components:

```bash
# Build and run tests
cd build
make nx_agent_unit_tests
./tests/nx_agent_unit_tests
```

Key test files:
- `tests/metadata_analyzer_tests.cpp`: Tests for metadata analysis
- `tests/anomaly_detector_tests.cpp`: Tests for anomaly detection
- `tests/response_protocol_tests.cpp`: Tests for response protocols

### Integration Testing

The `nx_agent_test` utility provides integration testing capabilities:

```bash
# Build and run integration test
cd build
make nx_agent_test
./tests/nx_agent_test
```

This tool simulates the Nx Meta environment and tests the entire plugin pipeline.

### Performance Testing

Performance testing scripts are available in the `tests/performance` directory:

```bash
# Run performance benchmark
cd build
make nx_agent_benchmark
./tests/nx_agent_benchmark
```

This benchmark measures:
- Frame processing time
- Memory usage
- CPU utilization
- Detection accuracy

## Contributing Guidelines

### Code Style

The project follows the Google C++ Style Guide with some modifications:

- Use 4-space indentation
- Use `m_` prefix for member variables
- Use `snake_case` for file names and `CamelCase` for class names
- Include detailed comments for public methods

### Submission Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/your-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin feature/your-feature`)
5. Create a new Pull Request

### Code Review Process

All submissions require code review. We use GitHub pull requests for this purpose. Consult GitHub Help for more information on using pull requests.

### Documentation Requirements

For new features or significant changes:

1. Update relevant header files with detailed comments
2. Add or update usage examples
3. Document any new configuration options
4. Update the README.md if necessary
5. Add appropriate unit and integration tests
