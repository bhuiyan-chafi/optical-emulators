# Development Retrospective & Operational Guide: ConfD Emulator

This report documents the technical challenges encountered while setting up the ConfD emulator and provides a guide for its operation.

## Operational Guide

### How to Launch the Container
We have provided helper scripts to simplify the lifecycle:

1.  **Build the Image**:
    ```bash
    cd emulator_TP_conf8
    docker build -t terminalemu-conf8 .
    ```

2.  **Start the Emulator**:
    ```bash
    bash emulator_TP_conf8/run.sh
    ```
    *This runs the container in the background (`-d`), maps ports, and mounts the project directory.*

3.  **View Logs**:
    ```bash
    docker logs -f terminalemu-conf8
    ```

### How to Save the Docker Image
To export the built image as a portable tar file (e.g., to move to another machine):

```bash
docker save -o terminalemu-conf8.tar terminalemu-conf8
```
*To load it on another machine: `docker load -i terminalemu-conf8.tar`*

### How to Connect via SSH
To connect to the **NETCONF subsystem** (for automated configuration):
```bash
# Connects to Host Port 2024 (mapped to Container 2022)
ssh -p 2024 -s admin@localhost netconf
```
*Password: admin (default)*

To connect to the **ConfD CLI** (Human Interface):
```bash
# Connects to Host Port 2025 (mapped to Container 2024)
ssh -p 2025 admin@localhost
```
*Note: We have updated `run.sh` to include this mapping (`-p 2025:2024`).*

### Understanding the Ports
ConfD uses different ports for different interfaces to separate management traffic:

*   **2022 (Container) / 2024 (Host)**: **NETCONF SSH**.
    *   *Purpose*: XML-based protocol for machine-to-machine configuration (used by our `client.py`).
    *   *Standard*: IANA assigned port is 830, but ConfD defaults to 2022.
*   **2024 (Container)**: **CLI SSH**.
    *   *Purpose*: The command-line interface for human operators (Cisco-style or Juniper-style commands).
    *   *Note*: This is distinct from NETCONF to allow separate access controls and user experiences.
*   **2023 (Container)**: **NETCONF TCP**.
    *   *Purpose*: Unencrypted NETCONF. Usually disabled or restricted to `localhost` for internal debugging.
*   **4565**: **IPC (Inter-Process Communication)**.
    *   *Purpose*: Used for ConfD to talk to external applications (like C or Python daemons) that implement data providers or validation logic.

## Multi-Rate Operational Modes (100G, 200G, 400G, 800G)

The emulator has been refined with full standards-compliant modeling based on `openconfig-terminal-device` and `openconfig-terminal-device-properties`:

### Supported Modes Catalog
| Mode ID | Line Bit Rate | Modulation | Symbol Rate (Baud) | Spectrum Width | FEC Scheme | Channel Spacing |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | **100G** (`TRIB_RATE_100G`) | DP-QPSK | 31.60 GBd | 36.34 GHz | `FEC_HD` (7% OH, 9.5 dB NCG) | 50.0 GHz (DWDM) |
| **2** | **200G** (`TRIB_RATE_200G`) | DP-8QAM | 42.00 GBd | 48.30 GHz | `FEC_O` (15% OH, 11.1 dB NCG) | 50.0 GHz (Flex Grid) |
| **3** | **400G** (`TRIB_RATE_400G`) | DP-16QAM | 59.84 GBd | 68.82 GHz | `FEC_C` (15% OH, 10.8 dB NCG) | 75.0 GHz (Flex Grid) |
| **4** | **800G** (`TRIB_RATE_800G`) | DP-16QAM | 118.00 GBd | 135.70 GHz | `FEC_C` (15% OH, 11.5 dB NCG) | 150.0 GHz (Flex Grid) |

### Physical & Logical Architecture
* **Line Side**: 1x 800G Coherent Port (`port-line-1`), `OSFP` 800ZR transceiver (`transceiver-line-1`), and `optical-channel-1` tunable across C-band (191.3 THz – 196.1 THz).
* **Client Side**: 8x 100GE Ports (`port-client-1` to `port-client-8`) with QSFP28 100GBASE-LR4 optics.
* **Logical Channels**: 8x 100GE Ethernet client logical channels (indices `100`–`107`) mapped via assignments to the OTN aggregate line logical channel (index `200`, allocation 800 Gbps).

