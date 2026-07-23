# ConfD 8.0 OpenROADM Emulator Guide

This directory (`emulator_ORDM_conf8`) contains a ConfD 8.0-based emulator configured with **OpenROADM** models.

## Quick Start

### 1. Build the Image
```bash
./build.sh
```

### 2. Run the Emulator
```bash
./run.sh
```
This starts the container `confd-ordm` with the following port mappings:

| Service | Container Port | Host Port | Description |
|---------|----------------|-----------|-------------|
| NETCONF | 2022 | **2028** | Main programmatic interface (SSH) |
| CLI | 2024 | **2029** | Interactive Command Line (SSH) |
| IPC | 4565 | 4568 | Internal Inter-Process Communication |

### 3. Verify Connectivity

**Using Python Client:**
```bash
python3 client_ordm.py
```
Should report `SUCCESS: verified org-openroadm-device capability.`.

**Using SSH CLI:**
```bash
ssh -p 2029 admin@localhost
# Password: admin
```

## Technical Details
- **Modules**: `org-openroadm-device`, plus dependencies from `Common/` and `Device/`.
- **Initialization**: OpenROADM requires mandatory data to start. `run.sh` automatically injects `ltp_init.xml` into the CDB directory to satisfy the `/org-openroadm-ltp-template:ltp-template/vendor` requirement.
