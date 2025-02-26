# NX Agent Supervisor

A powerful AI-powered security monitoring system for Network Optix Nx Meta VMS. The plugin combines advanced video analytics with Large Language Model (LLM) capabilities to provide intelligent security monitoring, anomaly detection, and automated response planning.

## Features

### Core Analytics
- **Real-time Metadata Analysis**: Analyze video metadata from camera feeds to detect objects, motion patterns, and activities.
- **Anomaly Detection**: Automatically learn normal patterns and detect deviations using statistical modeling and machine learning.
- **Persistent Unknown Visitor Detection**: Identify and track unknown individuals who remain in a scene for extended periods.
- **Suspicious Activity Recognition**: Detect unusual motion patterns, loitering in restricted areas, and other abnormal behaviors.

### Advanced AI Capabilities
- **LLM-Powered Reasoning**: Integration with Large Language Models for intelligent situation analysis and decision support.
- **Cross-Camera Intelligence**: Track subjects across multiple cameras and correlate information for comprehensive situational awareness.
- **Strategic Planning**: AI-driven security recommendations and response planning for detected threats.
- **Cognitive Status Reports**: Regular insights into the system's understanding of the security situation.

### System Integration
- **VMS UI Integration**: Seamlessly integrate with Nx Meta's interface to display alerts and camera feeds when anomalies are detected.
- **Automated Response Protocols**: Execute site-specific Standard Operating Procedures (SOPs) when threats are detected.
- **Continuous Learning**: Adapt to the environment over time by learning normal behaviors and refining anomaly detection.
- **Custom Goals**: Set and track specific security objectives through the AI system.

## System Requirements

- Network Optix Nx Meta VMS 5.0 or higher
- 64-bit Linux (Ubuntu 18.04/20.04 LTS recommended) or Windows 10/11/Server
- 8GB RAM minimum (16GB+ recommended for LLM integration)
- x86-64 CPU with SSE4.2 support
- OpenCV 4.2+ and its dependencies
- CURL library for LLM API communication
- 100MB free disk space for plugin, plus storage for models and configuration
- Internet connection for LLM API access (if enabled)

## Installation

### Pre-built Binaries

1. Download the latest release package for your operating system from the [Releases](https://github.com/nx-agent/nx-agent-plugin/releases) page.
2. Extract the package contents to a temporary location.
3. Copy the plugin library (`nx_agent_plugin.so` for Linux or `nx_agent_plugin.dll` for Windows) to the Nx Meta plugins directory:
   - Linux: `/opt/networkoptix-metavms/mediaserver/bin/plugins/`
   - Windows: `C:\Program Files\Network Optix\Nx Meta\bin\plugins\`
4. Restart the Nx Meta Server service:
   - Linux: `sudo systemctl restart networkoptix-metavms`
   - Windows: Use Services management console to restart "Nx Meta Server"

### Building from Source

#### Linux

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake libopencv-dev nlohmann-json3-dev libcurl4-openssl-dev

# Clone repository
git clone https://github.com/nx-agent/nx-agent-plugin.git
cd nx-agent-plugin

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make -j4

# Install
sudo make install
```

#### Windows

1. Install Visual Studio 2019 or later with C++ development tools
2. Install CMake 3.15 or higher
3. Install OpenCV 4.2+ using vcpkg or pre-built binaries
4. Install nlohmann-json and CURL using vcpkg
5. Open the project in Visual Studio and build the solution
6. Copy the built DLL to the Nx Meta plugins directory

## Configuration

### Enabling the Plugin

1. Open Nx Meta Desktop Client and log in as an administrator
2. Go to **Server Settings** > **Plugins**
3. Find "NX Agent AI Security Guard" in the list and click the checkbox to enable it
4. Click **Apply** to save changes

### LLM Integration Setup

1. Go to **Server Settings** > **Plugins** > **NX Agent** > **AI Settings**
2. Configure LLM settings:
   - Enable/disable LLM integration
   - Set API key for chosen LLM service (Claude or GPT)
   - Configure model parameters and reasoning intervals
   - Set up cross-camera correlation options

### Camera Configuration

1. Select a camera in the Resource Tree
2. Go to the **Camera Settings** tab
3. Open the **Analytics** section
4. Check "NX Agent AI Security Guard" to enable the plugin for this camera
5. Configure the plugin settings:
   - **Sensitivity Level**: Adjust how sensitive the system is to anomalies (0.0-1.0)
   - **Learning Mode**: Enable/disable automatic learning of normal patterns
   - **Detection Regions**: Draw regions of interest to focus monitoring on specific areas
   - **Business Hours**: Set normal operational hours for your site
   - **Unknown Visitor Threshold**: Time in seconds before an unknown visitor is considered suspicious
   - **AI Reasoning Settings**: Configure reasoning intervals and confidence thresholds

### Response Configuration

1. Go to **Event Rules** in Nx Meta Desktop Client
2. Create a new rule for the events generated by the plugin:
   - **Event**: Select "Analytics Event" > "NX Agent AI Security Guard" > choose event type
   - **Action**: Configure desired response (e.g., "Show on Alarm Layout", "Send Email", etc.)
   - **Schedule**: Set when this rule should be active

## Usage

### Learning Phase

When first enabled on a camera, the plugin enters a learning phase to establish normal patterns. During this time:

- The plugin collects data about typical activity patterns
- No anomaly alerts will be generated
- A status event "Learning in Progress" will be shown

The learning phase typically lasts 8-12 hours but can be extended for more complex environments. Once complete, the plugin automatically switches to detection mode.

### AI Reasoning System

The enhanced system provides:

- Intelligent analysis of detected anomalies
- Context-aware security recommendations
- Predictive insights about potential threats
- Cross-camera subject tracking and correlation
- Regular cognitive status reports

### Events and Alerts

The plugin generates several types of events:

- **Anomaly Detected**: General unusual activity that deviates from normal patterns
- **Unknown Visitor**: An unrecognized person lingering in the scene
- **Abnormal Activity**: Specific suspicious activities like loitering in restricted areas
- **AI Insights**: System reasoning and recommendations
- **Status Events**: Plugin status updates like "Learning Complete"

### Viewing Alerts

When an anomaly is detected:

1. The camera will appear in the Alarm Layout (if configured in Event Rules)
2. The detected object will be highlighted with a bounding box
3. Event details will be available in the Nx Meta event log
4. AI reasoning and recommendations will be displayed
5. Automated responses will be triggered according to your rules

## Documentation

Detailed documentation is available in the `docs` directory:

- [Agent Architecture](docs/agent_architecture.md): System design and component interaction
- [LLM Integration Guide](docs/llm_integration.md): Setting up and configuring LLM capabilities
- [Operator Manual](docs/operator_manual.md): Comprehensive usage instructions
- [Installation Guide](docs/installation_guide.md): Detailed setup instructions
- [Developer Guide](docs/developer_guide.md): Information for developers

## Support and Feedback

For support, bug reports, or feature requests, please:

- Create an issue on our [GitHub repository](https://github.com/nx-agent/nx-agent-plugin/issues)
- Contact us at support@nx-agent.com

## License

This plugin is licensed under the [MIT License](LICENSE).
