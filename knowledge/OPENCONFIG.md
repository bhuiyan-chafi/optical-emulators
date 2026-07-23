# OpenConfig Basics

## 1. What is OpenConfig?
**OpenConfig** is a collaborative effort by network operators (Google, AT&T, Microsoft, Meta, Comcast, etc.) to build **vendor-neutral** data models.

*   **The Problem**: Before OpenConfig, if we wanted to configure an interface:
    *   **Cisco** used `interface GigabitEthernet0/0/0`.
    *   **Juniper** used `set interfaces ge-0/0/0`.
    *   **Nokia** used something else.
    *   This meant we needed different scripts for every vendor.
*   **The Solution**: OpenConfig defines a **Standard YANG Model**.
    *   `openconfig-interfaces.yang`: Every vendor uses the same structure.
    *   we write our code **once**, and it works on Cisco, Juniper, and Nokia.

## 2. Key Models for Optical Transponders
For our specific use case (Optical Transponder `TP1`), we care about a few specific OpenConfig models:

### A. `openconfig-platform`
*   **Purpose**: Models the physical hardware.
*   **What's inside**: Chassis, Linecards, Ports, Transceivers, Fans, Power Supplies.
*   **Key concept**: `components`. Everything is a component.

### B. `openconfig-terminal-device` (The Big One)
This is the specific model for **Optical Transponders**.
*   **Purpose**: Manages the "Client side" (Eth) mapping to the "Line side" (Optical).
*   **Key concepts**:
    *   **Logical Channels**: Represents the signal flow.
    *   **Optical Channels**: The actual wavelength/frequency on the fiber.
    *   **Assignments**: Mapping a client port (100G) to an optical channel (OTN).

## 3. The Hierarchy in Action
When we look at our Transponder, we will see a structure like this:

```text
/components/component[name="OSC_Port"]/optical-channel/config/frequency
```
*   **Root**: Everything starts at the root.
*   **Component**: We select the physical port.
*   **Submodule**: We access the `optical-channel` features.
*   **Leaf**: We set the `frequency`.

## 4. Relation to the Stack
*   **NETCONF** (Transport)
*   **YANG** (Language)
*   **OpenConfig** (The specific Dictionary we are using)

## 5. Practical Implementation
In our `ncclient` scripts, copying the config from a Cisco device to a Nokia device is impossible with native models.
With OpenConfig:
1.  `get-config` from Cisco (returns OpenConfig XML).
2.  `edit-config` to Nokia (sends that same OpenConfig XML).
3.  It just works (ideally).
