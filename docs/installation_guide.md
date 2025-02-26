# NX Agent Installation and Configuration Guide

This document provides detailed instructions for installing, configuring, and troubleshooting the NX Agent AI Security Guard plugin for Network Optix Nx Meta VMS.

## Table of Contents

1. [System Requirements](#system-requirements)
2. [Installation](#installation)
   - [Pre-built Binaries](#pre-built-binaries)
   - [Building from Source](#building-from-source)
3. [Plugin Configuration](#plugin-configuration)
   - [Server Settings](#server-settings)
   - [Camera Configuration](#camera-configuration)
   - [Response Rules](#response-rules)
4. [Advanced Configuration](#advanced-configuration)
   - [Plugin Data Storage](#plugin-data-storage)
   - [SIP Integration](#sip-integration)
   - [HTTP Webhook Integration](#http-webhook-integration)
5. [Troubleshooting](#troubleshooting)
   - [Common Issues](#common-issues)
   - [Logs and Diagnostics](#logs-and-diagnostics)
6. [Upgrading](#upgrading)

## System Requirements

### Minimum Requirements

- Network Optix Nx Meta VMS 5.0 or higher
- 64-bit OS: Ubuntu 18.04/20.04 LTS, Windows 10/11/Server 2016+
- 4GB RAM
- x86-64 CPU with SSE4.2 support
- 50MB free disk space for plugin
- 1GB+ additional storage for models and configuration

### Recommended Configuration

- 8GB+ RAM
- 4+ CPU cores
- SSD storage
- Dedicated GPU (not required but improves performance)
- Nx Meta VMS 5.1 or higher

### Software Dependencies

- OpenCV 4.2 or higher
- libcurl 7.58 or higher
- nlohmann-json 3.9 or higher

## Installation

### Pre-built Binaries

#### Linux Installation

1. Download the latest Linux package from our [releases page](https://github.com/nx-agent/nx-agent-plugin/releases).

2. Extract the package:
   ```bash
   tar -xzf nx-agent-plugin-linux-x64.tar.gz
   ```

3. Run the installation script with administrative privileges:
   ```bash
   cd nx-agent-plugin
   sudo ./install.sh
   ```

4. Verify installation:
   ```bash
   ls -la /opt/networkoptix-metavms/mediaserver/bin/plugins/nx_agent_plugin.so
   ```

5. Restart the Nx Meta Server:
   ```bash
   sudo systemctl restart networkoptix-metavms
   ```

#### Windows Installation

1. Download the latest Windows package from our [releases page](https://github.com/nx-agent/nx-agent-plugin/releases).

2. Extract the ZIP file to a temporary location.

3. Run the included `install.bat` script as Administrator.

4. Alternatively, manually copy the plugin files:
   - Copy `nx_agent_plugin.dll` to `C:\Program Files\Network Optix\Nx Meta\bin\plugins\`
   - Copy support DLLs (if any) to `C:\Program Files\Network Optix\Nx Meta\bin\`

5. Restart the Nx Meta Server service using Windows Services management console.

### Building from Source

#### Linux Build

1. Install development dependencies:
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential cmake git
   sudo apt-get install -y libopencv-dev libcurl4-openssl-dev nlohmann-json3-dev
   ```

2. Clone the repository:
   ```bash
   git clone https://github.com/nx-agent/nx-agent-plugin.git
   cd nx-agent-plugin
   ```

3. Download and extract the Nx Meta SDK:
   ```bash
   mkdir -p nx_sdk
   wget https://nexus.networkoptix.com/sdk-release/nx_sdk_latest.tar.gz
   tar -xzf nx_sdk_latest.tar.gz -C nx_sdk
   ```

4. Build the plugin:
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```

5. Install:
   ```bash
   sudo make install
   ```

#### Windows Build

1. Install [Visual Studio 2019](https://visualstudio.microsoft.com/vs/) or later with C++ development tools.

2. Install [CMake](https://cmake.org/download/) (3.15 or higher).

3. Install [vcpkg](https://github.com/microsoft/vcpkg) for managing dependencies:
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

5. Download the Nx Meta SDK from the Network Optix developer portal.

6. Clone the repository:
   ```cmd
   git clone https://github.com/nx-agent/nx-agent-plugin.git
   cd nx-agent-plugin
   ```

7. Configure and build:
   ```cmd
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=[path\to\vcpkg]\scripts\buildsystems\vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
   ```

8. Install manually by copying the built `nx_agent_plugin.dll` to the Nx Meta plugins directory.

## Plugin Configuration

### Server Settings

1. Open Nx Meta Desktop Client and log in with administrator credentials.

2. Navigate to **Server Settings** > **Plugins**.

3. Find "NX Agent AI Security Guard" in the list of available plugins.

4. Click the checkbox to enable the plugin globally.

5. Configure global settings:
   - **Data Storage Path**: Location to store plugin data (models, configurations)
   - **Max Storage Size (MB)**: Maximum disk space to use
   - **Diagnostic Log Level**: Detail level for logs (0=off, 1=error, 2=warn, 3=info, 4=debug)
   - **SIP Integration**: Enable/disable SIP call notifications
   - **SIP Server**: SIP server address if using SIP integration
   - **SIP Username/Password**: Credentials for SIP server
   - **Alarm Phone Number**: Number to call for high-priority alerts

6. Click **Apply** to save changes.

### Camera Configuration

1. In Nx Meta Desktop Client, select a camera in the Resource Tree.

2. Go to the **Camera Settings** tab.

3. Find and expand the **Analytics** section.

4. Check "NX Agent AI Security Guard" to enable the plugin for this camera.

5. Configure camera-specific settings:
   - **Detection Regions**: Click "Edit" to draw regions of interest
   - **Minimum Person Confidence**: Threshold for person detection (0.0-1.0)
   - **Anomaly Threshold**: Sensitivity for anomaly detection (higher = more sensitive)
   - **Enable Unknown Visitor Detection**: Toggle detection of lingering unknown persons
   - **Unknown Visitor Threshold (seconds)**: Time before visitor is considered suspicious
   - **Enable Learning**: Toggle continuous learning and adaptation
   - **Business Hours Start/End**: Define normal operational hours

6. Click **Apply** to save camera settings.

### Response Rules

1. Navigate to **Event Rules** in Nx Meta Desktop Client.

2. Click **Add** to create a new rule.

3. Configure the event source:
   - Select **Analytics Event** as the event type
   - Choose "NX Agent AI Security Guard" as the source
   - Select an event type:
     - **Anomaly Detected**: General unusual activity
     - **Unknown Visitor**: Unrecognized person lingering
     - **Abnormal Activity**: Specific suspicious activity
     - **Status Event**: Plugin status updates

4. Configure actions to take when the event occurs:
   - **Show on Alarm Layout**: Display camera feed in alarm view
   - **Play Sound**: Alert operators with audio
   - **Send Email**: Notify security personnel
   - **Execute HTTP Request**: Integrate with external systems
   - **Start Recording**: Ensure the event is recorded

5. Set the rule schedule (when it should be active).

6. Name the rule and click **OK** to save.

## Advanced Configuration

### Plugin Data Storage

The plugin stores data in the following locations:

- **Linux**: `/var/lib/nx-agent/` by default
- **Windows**: `C:\ProgramData\NxAgent\` by default

This directory contains:
- Model files (saved anomaly detection models)
- Configuration data for each camera
- Diagnostic logs

You can change this location in the server settings, but ensure the new location is:
- Accessible by the Nx Meta Server process
- Has sufficient free space
- Is backed up regularly to prevent data loss

### SIP Integration

For SIP-based phone call notifications:

1. Ensure you have a SIP service provider account or local SIP server.

2. Configure SIP settings in the plugin's global configuration:
   - **SIP Server**: Your SIP server address (e.g., `sip.provider.com`)
   - **SIP Username**: Your SIP account username
   - **SIP Password**: Your SIP account password
   - **Alarm Phone Number**: Number to call when critical anomalies are detected

3. Create or modify event rules to use SIP notifications for high-priority events:
   - Event type: "Unknown Visitor" or critical anomalies
   - Use highest priority actions (e.g., phone calls) for the most serious situations

### HTTP Webhook Integration

To integrate with external systems via HTTP:

1. Create a webhook endpoint in your external system that can receive JSON data.

2. In Nx Meta, create an event rule:
   - Event source: Choose the appropriate NX Agent event type
   - Action: Select "Execute HTTP Request"
   - URL: Enter your webhook endpoint URL
   - HTTP Method: Usually POST
   - Content Type: application/json
   - Body: You can customize the payload, or use the default which includes event details

3. Test the integration by triggering a test event.

## Troubleshooting

### Common Issues

#### Plugin Not Appearing in Nx Meta

- Verify the plugin file is in the correct plugins directory
- Check Nx Meta Server service is running
- Ensure plugin file has correct permissions
- Check Nx Meta Server logs for loading errors

#### Plugin Loads But Not Working

- Verify plugin is enabled both globally and for the specific camera
- Check camera is streaming properly
- Ensure camera's video content is appropriate for analysis (good lighting, proper focus)
- Check diagnostic logs for errors

#### High CPU Usage

- Reduce the number of cameras using the plugin
- Adjust processing interval (frames per second)
- Limit analysis to smaller regions of interest
- Enable hardware acceleration if available

#### Learning Mode Persists Too Long

- Manually disable learning mode in camera settings
- Ensure camera is providing diverse but representative footage during learning
- Check logs for learning progress messages

### Logs and Diagnostics

#### Accessing Logs

- **Linux**: Logs are stored in `/var/log/nx-agent/` and `/var/log/networkoptix-metavms/`
- **Windows**: Logs are in `C:\ProgramData\NxAgent\logs\` and Nx Meta log directory

#### Debug Mode

To enable verbose logging:

1. Set "Diagnostic Log Level" to 4 (debug) in plugin settings
2. Restart the plugin or Nx Meta Server
3. Reproduce the issue
4. Check logs for detailed information

#### Health Check

The plugin generates status events that appear in Nx Meta's event log. Look for:
- "Learning Complete" events
- "NX Agent Status" events
- Any error messages or warnings

## Upgrading

### Standard Upgrade Process

1. Stop the Nx Meta Server service.
2. Back up your existing plugin data directory.
3. Remove the old plugin file from the plugins directory.
4. Copy the new plugin file to the plugins directory.
5. Restart the Nx Meta Server service.
6. Verify the plugin is working correctly in the Nx Meta Desktop Client.

### Version-Specific Notes

When upgrading between major versions, check the release notes for:
- Configuration format changes
- Model compatibility issues
- New features that require additional configuration

The plugin will attempt to migrate existing settings and models, but a backup is recommended before any upgrade.
