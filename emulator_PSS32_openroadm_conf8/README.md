# Nokia 1830 PSS-32 ROADM Digital Twin Emulator (OpenROADM v18.1.0)

**Product Designation:** Nokia 1830 Photonic Service Switch 32 (PSS-32) Core/Mesh ROADM Digital Twin  
**Photonic Architecture:** 4-Degree Unified Extended C+L Mesh + Contention-Aware FlexGrid SRG WSS (`CD-F`)  
**Data Model Standard:** Official OpenROADM MSA Device Model **v18.1.0** (`org-openroadm-device@2025-12-12`)  
**Emulation Framework:** Tail-f ConfD 8.0.20 with Pre-Compiled OpenROADM v18.1.0 FXS Schemas (staged from `../emulator_ORDM_conf8`)  
**Container Images:** `nokia-pss32-openroadm-conf8:latest` / `chafiullah/emu_pss32_openroadm:latest`  
**Network Architecture:** Zero Host Port Binding (Native SDN Bridge Attachment on `netbrTFS`)  

---

## 1. Architectural Overview & Pairing with Nokia CHM6

This digital twin models a **Nokia 1830 PSS-32 Universal Photonic Shelf** engineered to pair directly with the **Nokia CHM6 Augmented 8-Line C+L Muxponder Digital Twin** (`emulator_CHM6_conf8`) and **TeraFlowSDN (TFS)**:

* **4 Unified Extended C+L Mesh Line Degrees (`DEG1`–`DEG4`):** Models `WR20-TF` Twin FlexGrid Wavelength Selective Switches paired with dual-band Extended C+L line amplifiers (`AS23C`/`AS23L`) covering **186.050 THz to 196.125 THz** across 4 inter-ROADM fiber directions (North, East, South, West).
* **Unified Contention-Aware FlexGrid SRG (`SRG1`):** Models a single unified Extended C+L **SRG WSS (`WR20-TF`) + `CMX` 1×4 passive splitter/combiner** (`CD-F` architecture) with **16 bidirectional add/drop ports** (`SRG1-PP1-TXRX` through `SRG1-PP16-TXRX`):
  * **`wavelength-duplication = one-per-srg`:** Explicitly advertises to SDN controllers and Routing & Spectrum Assignment (RSA) engines that **two connections traversing `SRG1` cannot share the same wavelength/frequency**, preventing intra-SRG wavelength collisions across the shared `SRG1-CP-TXRX` common WSS port.
  * **Tunable ITU-T G.694.1 FlexGrid Passbands:** Supports `6.25 GHz` center-frequency granularity and `12.5 GHz` slot-width granularity (`1` to `24` slots, i.e., `12.5 GHz` to `300.0 GHz`), matching all 8 Nokia ICE6 operational modes (`50 GHz` 100G/200G up to `125 GHz` and `150 GHz` 800G DP-64QAM in both C-band and L-band).
* **Zero Duplicate YANG/FXS Footprint:** Just like `emulator_CHM6_conf8`, zero `.yang` or `.fxs` files are stored inside `emulator_PSS32_openroadm_conf8`. During `deploy.sh`, the 80 compiled OpenROADM v18.1.0 `.fxs` binaries are temporarily staged from `../emulator_ORDM_conf8/my_project/confd-cdb/` into `build_context/confd-cdb/` and removed immediately after `docker build`.

---

## 2. Circuit-Pack & Optical Port Mapping Matrix

| Circuit Pack | Slot | Hardware Model | Port Name(s) | Port Qual | Wavelength Type | Role / Pairing |
| :--- | :---: | :--- | :--- | :--- | :--- | :--- |
| **`1/0/EC32`** | `1` | `EC32` | *(Controller)* | N/A | N/A | Shelf Equipment Controller |
| **`1/0/DEG1-WSS`** | `2` | `WR20-TF + AS23C/AS23L` | `DEG1-TTP-TXRX`<br>`DEG1-CTP-TXRX` | `roadm-external`<br>`roadm-internal` | `multi-wavelength` | Line Degree 1 (North) DWDM Trunk & Internal Mesh |
| **`1/0/DEG2-WSS`** | `3` | `WR20-TF + AS23C/AS23L` | `DEG2-TTP-TXRX`<br>`DEG2-CTP-TXRX` | `roadm-external`<br>`roadm-internal` | `multi-wavelength` | Line Degree 2 (East) DWDM Trunk & Internal Mesh |
| **`1/0/DEG3-WSS`** | `4` | `WR20-TF + AS23C/AS23L` | `DEG3-TTP-TXRX`<br>`DEG3-CTP-TXRX` | `roadm-external`<br>`roadm-internal` | `multi-wavelength` | Line Degree 3 (South) DWDM Trunk & Internal Mesh |
| **`1/0/DEG4-WSS`** | `5` | `WR20-TF + AS23C/AS23L` | `DEG4-TTP-TXRX`<br>`DEG4-CTP-TXRX` | `roadm-external`<br>`roadm-internal` | `multi-wavelength` | Line Degree 4 (West) DWDM Trunk & Internal Mesh |
| **`1/0/SRG1-WSS`** | `6` | `WR20-TF SRG WSS + CMX 1x4` | `SRG1-PP1-TXRX` .. `SRG1-PP8-TXRX`<br>`SRG1-PP9-TXRX` .. `SRG1-PP16-TXRX`<br>`SRG1-CP-TXRX` | `roadm-external`<br>`roadm-external`<br>`roadm-internal` | `wavelength`<br>`wavelength`<br>`multi-wavelength` | CHM6 Sled #1 (`port-17`..`port-24` C+L)<br>CHM6 Sled #2 (`port-17`..`port-24` C+L)<br>Shared Common WSS Port (`one-per-srg`) |

---

## 3. Spectral Capabilities (`FLEXGRID-CL-WSS`)

| Parameter | OpenROADM v18.1.0 Leaf | Value |
| :--- | :--- | :--- |
| **Capability Profile Name** | `mc-capability-profile/profile-name` | `FLEXGRID-CL-WSS` |
| **Minimum Edge Frequency** | `min-edge-freq` | `186.05000000 THz` (Extended L-Band Edge) |
| **Maximum Edge Frequency** | `max-edge-freq` | `196.12500000 THz` (Extended C-Band Edge) |
| **Center Frequency Granularity** | `center-freq-granularity` | `6.25000 GHz` |
| **Slot Width Granularity** | `slot-width-granularity` | `12.50000 GHz` |
| **Minimum / Maximum Slots** | `min-slots` / `max-slots` | `1` (`12.5 GHz`) / `24` (`300.0 GHz`) |
| **Degree Max Wavelengths** | `degree/max-wavelengths` | `192` (`96 C + 96 L`) |
| **SRG Wavelength Duplication** | `shared-risk-group/wavelength-duplication` | `one-per-srg` (Contention-Aware) |

---

## 4. Network Protocols & Zero Host Port Binding Architecture

The container exposes services directly on its Docker bridge IP (`netbrTFS` default `10.10.102.11`) with zero host `-p` port bindings:

| Protocol / Service | Container IP Port | Subsystem / Type | Default Credentials | Description |
| :--- | :--- | :--- | :--- | :--- |
| **NETCONF SSH** | `<container_ip>:2022` | SSH Subsystem `netconf` | `admin` / `admin` | Read-write OpenROADM v18.1.0 NETCONF interface |
| **ConfD CLI SSH** | `<container_ip>:2024` | Interactive SSH CLI | `admin` / `admin` | Operator CLI for inspection & troubleshooting |
| **ConfD Internal IPC** | `127.0.0.1:4565` | Local TCP Socket | Internal | Loopback IPC socket used by `confd_load` |

---

## 5. Build, Deployment & Verification

### 5.1 One-Step Build & Deploy

```bash
cd emulator_PSS32_openroadm_conf8

# Regenerate initial XML, stage v18.1.0 FXS from ../emulator_ORDM_conf8, build image, and start RDM1:
bash deploy.sh
```

### 5.2 Launching Multiple ROADM Instances on `netbrTFS`

```bash
# Launch RDM1 (default 10.10.102.11), RDM2 (10.10.102.12), RDM3 (10.10.102.13):
bash run.sh RDM1 10.10.102.11
bash run.sh RDM2 10.10.102.12
bash run.sh RDM3 10.10.102.13
```

### 5.3 Initial Phase Lifecycle

When `entrypoint.sh` starts ConfD, it loads `/my_project/init_pss32_config.xml` into both the configuration and operational CDB datastores:

* **Shelf, Circuit Packs & 25 Optical Ports:** Initialized `inService`.
* **Line Transport & Multiplex Sections:** `OTS-DEG1`..`OTS-DEG4` and `OMS-DEG1`..`OMS-DEG4` initialized `inService`.
* **Media Channels & Cross-Connects:** `0` `MC-TTP`/`NMC-CTP` interfaces and `0` `roadm-connections` in the initial state, ready for dynamic SDN controller provisioning.

### 5.4 Automated Verification Suite

```bash
# 1. Audit OpenROADM v18.1.0 capabilities, inventory, 4 degrees, SRG1 (one-per-srg), and initial state:
python3 client_verify.py 10.10.102.11 2022

# 2. Test dynamic C-band (125 GHz), L-band (150 GHz), and Express provisioning + teardown:
python3 test_push_config.py 10.10.102.11 2022
```

### 5.5 Hot-Reloading Custom XML Configurations

```bash
docker cp custom_roadm_config.xml RDM1:/tmp/custom_roadm_config.xml
docker exec RDM1 /my_project/load_config.sh /tmp/custom_roadm_config.xml
```

---

*Document generated for the Nokia 1830 PSS-32 Core/Mesh ROADM OpenROADM v18.1.0 Digital Twin (`emulator_PSS32_openroadm_conf8`).*
