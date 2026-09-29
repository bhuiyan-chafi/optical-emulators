# OpenROADM Emulator Capabilities & Research Data Model Reference

This document provides a comprehensive technical audit of the **OpenROADM Emulator** (`emulator_ORDM_conf8`) and an exhaustive catalog of capabilities and data fields extracted from the **OpenROADM MSA Release 18.1.0** data models. It serves as a persistent reference for optical network research, SDN control, topology provisioning, and physical-layer impairment modeling.

---

## 1. Emulator Architecture & Technical Audit (`emulator_ORDM_conf8`)

### 1.1 How It Was Built
The existing emulator was constructed using ConfD 8.0 Basic running inside an Ubuntu 22.04 container (`confd-ordm-img`):
1. **YANG Ingestion (`init_project.sh`):**
   - Flattened and compiled **78 individual YANG modules** from OpenROADM Release 18.1.0 (`Common/` and `Device/`) into individual `.fxs` compiled binaries inside `my_project/confd-cdb/`.
2. **Startup Constraint Bypass (`ltp_init.xml`):**
   - OpenROADM models define `/org-openroadm-ltp-template:ltp-template/vendor` as a mandatory leaf without a default value.
   - `run.sh` copies `ltp_init.xml` into `/opt/confd/var/confd/cdb/` on container boot so ConfD starts without throwing a fatal schema-validation exception.
3. **Container Launch (`run.sh`):**
   - Runs `confd-ordm` mapping host ports:
     - **NETCONF:** Host `2028` -> Container `2022`
     - **CLI:** Host `2029` -> Container `2024`
     - **IPC:** Host `4568` -> Container `4565`
4. **Verification (`client_ordm.py`):**
   - Connects via `ncclient` on port `2028` and verifies that `http://org/openroadm/device` is listed in the NETCONF server hello capabilities.

### 1.2 Identified Inefficiencies & Limitations
- **Empty Topology:** The CDB contains only `<vendor>OpenROADM-Emulator</vendor>`. No degrees, SRGs, circuit packs, ports, or cross-connections are provisioned.
- **Compilation Overhead:** Compiling 78 separate YANG files produces an excessive memory footprint (`fxs store: >50MB`) when research primarily targets the core switching trees (`org-openroadm-device`).
- **Missing Multi-Layer Topology (`Network/`):** Only `Device/` and `Common/` modules were compiled. The IETF network topology abstraction (`org-openroadm-roadm.yang`, `org-openroadm-degree.yang`, `org-openroadm-srg.yang`) was not exposed.
- **No Operational Telemetry:** Without populated state or performance monitoring (PM) records, impairment-aware routing and closed-loop control cannot be evaluated.

---

## 2. OpenROADM MSA Repository Overview (`./repos/OpenROADM_MSA_Public/model/`)

The local repository is on **OpenROADM Release 18.1.0** (January 2026), structured as follows:
- **`Common/`**: Primitive types, link properties, alarm/PM definitions, port types, optical channel attributes.
- **`Device/`**: Device-level northbound model (`org-openroadm-device.yang`) and interface augmentations (`ots`, `oms`, `mc-ttp`, `nmc-ctp`, `otsi`, `odu`, `ethernet`).
- **`Network/`**: Multi-layer IETF network topology extensions (`org-openroadm-roadm.yang`, `org-openroadm-degree.yang`, `org-openroadm-srg.yang`, `org-openroadm-link.yang`).
- **`Service/`**: End-to-end service orchestration (`org-openroadm-service.yang`, `org-openroadm-operational-mode-catalog.yang`, `org-openroadm-rebalance-optical-power.yang`).
- **`Specifications/`**: Edge optical specifications and physical transmission constraints.

---

## 3. Comprehensive Data Field Catalog for Optical Research

### Category A: Node Identity & Global Boundaries
*Container: `/org-openroadm-device:org-openroadm-device/info`*

| Field Name | YANG Path | Type & Constraints | Access | Research & Optical Utility |
| :--- | :--- | :--- | :--- | :--- |
| `node-id` | `info/node-id` | `node-id-type` (string) | RW | Globally unique node name used by SDN controllers for path computation. |
| `node-type` | `info/node-type` | `enumeration` (`rdm`, `xpdr`, `ila`, `extplug`) | RW | Identifies node class; set to `rdm` for ROADM. |
| `node-subtype` | `info/node-subtype` | `enumeration` (`none`, `edge-optical-spec`) | RW | Conformance to edge optical specifications (metro/edge vs core). |
| `openroadm-version` | `info/openroadm-version` | `openroadm-version-type` | RO | Indicates protocol version (e.g., `18.1.0` supporting Flex-Grid and C+L band). |
| `max-degrees` | `info/max-degrees` | `uint16` | RO | Upper limit of line degree directions (e.g., 2, 4, 8, 16) supported by chassis backplane. |
| `max-srgs` | `info/max-srgs` | `uint16` | RO | Maximum number of local Add/Drop Shared Risk Groups. |
| `max-num-bin-15min-historical-pm` | `info/max-num-bin-15min-historical-pm` | `uint16` | RO | Capacity of rolling 15-minute historical PM bins for machine learning analytics. |
| `geoLocation` | `info/geoLocation/{latitude, longitude}` | `decimal64` (fraction-digits 4) | RW | Site geographic coordinates for fiber latency and physical route mapping. |

---

### Category B: Degree Architecture (Line Direction & Express Switching)
*Container: `/org-openroadm-device:org-openroadm-device/degree[degree-number]`*

Every degree represents an optical line direction (an ingress/egress fiber pair to an adjacent ROADM or ILA node).

| Field Name | YANG Path | Type & Constraints | Access | Research & Optical Utility |
| :--- | :--- | :--- | :--- | :--- |
| `degree-number` | `degree/degree-number` | `uint16` (1..N) | RW | Identifies the line direction (e.g., Degree 1 = East, Degree 2 = West). |
| `max-wavelengths` | `degree/max-wavelengths` | `uint16` | RO | Maximum spectral channels (e.g., 96 channels for 50 GHz grid, or 128 for Flex-Grid). |
| `optical-control` | `degree/optical-control` | `optical-control` | RW | Closed-loop power/gain regulation mode across express pass-through channels. |
| `circuit-packs` | `degree/circuit-packs/circuit-pack-name` | `leafref` (`/circuit-packs/...`) | RW | Associates logical degree with physical WSS and Line Amplifier cards. |
| `connection-ports` | `degree/connection-ports/{circuit-pack-name, port-name}` | `leafref` | RW | Points to primary line transmit/receive optical interface ports. |
| `mc-capability-profile-name` | `degree/mc-capability-profile-name` | `leafref` (`/mc-capability-profile/...`) | RO | Defines Flex-Grid granularity (e.g., 6.25 GHz slice, 12.5 GHz slot, 191.3–196.1 THz range). |
| `wss-capability-profile-name` | `degree/wss-capability-profile-name` | `leafref` (`/wss-capability-profile/...`) | RO | Input/output Power Spectral Density (PSD in dBm/50GHz) limits for the WSS block. |
| `osc-port` | `degree/osc-port/{circuit-pack-name, port-name}` | `leafref` | RW | Out-of-band Optical Supervisory Channel (1510 nm) port for telemetry signaling. |
| `otdr-port` | `degree/otdr-port/{circuit-pack-name, port-name}` | `leafref` | RW | Built-in OTDR line testing port for fiber break and degradation detection. |

---

### Category C: SRG Architecture (Shared Risk Group / Local Add/Drop)
*Container: `/org-openroadm-device:org-openroadm-device/shared-risk-group[srg-number]`*

SRGs manage local wavelength injection (Add) and extraction (Drop) between transponders and degrees.

| Field Name | YANG Path | Type & Constraints | Access | Research & Optical Utility |
| :--- | :--- | :--- | :--- | :--- |
| `srg-number` | `shared-risk-group/srg-number` | `uint16` (1..M) | RW | Identifies the local Add/Drop bank. |
| `max-add-drop-ports` | `shared-risk-group/max-add-drop-ports` | `uint16` | RO | Total physical client drop/add port capacity (e.g., 16, 32, 64 ports). |
| `current-provisioned-add-drop-ports` | `shared-risk-group/current-provisioned-add-drop-ports` | `uint16` | RO | Number of currently active add/drop ports. |
| `wavelength-duplication` | `shared-risk-group/wavelength-duplication` | `enumeration` (`one-per-srg`, `none`, `unrestricted`) | RO | **Critical for CDC**: `none`/`unrestricted` indicates **Contentionless** operation (multiple drops of identical $\lambda$). |
| `subgroup` | `shared-risk-group/subgroup[subgroup-id]` | list | RW | **Directionless** routing partitioning: enables flexible binding to any line degree. |
| `target-input-psd` | `shared-risk-group/target-input-psd` | `psd-dBm-50GHz` (decimal64) | RW | Target optical launch density from transponders entering the multiplexer. |
| `min-input-psd` / `max-input-psd` | `shared-risk-group/{min, max}-input-psd` | `psd-dBm-50GHz` | RO | Dynamic input power window to prevent transponder damage or low OSNR. |
| `max-composite-input-power` | `shared-risk-group/max-composite-input-power` | `power-dBm` (decimal64) | RO | Nonlinear threshold; total combined power allowed across all active add channels. |
| `available-composite-input-power`| `shared-risk-group/available-composite-input-power` | `power-dBm` (decimal64) | RO | Remaining optical power budget for provisioning new transponder channels. |

---

### Category D: Physical Inventory & Port Qualifiers
*Containers: `/org-openroadm-device:org-openroadm-device/circuit-packs` & `/ports`*

| Field Name | YANG Path | Type & Constraints | Access | Research & Optical Utility |
| :--- | :--- | :--- | :--- | :--- |
| `circuit-pack-name` | `circuit-packs/circuit-pack-name` | `string` | RW | Unique name of the hardware card (e.g., `DEG1-WSS`, `SRG1-PP`, `AMP-PREAMP`). |
| `circuit-pack-type` | `circuit-packs/circuit-pack-type` | `string` | RW | Hardware module classification (`wss`, `amplifier`, `mux-demux`, `transponder`). |
| `shelf` / `slot` | `circuit-packs/{shelf, slot}` | `string` | RW | Physical chassis positioning and rack slot layout. |
| `port-name` | `circuit-packs/ports/port-name` | `string` | RW | Physical port identifier on faceplate. |
| `port-qual` | `circuit-packs/ports/port-qual` | `enumeration` | RW | **Role classification:**<br>• `roadm-external`: Degree Line Ports & SRG Add/Drop Ports<br>• `roadm-internal`: Inter-card WSS-to-EDFA patches<br>• `xpdr-network`: Coherent line-side transponder optics<br>• `xpdr-client`: Router client-side 100G/400G/800G grey optics |
| `port-direction` | `circuit-packs/ports/port-direction` | `direction` (`tx`, `rx`, `bidirectional`) | RO | Unidirectional vs bidirectional fiber coupling. |
| `port-power-capability-min-rx / max-rx` | `circuit-packs/ports/roadm-port/port-power-capability-{min,max}-rx` | `power-dBm` | RO | Sensitivity limits of the optical receiver photodiode. |
| `port-power-capability-min-tx / max-tx` | `circuit-packs/ports/roadm-port/port-power-capability-{min,max}-tx` | `power-dBm` | RO | Dynamic attenuation and output capabilities of internal VOA / laser. |
| `logical-connection-point` | `circuit-packs/ports/logical-connection-point` | `string` | RW | Standardized identifier used by SDN controllers (e.g., `DEG1-TTP-TXRX`, `SRG1-PP1-TXRX`). |
| `partner-port` | `circuit-packs/ports/partner-port` | `leafref` | RO | Associates Tx and Rx ports forming a bidirectional circuit. |

---

### Category E: Inter-Equipment Topology Interconnects
*Container: `/org-openroadm-device:org-openroadm-device/external-link[external-link-name]`*

Models physical fiber patches connecting transponders, ROADMs, and external line equipment.

| Field Name | YANG Path | Type & Constraints | Access | Research & Optical Utility |
| :--- | :--- | :--- | :--- | :--- |
| `external-link-name` | `external-link/external-link-name` | `string` | RW | Link identifier (e.g., `LINK-ROADM1-DEG1-to-ROADM2-DEG2`, `LINK-XPDR1-to-SRG1-PP1`). |
| `source` | `external-link/source/{node-id, circuit-pack-name, port-name}` | `string` | RW | Egress interface endpoint. |
| `destination` | `external-link/destination/{node-id, circuit-pack-name, port-name}` | `string` | RW | Ingress interface endpoint. |

---

### Category F: Internal Cross-Connections & Wavelength Switching
*Container: `/org-openroadm-device:org-openroadm-device/roadm-connections[connection-name]`*

Models the dynamic internal optical cross-connect fabric established by WSS switching.

| Field Name | YANG Path | Type & Constraints | Access | Research & Optical Utility |
| :--- | :--- | :--- | :--- | :--- |
| `connection-name` | `roadm-connections/connection-name` | `string` | RW | Unique name of the provisioned wavelength path across the matrix. |
| `source/src-if` | `roadm-connections/source/src-if` | `leafref` (`/interface/name`) | RW | Ingress optical interface (`nmc-ctp` on Degree or SRG). |
| `destination/dst-if` | `roadm-connections/destination/dst-if` | `leafref` (`/interface/name`) | RW | Egress optical interface (`nmc-ctp` on Degree or SRG). |
| `opticalControlMode` | `roadm-connections/opticalControlMode` | `enumeration` (`power`, `gainLoss`) | RW | Control algorithm: `power` uses internal VOA to hit `target-output-power`; `gainLoss` sets fixed attenuation. |
| `target-output-power` | `roadm-connections/target-output-power` | `power-dBm` (decimal64) | RW | Target optical power setpoint for this wavelength channel leaving the ROADM. |
| `connection-state` | `roadm-connections/connection-state` | `enumeration` (`reserved`, `active`, `suspended`) | RO | State of the cross-connect in the WSS hardware. |

---

### Category G: Optical Interface Stack (Layered Transmission Abstraction)
*Container: `/org-openroadm-device:org-openroadm-device/interface[name]`*

OpenROADM defines a strict layered interface hierarchy:

```
[Physical Port]
      │
   ┌──┴──┐
   │ ots │ (Optical Transmission Section: physical fiber span parameters)
   └──┬──┘
   ┌──┴──┐
   │ oms │ (Optical Multiplex Section: composite WDM multiplex)
   └──┬──┘
   ┌──┴────┐
   │ mc-ttp│ (Media Channel: Flex-Grid spectrum allocation, e.g. 50GHz / 75GHz slot)
   └──┬────┘
   ┌──┴─────┐
   │ nmc-ctp│ (Network Media Channel: individual wavelength frequency & optical power)
   └──┬─────┘
   ┌──┴─────┐
   │  otsi  │ (Optical Tributary Signal: transponder coherent modulation & FEC)
   └────────┘
```

| Interface Type | Key Fields | Type & Units | Research & Optical Utility |
| :--- | :--- | :--- | :--- |
| **`ots`** *(Optical Transmission Section)* | `fiber-type`<br>`span-loss-receive`<br>`span-loss-transmit`<br>`ingress-span-loss-aging-margin`<br>`eol-max-load-pIn` | `enum` (`ssmf`, `leaf`, `twrs`)<br>`ratio-dB`<br>`ratio-dB`<br>`ratio-dB`<br>`power-dBm` | Models transmission fiber physics, span attenuation, aging degradation margin, and non-linear power thresholds. |
| **`oms`** *(Optical Multiplex Section)* | `oms-band` | `string` (`C-band`, `L-band`) | Identifies active optical transmission spectrum band. |
| **`mc-ttp`** *(Media Channel)* | `min-freq`<br>`max-freq`<br>`center-freq`<br>`slot-width` | `frequency-THz`<br>`frequency-THz`<br>`frequency-THz`<br>`frequency-GHz` | Defines Flex-Grid ITU-T G.694.1 spectral slot boundaries allocated by SDN controller. |
| **`nmc-ctp`** *(Network Media Channel)* | `frequency`<br>`width`<br>`full-bandwidth-at-3dB`<br>`full-bandwidth-at-10dB` | `frequency-THz`<br>`frequency-GHz`<br>`frequency-GHz`<br>`frequency-GHz` | Channel center frequency (e.g. `193.10000 THz`) and spectral roll-off filters for crosstalk and cascade filtering studies. |
| **`otsi`** *(Optical Tributary Signal)* | `otsi-rate`<br>`modulation-format`<br>`transmit-power`<br>`fec` | `identityref`<br>`enum` (`qpsk`, `dp-16qam`, `dp-64qam`)<br>`power-dBm`<br>`identityref` (`sc-fec`, `o-fec`) | Configures coherent transponder modulation format, launch power, and FEC scheme. |

---

### Category H: Performance Monitoring (PM) & Physical Impairments
*Modules: `org-openroadm-pm.yang` & `org-openroadm-pm-types.yang`*

Real-time analog and digital performance metrics extracted by optical transceivers and ROADM monitoring units:

| PM Parameter Name | Enum Value in YANG | Unit | Measurement Domain | Research Significance |
| :--- | :--- | :--- | :--- | :--- |
| **`opticalPowerInput` (OPIN)** | `5` | `dBm` | Port, OTS, OMS, NMC | Input signal power level for loss tracking and EDFA input monitoring. |
| **`opticalPowerOutput` (OPOUT)**| `3` | `dBm` | Port, OTS, OMS, NMC | Launched power level per channel or composite. |
| **`opticalPowerOutputDrift`** | `242` | `dB` | NMC interface | Delta between measured output power and WSS target setpoint. |
| **`opticalReturnLoss` (ORL)** | `4` | `dB` | OTS interface | Interface reflection ratio; detects dirty connectors or fiber damage. |
| **`OSNR`** | `223` | `dB` | NMC, OCh | **Optical Signal-to-Noise Ratio**; vital for Q-factor and link margin modeling. |
| **`chromaticDispersion` (CD)** | `235` | `ps/nm` | OCh, OTSI | Accumulated chromatic dispersion over fiber spans. |
| **`polarizationModeDispersion`**| `232` | `ps` | OCh, OTSI | Differential Group Delay (DGD) caused by fiber birefringence. |
| **`polarizationDependentLoss`** | `229` | `dB` | OCh, OTSI | PDL accumulated across optical components and WSS passes. |
| **`bitErrorRate` (BER)** | `2` | `scientific` | Pre-FEC / Post-FEC | Raw channel error rate; triggers proactive rerouting prior to packet loss. |

---

## 4. Next Steps for Topology & Port Generation

When ready to generate the emulator configuration, specify:
1. **Node Degree Count ($N$):** e.g., 2 degrees (East/West), 4 degrees, or 8 degrees.
2. **Add/Drop Bank Count ($M$):** e.g., 1, 2, or 4 SRGs.
3. **Ports per SRG ($P$):** e.g., 8, 16, 32, or 64 client-facing ports.
4. **SRG Architecture:** Colorless & Directionless (CD) vs. Contentionless (CDC).
5. **Connected Transponders:** Number of transponders to attach via `external-link`.
