# NETCONF Basics: A Study Guide

## 1. What is NETCONF?
**NETCONF (Network Configuration Protocol)** is an IETF standard protocol (RFC 6241) designed to manage network devices.

-   **Format**: It uses **XML** (Extensible Markup Language) for data encoding. Everything sent and received is wrapped in XML.
-   **Transport**: It typically runs over **SSH** (Secure Shell), ensuring security.
.   **Transaction-based**: Unlike CLI (Command Line Interface), NETCONF allows for atomic transactions (all-or-nothing changes).

## 2. What does it do?
It provides a mechanism to install, manipulate, and delete the configuration of network devices.

-   **Configuration Management**: Push new configs (`edit-config`), retrieve current configs (`get-config`).
-   **State Data**: Retrieve read-only status information (e.g., interface counters, temperature) (`get`).
-   **Notifications**: Subscribe to events (e.g., "Interface went down").

## 3. How do we use it?
NETCONF follows a **Client-Server** (or Manager-Agent) model.

-   **Client (Manager)**: This is your script or application (e.g., the `ssh_client.py` we just ran). It sends **RPC (Remote Procedure Call)** requests.
-   **Server (Agent)**: This is the software running on the network device (or your Docker emulator `TP1`). It listens for RPCs, executes them, and returns an `<rpc-reply>`.

### Common Operations (RPCs):

*   `<get-config>`: Get configuration (e.g., "Show me the interface IP").
*   `<edit-config>`: Change configuration (e.g., "Set IP to 10.0.0.1").
*   `<copy-config>`: Save config (e.g., "Save running config to startup").
*   `<lock>` / `<unlock>`: Prevent others from making changes while you are working.

## 4. Relation to Emulator and Real Devices

Let's assume we are running an emulated optical transponder through docker(which we will discuss for the practical part). In our emulated optical transponders `TP1` Docker container:

-   **The Container (`TP1`)**: Acts as the **NETCONF Server**. It is simulating a physical optical transponder. It has a process running (likely **ConfD** or a similar NETCONF agent) that listens on port 830 (or 2022 in your case) for incoming NETCONF connections. ***Now the fun part is that `confd` server which we will discuss next***.

-   **A Script (`a python script with ssh`)**: Acts as the **NETCONF Client**. It connected to `TP1`, established a session, and asked "What can you do?" (Capabilities). The capabilities are based on the data models which we will discuss in future.

-   **Real Devices**: Work exactly the same way. A physical Nokia, Cisco, or Ciena transponder runs a NETCONF server daemon. You would send the exact same XML commands to the real device as you do to the emulator.

## 5. Future knowledge

-   **Capabilities**: When we first connect, the client and server exchange "Hello" messages containing "Capabilities". This tells you what protocol version (1.0 or 1.1) and **which Data Models** (YANG modules) the device supports. We will write a code to fetch this capabilities, and save them in a `txt` file. 

-   **Data Storage**: NETCONF distinguishes between:

    -   `running`: What is active right now.
    -   `candidate`: A "sandbox" to prepare changes before committing (if supported).
    -   `startup`: The config that loads when the device reboots.

-   **The "Stack"**:
    
    -   **NETCONF**: The *Transport* (The truck delivering the package).
    -   **YANG**: The *Data Model* (The packing list defining what's in the box).
    -   **OpenConfig**: A specific *Standard* of YANG models (Standardized items in the box).
    -   **SSH**: is the secure tunnel (built on the TCP road) that protects the NETCONF truck.

---
## Final verdict: NETCONF is a protocol