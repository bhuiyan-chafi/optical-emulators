# Development Retrospective & Operational Guide: ConfD Emulator

This report documents the technical challenges encountered while setting up the ConfD emulator and provides a guide for its operation.

## Operational Guide

### How to Launch the Container
We have provided helper scripts to simplify the lifecycle:

1.  **Build the Image**:
    ```bash
    cd emulator_latest
    docker build -t confd-latest-img .
    ```

2.  **Start the Emulator**:
    ```bash
    bash emulator_latest/run.sh
    ```
    *This runs the container in the background (`-d`), maps ports, and mounts the project directory.*

3.  **View Logs**:
    ```bash
    docker logs -f confd-emulator
    ```

### How to Save the Docker Image
To export the built image as a portable tar file (e.g., to move to another machine):

```bash
docker save -o confd-latest-img.tar confd-latest-img
```
*To load it on another machine: `docker load -i confd-latest-img.tar`*

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
