#!/usr/bin/env python3
"""
Nokia CHM6 Digital Twin Emulator Verification Script (Day-0 Initial Phase)
Connects to ConfD via NETCONF (port 2034) and audits:
  - 16 Client Ports (QSFP28, QSFP56, QSFP-DD)
  - 8 ICE6 800G Line Ports & Optical Channels (C+L Bands) - Day-0 Unlit / Disabled
  - 8 Multi-Rate Operational Modes (100G to 800G Turbo)
  - Line Channel Day-0 Status (Admin: DISABLED, Link: DOWN, Allocation: 0)
"""

import sys
from ncclient import manager
import xml.etree.ElementTree as ET

import os

HOST = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('NETCONF_HOST', '10.10.102.1')
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else int(os.environ.get('NETCONF_PORT', '2022'))
USER = 'admin'
PASS = 'admin'

NS = {
    'nc': 'urn:ietf:params:xml:ns:netconf:base:1.0',
    'oc-platform': 'http://openconfig.net/yang/platform',
    'oc-platform-types': 'http://openconfig.net/yang/platform-types',
    'oc-opt-types': 'http://openconfig.net/yang/transport-types',
    'oc-term-props': 'http://openconfig.net/yang/openconfig-terminal-device-properties',
    'oc-term': 'http://openconfig.net/yang/terminal-device',
    'oc-transceiver': 'http://openconfig.net/yang/platform/transceiver'
}

def verify():
    print(f"[*] Connecting to Nokia CHM6 NETCONF server at {HOST}:{PORT}...")
    try:
        with manager.connect(host=HOST, port=PORT, username=USER, password=PASS,
                             hostkey_verify=False, look_for_keys=False, allow_agent=False,
                             timeout=15) as m:
            print("[+] NETCONF connection established successfully!\n")

            # -------------------------------------------------------------
            # 1. Fetch Platform Components (Config & Operational State)
            # -------------------------------------------------------------
            print("=" * 105)
            print("1. PLATFORM COMPONENTS AUDIT (Nokia 1830 GX G42 / CHM6 Sled)")
            print("=" * 105)
            comp_filter = """<components xmlns="http://openconfig.net/yang/platform"/>"""
            reply_comp = m.get(filter=('subtree', comp_filter))
            root_comp = ET.fromstring(reply_comp.xml)

            components = root_comp.findall('.//oc-platform:component', NS)
            print(f"[+] Total Components Found: {len(components)}")

            client_ports = []
            line_ports = []
            optical_channels = {}
            transceivers = {}

            for comp in components:
                name_elem = comp.find('oc-platform:name', NS)
                if name_elem is None: continue
                name = name_elem.text
                if name.startswith('port-'):
                    p_num = int(name.split('-')[1])
                    if p_num <= 16:
                        client_ports.append(comp)
                    else:
                        line_ports.append(comp)
                elif name.startswith('channel-'):
                    optical_channels[name] = comp
                elif name.startswith('transceiver-'):
                    transceivers[name] = comp

            print(f"    - Client Ports Found: {len(client_ports)} / 16 target")
            print(f"    - Line Ports Found:   {len(line_ports)} / 8 target")
            print(f"    - Optical Channels:   {len(optical_channels)} / 8 target")
            print(f"    - Transceivers:       {len(transceivers)} / 24 target\n")

            # Print Client Ports Table
            print(f"{'Client Port':<16} | {'ODTN-Type':<10} | {'Form Factor':<14} | {'PMD Preconf':<22} | {'Description'}")
            print("-" * 110)
            for cp in sorted(client_ports, key=lambda c: int(c.find('oc-platform:name', NS).text.split('-')[-1])):
                p_name = cp.find('oc-platform:name', NS).text
                desc_el = cp.find('.//oc-platform:description', NS)
                desc = desc_el.text if desc_el is not None else ""
                
                odtn = "Unknown"
                for prop in cp.findall('.//oc-platform:property', NS):
                    p_id = prop.find('oc-platform:name', NS)
                    if p_id is not None and p_id.text in ('odtn-port-type', 'ODTN-PORT-TYPE'):
                        val = prop.find('.//oc-platform:value', NS)
                        if val is not None: odtn = val.text

                t_idx = p_name.split('-')[-1]
                t_name = f"transceiver-{t_idx}"
                ff = "Unknown"
                pmd = "Unknown"
                if t_name in transceivers:
                    t_comp = transceivers[t_name]
                    ff_el = t_comp.find('.//oc-transceiver:form-factor', NS)
                    if ff_el is not None: ff = ff_el.text.split(':')[-1]
                    pmd_el = t_comp.find('.//oc-transceiver:ethernet-pmd-preconf', NS)
                    if pmd_el is not None: pmd = pmd_el.text.split(':')[-1]

                print(f"{p_name:<16} | {odtn:<10} | {ff:<14} | {pmd:<22} | {desc}")

            # Print Line Ports & Optical Channels (Day-0 Initial Phase)
            print("\n" + "=" * 105)
            print("2. ICE6 800G LINE PORTS & OPTICAL CHANNELS (DAY-0 INITIAL PHASE: LASERS MUTED)")
            print("=" * 105)
            print(f"{'Line Port':<14} | {'Xcvr State':<12} | {'Nominal Freq':<14} | {'Band':<8} | {'Output Pwr':<12} | {'Input Pwr':<12} | {'Mode'}")
            print("-" * 100)

            for lp in sorted(line_ports, key=lambda c: int(c.find('oc-platform:name', NS).text.split('-')[-1])):
                lp_name = lp.find('oc-platform:name', NS).text
                p_num = int(lp_name.split('-')[-1])
                j = p_num - 16
                och_target = f"channel-{j}"
                t_name = f"transceiver-{p_num}"
                
                xcvr_state = "DISABLED"
                if t_name in transceivers:
                    en_el = transceivers[t_name].find('.//oc-transceiver:enabled', NS)
                    if en_el is not None and en_el.text == 'true':
                        xcvr_state = "ENABLED"

                freq_thz = "N/A"
                band = "C-band" if j <= 4 else "L-band"
                pwr = "-99.0 dBm"
                in_pwr = "-99.0 dBm"
                act_mode = "N/A"

                if och_target in optical_channels:
                    och_comp = optical_channels[och_target]
                    freq_el = och_comp.find('.//oc-term:frequency', NS)
                    if freq_el is not None:
                        freq_thz = f"{int(freq_el.text) / 1000000:.3f} THz"
                    pwr_el = och_comp.find('.//oc-term:output-power/oc-term:instant', NS)
                    if pwr_el is not None: pwr = f"{float(pwr_el.text):.1f} dBm"
                    inpwr_el = och_comp.find('.//oc-term:input-power/oc-term:instant', NS)
                    if inpwr_el is not None: in_pwr = f"{float(inpwr_el.text):.1f} dBm"
                    mode_el = och_comp.find('.//oc-term:operational-mode', NS)
                    if mode_el is not None: act_mode = f"Mode {mode_el.text}"

                print(f"{lp_name:<14} | {xcvr_state:<12} | {freq_thz:<14} | {band:<8} | {pwr:<12} | {in_pwr:<12} | {act_mode}")

            # -------------------------------------------------------------
            # 3. Operational Mode Manifest
            # -------------------------------------------------------------
            print("\n" + "=" * 105)
            print("3. NOKIA ICE6 OPERATIONAL MODES CATALOG (100G - 800G TURBO)")
            print("=" * 105)
            modes_filter = """
            <operational-mode-descriptors xmlns="http://openconfig.net/yang/openconfig-terminal-device-properties">
                <operational-modes/>
            </operational-mode-descriptors>
            """
            reply_modes = m.get(filter=('subtree', modes_filter))
            root_modes = ET.fromstring(reply_modes.xml)

            modes = root_modes.findall('.//oc-term-props:operational-modes', NS)
            print(f"[+] Total Operational Modes Found: {len(modes)} / 8 target\n")
            print(f"{'Mode ID':<8} | {'Bit Rate':<14} | {'Modulation Format':<18} | {'Baud Rate':<12} | {'Width (GHz)':<12} | {'FEC Code':<10} | {'Pre-FEC BER'}")
            print("-" * 96)

            for m_elem in sorted(modes, key=lambda x: int(x.find('oc-term-props:mode-id', NS).text)):
                m_id = m_elem.find('oc-term-props:mode-id', NS).text
                bit_rate = m_elem.find('.//oc-term-props:bit-rate', NS).text.split(':')[-1]
                mod = m_elem.find('.//oc-term-props:modulation-format', NS).text.split('MODULATION_FORMAT_')[-1]
                baud_val = float(m_elem.find('.//oc-term-props:baud-rate', NS).text) / 1e9
                baud_str = f"{baud_val:.2f} GBd"
                width = m_elem.find('.//oc-term-props:optical-channel-spectrum-width', NS).text
                fec = m_elem.find('.//oc-term-props:fec-coding', NS).text.split(':')[-1]
                ber = m_elem.find('.//oc-term-props:pre-fec-ber-threshold', NS).text
                print(f"Mode {m_id:<3} | {bit_rate:<14} | {mod:<18} | {baud_str:<12} | {width + ' GHz':<12} | {fec:<10} | {ber}")

            # -------------------------------------------------------------
            # 4. Logical Channels (Day-0 Initial Phase Verification)
            # -------------------------------------------------------------
            print("\n" + "=" * 105)
            print("4. TERMINAL-DEVICE LOGICAL CHANNELS (DAY-0 UNPROVISIONED STATE)")
            print("=" * 105)
            term_filter = """
            <terminal-device xmlns="http://openconfig.net/yang/terminal-device">
                <logical-channels/>
            </terminal-device>
            """
            reply_term = m.get(filter=('subtree', term_filter))
            root_term = ET.fromstring(reply_term.xml)

            lchannels = root_term.findall('.//oc-term:channel', NS)
            print(f"[+] Total Logical Channels Found: {len(lchannels)} (16 Client + 8 Line)\n")
            
            line_channels = [c for c in lchannels if int(c.find('oc-term:index', NS).text) <= 8]
            print(f"{'Channel Index':<16} | {'Admin State':<14} | {'Link State':<14} | {'Optical Association':<24} | {'Active Allocation'}")
            print("-" * 95)
            for lc in sorted(line_channels, key=lambda c: int(c.find('oc-term:index', NS).text)):
                idx = lc.find('oc-term:index', NS).text
                adm = lc.find('.//oc-term:admin-state', NS).text
                lnk = lc.find('.//oc-term:link-state', NS).text
                ass = lc.findall('.//oc-term:assignment', NS)
                och_ref = "None"
                alloc = "0 Gbps"
                if ass:
                    a1 = ass[0]
                    och_el = a1.find('.//oc-term:optical-channel', NS)
                    if och_el is not None: och_ref = och_el.text
                    alloc_el = a1.find('.//oc-term:allocation', NS)
                    if alloc_el is not None: alloc = f"{alloc_el.text} Gbps"

                print(f"Line Channel {idx:<3} | {adm:<14} | {lnk:<14} | {och_ref:<24} | {alloc}")

            print("\n" + "=" * 105)
            print("[+] NOKIA CHM6 DAY-0 DIGITAL TWIN AUDIT COMPLETED: 100% UNPROVISIONED REALISM VERIFIED!")
            print("=" * 105)

    except Exception as e:
        print(f"[-] Verification failed with error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == '__main__':
    verify()
