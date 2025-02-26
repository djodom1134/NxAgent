# LLM Integration Guide

## Overview

The NX Agent plugin now includes integration with Large Language Models (LLMs) to provide advanced reasoning, planning, and analysis capabilities. This document explains how to set up and configure the LLM integration for optimal performance.

## Supported LLM Services

The current implementation supports the following LLM services:

- **Anthropic Claude** (recommended)
  - Models: claude-3-haiku, claude-3-sonnet, claude-3-opus
  - API Endpoint: https://api.anthropic.com/v1/messages

- **OpenAI GPT**
  - Models: gpt-4, gpt-4-turbo, gpt-3.5-turbo
  - API Endpoint: https://api.openai.com/v1/chat/completions

## Configuration

### Global Settings

LLM integration is configured through the global settings in the NX Meta VMS interface. The following settings are available:

| Setting | Description | Default |
|---------|-------------|---------|
| `enableLLMIntegration` | Enable/disable LLM integration | `true` |
| `llmApiKey` | API key for the LLM service | `""` |
| `llmModelName` | Name of the LLM model to use | `"claude-3-haiku-20240307"` |
| `llmApiEndpoint` | API endpoint URL | `"https://api.anthropic.com/v1/messages"` |
| `llmMaxTokens` | Maximum tokens for LLM responses | `4096` |
| `llmTemperature` | Temperature setting for LLM (0.0-1.0) | `0.7` |
| `llmRequestTimeoutSecs` | Timeout for LLM requests in seconds | `30` |

### Device-Specific Settings

Each camera can have its own AI reasoning settings:

| Setting | Description | Default |
|---------|-------------|---------|
| `enableAIReasoning` | Enable/disable AI reasoning for this camera | `true` |
| `reasoningConfidenceThreshold` | Confidence threshold for AI reasoning (0.0-1.0) | `0.65` |
| `reasoningInterval` | Interval between reasoning cycles in seconds | `60` |
| `enableCrossCameraAnalysis` | Enable correlation across multiple cameras | `true` |

## Obtaining API Keys

### Anthropic Claude

1. Go to [Anthropic Console](https://console.anthropic.com/)
2. Create an account or log in
3. Navigate to the API Keys section
4. Create a new API key
5. Copy the key and paste it into the `llmApiKey` setting

### OpenAI

1. Go to [OpenAI Platform](https://platform.openai.com/)
2. Create an account or log in
3. Navigate to the API Keys section
4. Create a new API key
5. Copy the key and paste it into the `llmApiKey` setting
6. Change the `llmApiEndpoint` to `https://api.openai.com/v1/chat/completions`
7. Change the `llmModelName` to your preferred OpenAI model (e.g., `gpt-4`)

## Network Requirements

The NX Agent plugin requires outbound internet access to communicate with the LLM API services. Ensure that your firewall allows outbound connections to the following domains:

- `api.anthropic.com` (for Anthropic Claude)
- `api.openai.com` (for OpenAI)

The plugin uses HTTPS (port 443) for all API communications.

## Resource Requirements

LLM integration requires additional system resources:

- **Memory**: At least 4GB of additional RAM is recommended
- **CPU**: Multi-core processor recommended for parallel processing
- **Disk Space**: Additional 100MB for caching and model storage
- **Network**: Reliable internet connection with at least 1 Mbps upload/download

## Reasoning Capabilities

The LLM integration provides the following reasoning capabilities:

1. **Anomaly Analysis**: Deeper understanding of detected anomalies
2. **Situation Assessment**: Holistic evaluation of the security situation
3. **Response Planning**: Intelligent planning of responses to security incidents
4. **Predictive Analysis**: Prediction of future behavior based on observed patterns
5. **Cross-Camera Analysis**: Correlation of information across multiple cameras

## Troubleshooting

### Common Issues

1. **API Key Invalid**
   - Check that the API key is correctly entered
   - Verify that the API key has not expired
   - Ensure the API key has the necessary permissions

2. **Network Connectivity**
   - Verify that the system can reach the API endpoint
   - Check firewall settings for outbound connections
   - Test connectivity using a simple curl command:
     ```
     curl -I https://api.anthropic.com/v1/messages
     ```

3. **High Latency**
   - Increase the `llmRequestTimeoutSecs` setting
   - Consider using a faster model (e.g., claude-3-haiku instead of claude-3-opus)
   - Check network quality and bandwidth

4. **Excessive Resource Usage**
   - Increase the `reasoningInterval` to reduce the frequency of LLM calls
   - Disable LLM integration for less critical cameras
   - Use a smaller model with lower resource requirements

### Logs

LLM-related logs can be found in the NX Meta VMS log directory. Look for entries with the following prefixes:

- `[LLMManager]` - LLM API communication
- `[ReasoningSystem]` - Reasoning process
- `[StrategyManager]` - Strategic planning
- `[NxAgentSystem]` - Overall system coordination

## Best Practices

1. **API Key Security**
   - Store API keys securely
   - Use environment variables when possible
   - Rotate API keys periodically

2. **Model Selection**
   - Use smaller models (e.g., claude-3-haiku) for real-time analysis
   - Use larger models (e.g., claude-3-opus) for in-depth analysis of critical events
   - Balance model capability with response time requirements

3. **Reasoning Interval**
   - Set appropriate reasoning intervals based on camera importance
   - Critical areas: 30-60 seconds
   - Standard areas: 2-5 minutes
   - Low-priority areas: 10+ minutes

4. **Cross-Camera Analysis**
   - Enable for cameras covering connected areas
   - Group cameras logically by physical proximity
   - Consider network topology when enabling cross-camera analysis

## Conclusion

The LLM integration significantly enhances the capabilities of the NX Agent plugin, providing advanced reasoning and analysis capabilities. By properly configuring the LLM settings, you can achieve an optimal balance of performance, resource usage, and security intelligence.
