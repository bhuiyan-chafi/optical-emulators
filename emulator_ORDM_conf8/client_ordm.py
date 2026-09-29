#!/usr/bin/env python3
import sys
import logging
import xml.etree.ElementTree as ET
from ncclient import manager

logging.basicConfig(level=logging.INFO, format='%(asctime)s | %(levelname)-8s | %(message)s')
logger = logging.getLogger(__name__)

def main():
    HOST = 'localhost'
    PORT = 2028
    USER = 'admin'
    PASS = 'admin'

    logger.info(f"Connecting to OpenROADM Emulator on {HOST}:{PORT}...")

    try:
        with manager.connect(
            host=HOST,
            port=PORT,
            username=USER,
            password=PASS,
            hostkey_verify=False,
            device_params={'name': 'default'},
            look_for_keys=False,
            allow_agent=False,
            timeout=20
        ) as m:
            logger.info("Connected to OpenROADM Emulator successfully!")

            # 1. Verify capabilities
            found_version = None
            for cap in m.server_capabilities:
                if "org-openroadm-device" in cap:
                    found_version = cap
                    break
            logger.info(f"OpenROADM YANG Capability: {found_version}")

            # 2. Query org-openroadm-device configuration
            device_filter = """
            <org-openroadm-device xmlns="http://org/openroadm/device"/>
            """
            reply = m.get_config(source='running', filter=('subtree', device_filter))
            root = ET.fromstring(reply.data_xml)
            ns = {'ordm': 'http://org/openroadm/device'}

            dev = root.find('ordm:org-openroadm-device', ns)
            if dev is None:
                logger.error("Could not find <org-openroadm-device> in running datastore!")
                return

            # Parse info
            info = dev.find('ordm:info', ns)
            node_id = info.findtext('ordm:node-id', default='N/A', namespaces=ns) if info is not None else 'N/A'
            node_type = info.findtext('ordm:node-type', default='N/A', namespaces=ns) if info is not None else 'N/A'
            clli = info.findtext('ordm:clli', default='N/A', namespaces=ns) if info is not None else 'N/A'

            print("\n" + "=" * 60)
            print(f"  OPENROADM NODE SUMMARY: {node_id}")
            print("=" * 60)
            print(f"  Node ID     : {node_id}")
            print(f"  Node Type   : {node_type}")
            print(f"  CLLI        : {clli}")
            print(f"  MSA Release : 18.1.0")

            # Parse Degrees
            degrees = dev.findall('ordm:degree', ns)
            print(f"\n[+] DEGREES ({len(degrees)} configured):")
            for deg in degrees:
                deg_num = deg.findtext('ordm:degree-number', default='?', namespaces=ns)
                cp = deg.find('ordm:circuit-packs/ordm:circuit-pack-name', ns)
                cp_name = cp.text if cp is not None else 'N/A'
                port = deg.find('ordm:connection-ports/ordm:port-name', ns)
                port_name = port.text if port is not None else 'N/A'
                print(f"    - Degree #{deg_num}: Circuit-Pack={cp_name}, Line Port={port_name}")

            # Parse SRGs
            srgs = dev.findall('ordm:shared-risk-group', ns)
            print(f"\n[+] SHARED RISK GROUPS (SRG) ({len(srgs)} configured):")
            for srg in srgs:
                srg_num = srg.findtext('ordm:srg-number', default='?', namespaces=ns)
                max_ports = srg.findtext('ordm:prov-max-add-drop-ports', default='8', namespaces=ns)
                cp = srg.find('ordm:circuit-packs/ordm:circuit-pack-name', ns)
                cp_name = cp.text if cp is not None else 'N/A'
                print(f"    - SRG #{srg_num}: Circuit-Pack={cp_name}, Capacity={max_ports} ports")

            # Parse Circuit Packs & Add/Drop Ports
            cps = dev.findall('ordm:circuit-packs', ns)
            srg_ports = []
            for cp in cps:
                cp_name = cp.findtext('ordm:circuit-pack-name', default='', namespaces=ns)
                if cp_name.startswith('SRG'):
                    ports = cp.findall('ordm:ports', ns)
                    for p in ports:
                        pname = p.findtext('ordm:port-name', default='', namespaces=ns)
                        srg_ports.append(f"{cp_name}/{pname}")

            print(f"\n[+] PROVISIONED ADD/DROP PORTS ({len(srg_ports)} total):")
            for i, p in enumerate(srg_ports, 1):
                print(f"    {i:2d}. {p}")

            # Parse External Links
            links = dev.findall('ordm:external-link', ns)
            rdm_links = []
            xpdr_links = []
            for l in links:
                lname = l.findtext('ordm:external-link-name', default='', namespaces=ns)
                dst = l.find('ordm:destination', ns)
                dst_node = dst.findtext('ordm:node-id', default='', namespaces=ns) if dst is not None else ''
                if "roadm" in dst_node.lower():
                    rdm_links.append((lname, dst_node))
                else:
                    xpdr_links.append((lname, dst_node))

            print(f"\n[+] INTER-ROADM EXTERNAL LINKS ({len(rdm_links)} links):")
            for lname, dst_node in rdm_links:
                print(f"    - {lname} -> Neighbor Node: {dst_node}")

            print(f"\n[+] ATTACHED TRANSPONDER LINKS ({len(xpdr_links)} transponders):")
            for lname, dst_node in xpdr_links:
                print(f"    - {lname} -> Attached Transponder: {dst_node}")

            print("=" * 60 + "\n")

    except Exception as e:
        logger.error(f"Failed to communicate with OpenROADM Emulator: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
