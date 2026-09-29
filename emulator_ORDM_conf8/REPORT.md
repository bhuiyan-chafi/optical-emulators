# ConfD 8.0 OpenROADM Emulator Guide

This directory (`emulator_ORDM_conf8`) contains a ConfD 8.0-based emulator configured with **OpenROADM Release 18.1.0** data models and a fully provisioned ROADM switching topology.

---

## 1. Quick Start

### 1.1 Build the Image
```bash
./build.sh
```

### 1.2 Run the Emulator
```bash
./run.sh
```

This starts the container `confd-ordm` mapping the following host ports:

| Service | Container Port | Host Port | Protocol | Description |
| :--- | :--- | :--- | :--- | :--- |
| **NETCONF** | 2022 | **2028** | SSH / XML | Programmatic SDN controller & telemetry interface |
| **CLI** | 2024 | **2029** | SSH | Interactive network engineer command line |
| **IPC** | 4565 | **4568** | Internal | ConfD Inter-Process Communication socket |

### 1.3 Verify Topology with Python Client
```bash
python3 client_ordm.py
```

---

## 2. Provisioned Topology Specifications

The emulator automatically loads [ordm_init.xml](file:///home/chafi/optical-emulators/emulator_ORDM_conf8/my_project/confd-cdb/ordm_init.xml) into the ConfD CDB on startup:

- **Node Identity:**
  - `node-id`: `dii-openroadm-emulator`
  - `node-type`: `rdm`
  - `node-subtype`: `none`
  - `clli`: `DII-LAB`
  - `openroadm-version`: `18.1.0` (OpenROADM MSA Release 18.1.0)
- **Line Degrees (4 Directions):**
  - `Degree 1`: Circuit-Pack `DEG1-WSS`, Line Port `DEG1-TTP-TXRX`
  - `Degree 2`: Circuit-Pack `DEG2-WSS`, Line Port `DEG2-TTP-TXRX`
  - `Degree 3`: Circuit-Pack `DEG3-WSS`, Line Port `DEG3-TTP-TXRX`
  - `Degree 4`: Circuit-Pack `DEG4-WSS`, Line Port `DEG4-TTP-TXRX`
- **Shared Risk Groups (2 Add/Drop Banks):**
  - `SRG 1`: Circuit-Pack `SRG1-PP` with 8 ports (`SRG1-PP1-TXRX` to `SRG1-PP8-TXRX`)
  - `SRG 2`: Circuit-Pack `SRG2-PP` with 8 ports (`SRG2-PP1-TXRX` to `SRG2-PP8-TXRX`)
  - **Total Add/Drop Capacity:** 16 optical ports
- **Inter-Equipment External Links:**
  - **3 Adjacent ROADMs:**
    - `Degree 1` $\longleftrightarrow$ `dii-roadm-A` (Line Port `DEG1-TTP-TXRX`)
    - `Degree 2` $\longleftrightarrow$ `dii-roadm-B` (Line Port `DEG1-TTP-TXRX`)
    - `Degree 3` $\longleftrightarrow$ `dii-roadm-C` (Line Port `DEG1-TTP-TXRX`)
    - `Degree 4` reserved for express line transit / expansion
  - **16 Attached Transponders:**
    - Transponders `dii-xpdr-1` to `dii-xpdr-8` patched to `SRG1-PP1` through `SRG1-PP8`
    - Transponders `dii-xpdr-9` to `dii-xpdr-16` patched to `SRG2-PP1` through `SRG2-PP8`
