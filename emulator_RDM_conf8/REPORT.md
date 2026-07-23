# ConfD 8.0 ROADM Emulator Guide

This directory (`emulator_RDM_conf8`) contains a ConfD 8.0-based emulator configured as a **ROADM** (Reconfigurable Optical Add-Drop Multiplexer).

## Quick Start

### 1. Build the Image
```bash
./build.sh
```
This reuses the ConfD 8.0 zip from `../repos/` and creates the `confd-rdm-img` Docker image.

### 2. Run the Emulator
```bash
./run.sh
```
This starts the container `confd-rdm` with the following port mappings:

| Service | Container Port | Host Port | Description |
|---------|----------------|-----------|-------------|
| NETCONF | 2022 | **2026** | Main programmatic interface (SSH) |
| CLI | 2024 | **2027** | Interactive Command Line (SSH) |
| IPC | 4565 | 4567 | Internal Inter-Process Communication |

### 3. Verify Connectivity

**Using Python Client:**
```bash
python3 client_rdm.py
```
Should report `SUCCESS: Found openconfig-optical-amplifier capability!`.

**Using SSH CLI:**
```bash
ssh -p 2027 admin@localhost
# Password: admin
```

## YANG Models
The emulator is compiled with the following primary OpenConfig models:
- `openconfig-optical-amplifier`
- `openconfig-transport-line-protection`
- `openconfig-platform`

Dependencies (extensions, types, etc.) are included in the `my_project/yang` directory.

## Troubleshooting
- **Logs**: `docker logs confd-rdm`
- **Re-compile**: If you add new models, run:
  ```bash
  docker run --rm -v $(pwd)/my_project:/my_project -w /my_project confd-rdm-img make all
  docker restart confd-rdm
  ```
