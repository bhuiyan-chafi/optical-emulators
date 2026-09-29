# ConfD 8.0 Optical Amplifier Emulator Guide

This directory (`emulator_AMP_conf8`) contains a ConfD 8.0-based emulator configured as an **OpenConfig Optical Amplifier**.

## Quick Start

### 1. Build / Tag Image

```bash
./build.sh
```

Uses the pre-built `terminalemu-conf8:latest` or builds from `confd-basic-8.0.20.linux.x86_64.signed.zip` to produce `optical-amp-conf8:latest`.

### 2. Deploy & Run with Startup Configuration

Deploy any of the sample XML configuration files generated in `sample_amplifier_xmls/`:

```bash
# Default (EDFA Pre-Amplifier):
./deploy.sh

# Or deploy any specific amplifier type:
./deploy.sh ./sample_amplifier_xmls/edfa_booster.xml
./deploy.sh ./sample_amplifier_xmls/raman_backward.xml
./deploy.sh ./sample_amplifier_xmls/raman_forward.xml
./deploy.sh ./sample_amplifier_xmls/hybrid_amplifier.xml
```

### 3. Port Mappings

| Service     | Container Port | Host Port | Description                                |
| :---------- | :------------- | :-------- | :----------------------------------------- |
| **NETCONF** | 2022           | **2028**  | Main programmatic NETCONF interface (SSH)  |
| **CLI**     | 2024           | **2029**  | Interactive Command Line (SSH)             |
| **IPC**     | 4565           | **4568**  | Internal ConfD Inter-Process Communication |

### 4. Verify Functionality

**Using Python Client:**

```bash
python3 client_amp.py
```

Outputs the server capabilities, running config, and live operational telemetry stats.

**Using SSH CLI:**

```bash
ssh -p 2029 admin@localhost
# Password: admin
```

**Using NETCONF console:**

```bash
ssh -p 2028 -s admin@localhost netconf
# Password: admin
```
