# YANG Basics

## 1. What is YANG?
**YANG (Yet Another Next Generation)** is a data modeling language designed specifically for network configuration.

*   **The Blueprint**: If NETCONF is the transport protocol, YANG is the logic that defines *what* can be transported.
*   **Structure**: It defines data in a hierarchical tree structure (like a folder system).
*   **Constraints**: It enforces rules (e.g., "Frequency must be between 191.1 and 196.1").

## 2. Key Concepts & Hierarchy
YANG models are built using a few core building blocks. You will see these everywhere in OpenConfig.

*   **`container`**: A folder that groups related nodes. It has no value itself, just children.
    *   *Example*: `optical-channel` (grouping all settings for one channel).
*   **`list`**: A collection of similar items (like a table rows). Each item is identified by a **key**.
    *   *Example*: `interfaces` (a list of all ports: eth0, eth1, etc.).
*   **`leaf`**: The actual data field holding a value.
    *   *Example*: `output-power` (holds the value `-3.5`).
*   **`type`**: The data type (integer, string, boolean, enumeration).

## 3. How it looks (Simplified Example)
```yang
container optical-device {
    list transponder {
        key "name";           // Unique ID for each transponder
        
        leaf name {           // The name (e.g., "TP1")
            type string;
        }

        leaf frequency {      // The frequency setting
            type decimal64;
            units "THz";
        }

        leaf enabled {        // On switch
            type boolean;
            default "false";
        }
    }
}
```

## 4. The Analogy From CONFD
*   **NETCONF**: The **Truck**.
*   **XML**: The **Box**.
*   **ConfD**: The **Warehouse Manager**.
*   **YANG**: The **Itemized Packing List ("Manifest")**.
    *   The list says: "Box 1 *must* contain a `frequency`."
    *   The list says: "The `frequency` *must* be a number."
    *   If you put a string "Fast" into the `frequency` slot, the packing list validation fails, and ConfD rejects it.

## 5. Why is this specific to OpenConfig?
YANG is just the *language*. Anyone can write a YANG model.
*   **Vendor Models**: Nokia, Cisco, and Juniper write their own proprietary YANG models (e.g., `nokia-optical.yang`).
*   **OpenConfig Models**: Google, AT&T, and Microsoft got together and wrote a **Standardized** set of YANG models (e.g., `openconfig-terminal-device.yang`) so they could configure *any* vendor's device with the exact same code. This is what we are studying.
