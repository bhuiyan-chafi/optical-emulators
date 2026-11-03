#!/usr/bin/env python3
"""
Nokia 1830 PSS-32 ROADM Digital Twin Verification Script (Initial Phase)
Connects to ConfD via NETCONF (default port 2022) and audits:
  - OpenROADM v18.1.0 (revision 2025-12-12) NETCONF capability advertisement
  - Node identity & system limits (Nokia 1830 PSS-32, node-type=rdm, version=18.1.0, 4 Degrees, 1 SRG)
  - Chassis shelf & 6 circuit-packs (EC32 controller, 4 Degree WR20-TF WSS packs, 1 Unified SRG1 WSS pack)
  - Extended C+L FlexGrid capability profile (186.050-196.125 THz, 6.25 GHz center, 12.5 GHz slot, up to 300 GHz)
  - 4 Unified C+L Mesh Degrees (DEG1..DEG4) & 10 internal photonic backplane links
  - Unified Contention-Aware SRG1 (wavelength-duplication=one-per-srg, 16 external PP add/drop ports)
  - Initial OTS/OMS line interfaces (8 inService) and 0 active roadm-connections
"""

import os
import sys
import xml.etree.ElementTree as ET
from ncclient import manager

HOST = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('NETCONF_HOST', '10.10.102.11')
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else int(os.environ.get('NETCONF_PORT', '2022'))
USER = 'admin'
PASS = 'admin'

NS = {
    'nc': 'urn:ietf:params:xml:ns:netconf:base:1.0',
    'ordm': 'http://org/openroadm/device',
    'ots': 'http://org/openroadm/optical-transport-interfaces',
    'mc': 'http://org/openroadm/media-channel-interfaces',
    'nmc': 'http://org/openroadm/network-media-channel-interfaces',
    'pcap': 'http://org/openroadm/port-capability',
}


def txt(elem, path, default=''):
    found = elem.find(path, NS)
    return found.text.strip() if (found is not None and found.text is not None) else default


def verify():
    print(f"[*] Connecting to Nokia 1830 PSS-32 OpenROADM NETCONF server at {HOST}:{PORT}...")
    try:
        with manager.connect(
            host=HOST,
            port=PORT,
            username=USER,
            password=PASS,
            hostkey_verify=False,
            look_for_keys=False,
            allow_agent=False,
            timeout=15,
        ) as m:
            print("[+] NETCONF connection established successfully!\n")

            # -----------------------------------------------------------------
            # 1. Server Capability Audit (OpenROADM v18.1.0 / 2025-12-12)
            # -----------------------------------------------------------------
            print("=" * 105)
            print("1. OPENROADM v18.1.0 NETCONF CAPABILITIES AUDIT")
            print("=" * 105)
            ordm_caps = sorted(c for c in m.server_capabilities if 'http://org/openroadm/' in c)
            dev_cap = [c for c in ordm_caps if 'module=org-openroadm-device&' in c]
            print(f"[+] Total OpenROADM YANG capabilities advertised: {len(ordm_caps)}")
            assert dev_cap, "Missing org-openroadm-device capability!"
            print(f"[+] Core Device Model Capability: {dev_cap[0]}")
            assert 'revision=2025-12-12' in dev_cap[0], f"Expected revision 2025-12-12 (v18.1.0), got {dev_cap[0]}"

            # Fetch full org-openroadm-device operational + running tree
            dev_filter = '<org-openroadm-device xmlns="http://org/openroadm/device"/>'
            reply = m.get(filter=('subtree', dev_filter))
            root = ET.fromstring(reply.xml)
            dev = root.find('.//ordm:org-openroadm-device', NS)
            assert dev is not None, "org-openroadm-device container missing from datastore!"

            # -----------------------------------------------------------------
            # 2. Node Info & Shelf Inventory Audit
            # -----------------------------------------------------------------
            print("\n" + "=" * 105)
            print("2. NODE IDENTITY & CHASSIS SHELF AUDIT (Nokia 1830 PSS-32 Core/Mesh ROADM)")
            print("=" * 105)
            info = dev.find('ordm:info', NS)
            assert info is not None, "info container missing!"
            node_id = txt(info, 'ordm:node-id')
            node_type = txt(info, 'ordm:node-type')
            node_sub = txt(info, 'ordm:node-subtype')
            vendor = txt(info, 'ordm:vendor')
            model = txt(info, 'ordm:model')
            ordm_ver = txt(info, 'ordm:openroadm-version')
            max_deg = txt(info, 'ordm:max-degrees')
            max_srg = txt(info, 'ordm:max-srgs')

            print(f"    - Node ID:           {node_id}")
            print(f"    - Node Type/Subtype: {node_type} / {node_sub}")
            print(f"    - Vendor / Model:    {vendor} / {model}")
            print(f"    - OpenROADM Version: {ordm_ver} (Schema v18.1.0)")
            print(f"    - Max Degrees / SRG: {max_deg} Degrees / {max_srg} Unified SRG")

            assert node_id == 'ROADM-1', f"Unexpected node-id: {node_id}"
            assert node_type == 'rdm', f"Expected node-type 'rdm', got {node_type}"
            assert node_sub == 'none', f"Expected node-subtype 'none', got {node_sub}"
            assert vendor == 'Nokia', f"Expected vendor 'Nokia', got {vendor}"
            assert model == '1830 PSS-32', f"Expected model '1830 PSS-32', got {model}"
            assert ordm_ver == '18.1.0', f"Expected openroadm-version '18.1.0', got {ordm_ver}"

            shelves = dev.findall('ordm:shelves', NS)
            assert len(shelves) == 1, f"Expected 1 shelf, found {len(shelves)}"
            sh = shelves[0]
            print(
                f"    - Shelf Inventory:   {txt(sh, 'ordm:shelf-name')} ({txt(sh, 'ordm:shelf-type')}) "
                f"[Admin={txt(sh, 'ordm:administrative-state')}, Oper={txt(sh, 'ordm:operational-state')}]"
            )

            # -----------------------------------------------------------------
            # 3. Circuit Packs & Physical Optical Ports Audit
            # -----------------------------------------------------------------
            print("\n" + "=" * 105)
            print("3. CIRCUIT PACKS & OPTICAL PORTS INVENTORY")
            print("=" * 105)
            cpacks = dev.findall('ordm:circuit-packs', NS)
            print(f"[+] Total Circuit Packs Found: {len(cpacks)} / 6 target\n")
            print(f"{'Circuit Pack':<16} | {'Slot':<5} | {'Type':<18} | {'Model':<26} | {'Ports':<6} | {'Oper State'}")
            print("-" * 92)

            total_ports = 0
            ext_srg_ports = []
            ext_deg_ports = []
            for cp in cpacks:
                cp_name = txt(cp, 'ordm:circuit-pack-name')
                slot = txt(cp, 'ordm:slot')
                cp_type = txt(cp, 'ordm:circuit-pack-type')
                cp_model = txt(cp, 'ordm:model')
                oper = txt(cp, 'ordm:operational-state')
                ports = cp.findall('ordm:ports', NS)
                total_ports += len(ports)
                for p in ports:
                    pname = txt(p, 'ordm:port-name')
                    pqual = txt(p, 'ordm:port-qual')
                    if pqual == 'roadm-external':
                        if pname.startswith('SRG1-PP'):
                            ext_srg_ports.append(pname)
                        elif pname.startswith('DEG'):
                            ext_deg_ports.append(pname)
                print(f"{cp_name:<16} | {slot:<5} | {cp_type:<18} | {cp_model:<26} | {len(ports):<6} | {oper}")

            assert len(cpacks) == 6, f"Expected 6 circuit-packs, found {len(cpacks)}"
            assert len(ext_deg_ports) == 4, f"Expected 4 external degree TTP ports, found {len(ext_deg_ports)}"
            assert len(ext_srg_ports) == 16, f"Expected 16 external SRG1 PP ports, found {len(ext_srg_ports)}"
            assert total_ports == 25, f"Expected 25 total optical ports, found {total_ports}"

            # -----------------------------------------------------------------
            # 4. Media Channel Capability Profile (Extended C+L FlexGrid)
            # -----------------------------------------------------------------
            print("\n" + "=" * 105)
            print("4. EXTENDED C+L FLEXGRID MEDIA CHANNEL CAPABILITY PROFILE (CHM6 ALIGNED)")
            print("=" * 105)
            mc_profiles = dev.findall('ordm:mc-capability-profile', NS)
            assert len(mc_profiles) == 1, f"Expected 1 mc-capability-profile, found {len(mc_profiles)}"
            mcp = mc_profiles[0]
            prof_name = txt(mcp, 'ordm:profile-name')
            min_f = float(txt(mcp, 'ordm:min-edge-freq'))
            max_f = float(txt(mcp, 'ordm:max-edge-freq'))
            cf_gran = float(txt(mcp, 'ordm:center-freq-granularity'))
            sw_gran = float(txt(mcp, 'ordm:slot-width-granularity'))
            min_slots = int(txt(mcp, 'ordm:min-slots'))
            max_slots = int(txt(mcp, 'ordm:max-slots'))

            print(f"    - Profile Name:            {prof_name}")
            print(f"    - Passband Edge Range:     {min_f:.3f} THz .. {max_f:.3f} THz (Unified Extended C+L Band)")
            print(f"    - Center Freq Granularity: {cf_gran:.2f} GHz")
            print(f"    - Slot Width Granularity:  {sw_gran:.2f} GHz")
            print(f"    - Slot Width Range:        {min_slots} .. {max_slots} slots ({min_slots * sw_gran:.1f} .. {max_slots * sw_gran:.1f} GHz)")

            assert prof_name == 'FLEXGRID-CL-WSS'
            assert abs(min_f - 186.050) < 1e-6 and abs(max_f - 196.125) < 1e-6
            assert abs(cf_gran - 6.25) < 1e-6 and abs(sw_gran - 12.5) < 1e-6
            assert min_slots == 1 and max_slots == 24

            # -----------------------------------------------------------------
            # 5. Degrees (DEG1..DEG4) & Contention-Aware Unified SRG1 Audit
            # -----------------------------------------------------------------
            print("\n" + "=" * 105)
            print("5. MESH DEGREES (4x C+L) & UNIFIED CONTENTION-AWARE SRG1 AUDIT")
            print("=" * 105)
            degrees = dev.findall('ordm:degree', NS)
            assert len(degrees) == 4, f"Expected 4 degrees, found {len(degrees)}"
            for deg in sorted(degrees, key=lambda d: int(txt(d, 'ordm:degree-number'))):
                dnum = txt(deg, 'ordm:degree-number')
                max_wl = txt(deg, 'ordm:max-wavelengths')
                d_prof = txt(deg, 'ordm:mc-capability-profile-name')
                conn_p = txt(deg, 'ordm:connection-ports/ordm:port-name')
                print(f"    - Degree {dnum}: Line Port={conn_p:<16} | Max Wavelengths={max_wl} | Profile={d_prof}")
                assert max_wl == '192' and d_prof == 'FLEXGRID-CL-WSS'

            srgs = dev.findall('ordm:shared-risk-group', NS)
            assert len(srgs) == 1, f"Expected 1 unified SRG, found {len(srgs)}"
            srg = srgs[0]
            srg_num = txt(srg, 'ordm:srg-number')
            wl_dup = txt(srg, 'ordm:wavelength-duplication')
            max_pp = txt(srg, 'ordm:max-add-drop-ports')
            prov_pp = txt(srg, 'ordm:current-provisioned-add-drop-ports')
            prov_max = txt(srg, 'ordm:prov-max-add-drop-ports')
            srg_prof = txt(srg, 'ordm:mc-capability-profile-name')

            print(
                f"\n    - Unified SRG {srg_num}: Add/Drop Ports={prov_pp}/{max_pp} (prov-max={prov_max}, SRG1-PP1-TXRX .. SRG1-PP16-TXRX)\n"
                f"      Wavelength Duplication = '{wl_dup}' (Contention-Aware SRG WSS: blocks duplicate colors in SRG1)\n"
                f"      FlexGrid WSS Profile   = '{srg_prof}'"
            )
            assert srg_num == '1'
            assert wl_dup == 'one-per-srg', f"Expected contention-aware 'one-per-srg', got {wl_dup}"
            assert max_pp == '16' and prov_pp == '16' and prov_max == '16'
            assert srg_prof == 'FLEXGRID-CL-WSS'

            ilinks = dev.findall('ordm:internal-link', NS)
            print(f"    - Internal Photonic Mesh Links: {len(ilinks)} (4 SRG1<->DEG + 6 DEG<->DEG)")
            assert len(ilinks) == 10, f"Expected 10 internal links, found {len(ilinks)}"

            # -----------------------------------------------------------------
            # 6. Initial Interfaces & Cross-Connects Audit
            # -----------------------------------------------------------------
            print("\n" + "=" * 105)
            print("6. INITIAL INTERFACES & OPTICAL CROSS-CONNECTS STATE")
            print("=" * 105)
            intfs = dev.findall('ordm:interface', NS)
            conns = dev.findall('ordm:roadm-connections', NS)
            print(f"[+] Total Configured Interfaces: {len(intfs)} (Expected 8 initial OTS/OMS line interfaces)")
            for iface in intfs:
                iname = txt(iface, 'ordm:name')
                itype = txt(iface, 'ordm:type').split(':')[-1]
                iadm = txt(iface, 'ordm:administrative-state')
                ioper = txt(iface, 'ordm:operational-state')
                sport = txt(iface, 'ordm:supporting-port')
                print(f"    - {iname:<12} | Type={itype:<26} | Port={sport:<15} | Admin={iadm}, Oper={ioper}")

            print(f"[+] Initial Active ROADM Cross-Connects: {len(conns)} (Expected 0 in clean initial phase)")
            assert len(intfs) == 8, f"Expected 8 initial OTS/OMS interfaces, found {len(intfs)}"
            assert len(conns) == 0, f"Expected 0 initial roadm-connections, found {len(conns)}"

            print("\n" + "=" * 105)
            print("[+] NOKIA 1830 PSS-32 (OPENROADM v18.1.0) INITIAL DIGITAL TWIN AUDIT PASSED 100%!")
            print("=" * 105)

    except Exception as e:
        print(f"[-] Verification failed with error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    verify()
