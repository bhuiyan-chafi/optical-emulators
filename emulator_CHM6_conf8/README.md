# Nokia CHM6 Optical Muxponder Digital Twin Emulator

**Product Designation:** Nokia Coherent Module 6 (CHM6) Digital Twin  
**Base Hardware Platform:** Nokia 1830 Global Express (GX) G42 3RU Modular Chassis  
**Operating Software:** Nokia Converged OS (COS) Release 22.10  
**Emulation Framework:** Tail-f ConfD 8.0 with Native OpenConfig YANG Models  
**Container Image:** `chafiullah/emu_chm6:latest` (Self-contained, zero-mount)  
**Network Architecture:** Zero Host Port Binding (Native SDN Bridge Attachment)  

---

## Augmented 8-Line C+L Band Sled

While commercial Nokia CHM6 sleds feature 2 embedded coherent optical carriers (operating in either C-band or L-band), this digital twin implements an **Augmented 8-Line Architecture (`CHM6-BASE-8L`)** combining:

* **4 Extended C-Band Carriers** (Lines 1 to 4)
* **4 Extended L-Band Carriers** (Lines 5 to 8)

This architecture provides 6.4 Tb/s aggregate coherent line capacity and 1.6 Tb/s client capacity within a single digital twin instance, enabling large-scale full-spectrum C+L optical network emulation, Routing and Spectrum Assignment (RSA) research, and multi-band SDN orchestration in **TeraFlowSDN (TFS)**.

---

## 2. Comprehensive 24-Port Mapping Matrix

The emulator models 24 physical ports strictly conforming to the OpenConfig platform and terminal device YANG models. To guarantee seamless integration with SDN controllers (such as TeraFlowSDN's OpenConfig driver `transponders.py`), all ports use single-hyphen component names (`port-<id>`), where `<id>` is a unique integer string matching the `onos-index` and endpoint UUID.

| Port Name | Role | ONOS Index | Form Factor | Optical PMD / Interface | Tx Component | Optical Channel Component | Logical Channel | Supported Data Rates |
| :--- | :--- | :---: | :--- | :--- | :--- | :---: | :---: | :--- |
| **`port-1`** | `client` | 1 | QSFP28 | `ETH_100GBASE_LR4` | `Tx-1` | N/A | `101` | 100GE, OTU4 |
| **`port-2`** | `client` | 2 | QSFP28 | `ETH_100GBASE_LR4` | `Tx-2` | N/A | `102` | 100GE, OTU4 |
| **`port-3`** | `client` | 3 | QSFP28 | `ETH_100GBASE_LR4` | `Tx-3` | N/A | `103` | 100GE, OTU4 |
| **`port-4`** | `client` | 4 | QSFP28 | `ETH_100GBASE_LR4` | `Tx-4` | N/A | `104` | 100GE, OTU4 |
| **`port-5`** | `client` | 5 | QSFP28 | `ETH_100GBASE_LR4` | `Tx-5` | N/A | `105` | 100GE, OTU4 |
| **`port-6`** | `client` | 6 | QSFP28 | `ETH_100GBASE_LR4` | `Tx-6` | N/A | `106` | 100GE, OTU4 |
| **`port-7`** | `client` | 7 | QSFP28 | `ETH_100GBASE_LR4` | `Tx-7` | N/A | `107` | 100GE, OTU4 |
| **`port-8`** | `client` | 8 | QSFP28 | `ETH_100GBASE_LR4` | `Tx-8` | N/A | `108` | 100GE, OTU4 |
| **`port-9`** | `client` | 9 | QSFP56 | `ETH_100GBASE_LR4` | `Tx-9` | N/A | `109` | 100GE, 200GE |
| **`port-10`** | `client` | 10 | QSFP56 | `ETH_100GBASE_LR4` | `Tx-10` | N/A | `110` | 100GE, 200GE |
| **`port-11`** | `client` | 11 | QSFP56 | `ETH_100GBASE_LR4` | `Tx-11` | N/A | `111` | 100GE, 200GE |
| **`port-12`** | `client` | 12 | QSFP56 | `ETH_100GBASE_LR4` | `Tx-12` | N/A | `112` | 100GE, 200GE |
| **`port-13`** | `client` | 13 | QSFP56_DD | `ETH_400GBASE_FR4` | `Tx-13` | N/A | `113` | 100GE, 200GE, 400GE |
| **`port-14`** | `client` | 14 | QSFP56_DD | `ETH_400GBASE_FR4` | `Tx-14` | N/A | `114` | 100GE, 200GE, 400GE |
| **`port-15`** | `client` | 15 | QSFP56_DD | `ETH_400GBASE_FR4` | `Tx-15` | N/A | `115` | 100GE, 200GE, 400GE |
| **`port-16`** | `client` | 16 | QSFP56_DD | `ETH_400GBASE_FR4` | `Tx-16` | N/A | `116` | 100GE, 200GE, 400GE |
| **`port-17`** | `line` | 17 | NON_PLUGGABLE | ICE6 800G Coherent (C,L-band) | `Tx-17` | `channel-1` | `1` | 100G - 800G Coherent |
| **`port-18`** | `line` | 18 | NON_PLUGGABLE | ICE6 800G Coherent (C,L-band) | `Tx-18` | `channel-2` | `2` | 100G - 800G Coherent |
| **`port-19`** | `line` | 19 | NON_PLUGGABLE | ICE6 800G Coherent (C,L-band) | `Tx-19` | `channel-3` | `3` | 100G - 800G Coherent |
| **`port-20`** | `line` | 20 | NON_PLUGGABLE | ICE6 800G Coherent (C,L-band) | `Tx-20` | `channel-4` | `4` | 100G - 800G Coherent |
| **`port-21`** | `line` | 21 | NON_PLUGGABLE | ICE6 800G Coherent (C,L-band) | `Tx-21` | `channel-5` | `5` | 100G - 800G Coherent |
| **`port-22`** | `line` | 22 | NON_PLUGGABLE | ICE6 800G Coherent (C,L-band) | `Tx-22` | `channel-6` | `6` | 100G - 800G Coherent |
| **`port-23`** | `line` | 23 | NON_PLUGGABLE | ICE6 800G Coherent (C,L-band) | `Tx-23` | `channel-7` | `7` | 100G - 800G Coherent |
| **`port-24`** | `line` | 24 | NON_PLUGGABLE | ICE6 800G Coherent (C,L-band) | `Tx-24` | `channel-8` | `8` | 100G - 800G Coherent |

---

## 3. Nokia ICE6 Optical Transmission Engine Capabilities

The Nokia ICE6 coherent optical engine is modeled in the emulator with high fidelity according to the OpenConfig terminal device properties model (`openconfig-terminal-device-properties`). All 8 multi-rate operational modes are pre-configured in the device's manifest and are immediately discoverable via NETCONF.

### Table 3.1: ICE6 Operational Modes & Transmission Formats

| Mode ID | Designation / Application | Trib Rate | Modulation Format | Symbol Baud Rate | Min Channel Spacing | Optical Spectrum Width | Pulse Shaping |
| :---: | :--- | :--- | :--- | :---: | :---: | :---: | :---: |
| **1** | 100G DP-QPSK Standard | `TRIB_RATE_100G` | `MODULATION_FORMAT_DP_QPSK` | 31.60 GBd | 50.0 GHz (DWDM) | 36.34 GHz | RRC ($\alpha=0.15$) |
| **2** | 200G DP-QPSK Long-Haul/Subsea | `TRIB_RATE_200G` | `MODULATION_FORMAT_DP_QPSK` | 63.20 GBd | 75.0 GHz (Flex) | 72.68 GHz | RRC ($\alpha=0.15$) |
| **3** | 200G DP-8QAM High-Efficiency | `TRIB_RATE_200G` | `MODULATION_FORMAT_DP_8QAM` | 42.00 GBd | 50.0 GHz (Flex) | 48.30 GHz | RRC ($\alpha=0.15$) |
| **4** | 400G DP-16QAM / LC-PCS Long-Haul | `TRIB_RATE_400G` | `MODULATION_FORMAT_DP_16QAM` | 59.84 GBd | 75.0 GHz (Flex) | 68.82 GHz | RRC ($\alpha=0.15$) |
| **5** | 400G DP-8QAM / LC-PCS Extended Reach | `TRIB_RATE_400G` | `MODULATION_FORMAT_DP_8QAM` | 84.00 GBd | 100.0 GHz (Flex) | 96.60 GHz | RRC ($\alpha=0.15$) |
| **6** | 600G DP-32QAM / LC-PCS Regional | `TRIB_RATE_600G` | `MODULATION_FORMAT_DP_32QAM` | 84.00 GBd | 100.0 GHz (Flex) | 96.60 GHz | RRC ($\alpha=0.15$) |
| **7** | 800G DP-64QAM / LC-PCS Metro-Regional | `TRIB_RATE_800G` | `MODULATION_FORMAT_DP_64QAM` | 96.00 GBd | 125.0 GHz (Flex) | 110.40 GHz | RRC ($\alpha=0.15$) |
| **8** | 800G DP-64QAM / LC-PCS ICE6 Turbo | `TRIB_RATE_800G` | `MODULATION_FORMAT_DP_64QAM` | 100.40 GBd | 150.0 GHz (Flex) | 115.46 GHz | RRC ($\alpha=0.15$) |

### Table 3.2: Forward Error Correction (FEC), OSNR & Dynamic Impairment Limits

| Mode ID | FEC Code | FEC Overhead | Net Coding Gain | Min Tx OSNR | Min Rx OSNR | Dynamic Input Power Range | Max Chromatic Dispersion | Max DGD | Max PDL | Pre-FEC BER Threshold |
| :---: | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | `FEC_HD` | 7.00% | 9.50 dB | 35.0 dB | 13.50 dB | -20.00 to 0.00 dBm | 400,000 ps/nm | 160 ps | 3.50 dB | $1.50 \times 10^{-2}$ |
| **2** | `FEC_O` | 20.00% | 11.20 dB | 35.0 dB | 15.00 dB | -20.00 to 0.00 dBm | 350,000 ps/nm | 120 ps | 3.50 dB | $2.00 \times 10^{-2}$ |
| **3** | `FEC_O` | 15.00% | 11.10 dB | 35.0 dB | 17.50 dB | -20.00 to 0.00 dBm | 200,000 ps/nm | 100 ps | 3.50 dB | $2.00 \times 10^{-2}$ |
| **4** | `FEC_C` | 20.00% | 11.50 dB | 35.0 dB | 21.00 dB | -20.00 to 0.00 dBm | 200,000 ps/nm | 80 ps | 3.50 dB | $2.40 \times 10^{-2}$ |
| **5** | `FEC_C` | 33.00% | 12.00 dB | 35.0 dB | 18.50 dB | -20.00 to 0.00 dBm | 300,000 ps/nm | 100 ps | 3.50 dB | $2.80 \times 10^{-2}$ |
| **6** | `FEC_C` | 20.00% | 11.50 dB | 35.0 dB | 23.50 dB | -18.00 to 0.00 dBm | 150,000 ps/nm | 60 ps | 3.00 dB | $2.40 \times 10^{-2}$ |
| **7** | `FEC_C` | 20.00% | 11.50 dB | 35.0 dB | 26.00 dB | -15.00 to 0.00 dBm | 150,000 ps/nm | 50 ps | 3.00 dB | $2.40 \times 10^{-2}$ |
| **8** | `FEC_C` | 33.00% | 12.20 dB | 35.0 dB | 25.00 dB | -15.00 to 0.00 dBm | 150,000 ps/nm | 50 ps | 3.00 dB | $3.30 \times 10^{-2}$ |

---

## 4. Continuous Spectral Tuning & Power Dynamic Ranges

The digital twin models realistic linecard and transceiver optical constraints:

* **Frequency Tuning Range:** 186.050 THz to 196.125 THz (186,050,000 MHz to 196,125,000 MHz).
* **Supported Optical Bands:** Full Extended C-band (191.325 to 196.125 THz) and Extended L-band (186.050 to 191.050 THz).
* **Spectral Grid Granularity:** 6.25 GHz (`oc-opt-term-prop-types:G_6_25GHZ`) with 0.05 GHz fine-tuning capability.
* **Optical Carrier Launch Power Range:** -6.00 dBm to +9.00 dBm (in operational state).
* **Initial Laser Mute State:** -99.00 dBm output power and -99.00 dBm input power (unlit dark fiber).

---

## 5. Network Protocols & Host Port Binding Architecture

* **Direct L3 Controller Reachability:** The SDN controller contacts each device directly at its container IP on the standard NETCONF port `2022`.

| Protocol / Service | Container IP Port | Subsystem / Type | Default Credentials | Description |
| :--- | :--- | :--- | :--- | :--- |
| **NETCONF SSH** | `<container_ip>:2022` | SSH Subsystem `netconf` | `admin` / `admin` | Full read-write OpenConfig NETCONF interface |
| **ConfD CLI SSH** | `<container_ip>:2024` | Interactive SSH CLI | `admin` / `admin` | Cisco-style network operator CLI |
| **ConfD Internal IPC** | `127.0.0.1:4565` | Local TCP Socket | Internal | Loopback socket used by `confd_load` / daemons |

---

## 7. TeraFlowSDN (TFS) Deployment & Integration

### 7.1 Running the Emulator Container on `netbrTFS`

To deploy container `TP1` with static IP `10.10.102.1` attached to `netbrTFS`:

```bash
# 1. Pull the self-contained standalone image:
docker pull chafiullah/emu_chm6:latest

# 2. Launch container with zero host port bindings:
docker run -d \
  --net=netbrTFS \
  --ip=10.10.102.1 \
  --name TP1 \
  chafiullah/emu_chm6:latest
```

## 8. Operations, Dynamic Configuration & Verification

### 8.1 Initial Phase Lifecycle

When the container launches, `entrypoint.sh` starts ConfD and automatically loads `/my_project/init_chm6_config.xml` into the ConfD database:

* **Line Transceivers:** Transceivers `transceiver-17` through `transceiver-24` are `enabled: false`.
* **Lasers Muted:** Optical channels `channel-1` through `channel-8` have target output power at standby (`-30.0 dBm`) and active output power muted to `-99.0 dBm`.
* **Line Channels:** Logical channels `1` through `8` are initialized with `admin-state: DISABLED`, `link-state: DOWN`, and `allocation: 0 Gbps`.
* **Client Channels:** Logical channels `101` through `116` are initialized on standby with `admin-state: ENABLED` and `link-state: DOWN`.

### 8.2 Digital Twin Verification Scripts

The repository includes verification tools to audit and test the digital twin:

```bash
# 1. Audit all 24 ports, 8 line channels, 8 ICE6 modes, and muted power levels:
python3 client_verify.py 10.10.102.1 2022

# 2. Test runtime NETCONF configuration push and datastore verification:
python3 test_push_config.py 10.10.102.1 2022
```

### 8.4 In-Container CLI & Hot-Reloading

For interactive troubleshooting or manual configuration injection:

```bash
# Access the ConfD CLI directly:
ssh -p 2024 admin@10.10.102.1
# Password: admin

# Hot-reload an updated XML file without restarting the container:
docker cp updated_config.xml TP1:/tmp/updated_config.xml
docker exec TP1 /my_project/load_config.sh /tmp/updated_config.xml
```

### 8.5 Building from Source & Docker Hub Publication

To rebuild the self-contained image from scratch:

```bash
cd emulator_CHM6_conf8

# Regenerate Initial XML if Python models were modified:
python3 generate_chm6_xml.py

# Build image and launch local TP1 container:
bash deploy.sh

# Publish to Docker Hub:
docker login
docker push chafiullah/emu_chm6:latest
```

---

*Document generated for the Nokia CHM6 Coherent Muxponder Digital Twin (`emulator_CHM6_conf8`).*
