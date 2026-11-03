#!/usr/bin/env python3
"""
Runtime NETCONF Provisioning & Teardown Test for Nokia 1830 PSS-32 (OpenROADM v18.1.0).
Validates dynamic SDN controller workflows over <edit-config> (candidate) + <commit>:
  1. C-Band 800G Add/Drop (CHM6 Mode 7: 193.100 THz, 125.0 GHz slot width, SRG1-PP1-TXRX <-> DEG1-TTP-TXRX)
  2. L-Band 800G Turbo Add/Drop (CHM6 Mode 8: 187.400 THz, 150.0 GHz slot width, SRG1-PP5-TXRX <-> DEG2-TTP-TXRX)
  3. Express Inter-Degree Transit (193.100 THz, 125.0 GHz slot width, DEG3-TTP-TXRX <-> DEG4-TTP-TXRX)
  4. Full Running Datastore Readback Verification
  5. Clean Teardown (nc:operation="delete" + <commit>) restoring the initial unprovisioned state
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
    'mc': 'http://org/openroadm/media-channel-interfaces',
    'nmc': 'http://org/openroadm/network-media-channel-interfaces',
}


def build_mc_nmc_xml(deg_num: int, label: str, center_thz: str, min_thz: str, max_thz: str, width_ghz: str) -> str:
    return f"""
      <interface>
        <name>MC-TTP-DEG{deg_num}-{label}</name>
        <description>Media Channel TTP on DEG{deg_num} at {center_thz} THz ({width_ghz} GHz)</description>
        <type>openROADM-if:mediaChannelTrailTerminationPoint</type>
        <administrative-state>inService</administrative-state>
        <supporting-circuit-pack-name>1/0/DEG{deg_num}-WSS</supporting-circuit-pack-name>
        <supporting-port>DEG{deg_num}-TTP-TXRX</supporting-port>
        <supporting-interface-list>OMS-DEG{deg_num}</supporting-interface-list>
        <mc-ttp xmlns="http://org/openroadm/media-channel-interfaces">
          <min-freq>{min_thz}</min-freq>
          <max-freq>{max_thz}</max-freq>
        </mc-ttp>
      </interface>
      <interface>
        <name>NMC-CTP-DEG{deg_num}-{label}</name>
        <description>Network Media Channel CTP on DEG{deg_num} at {center_thz} THz ({width_ghz} GHz)</description>
        <type>openROADM-if:networkMediaChannelConnectionTerminationPoint</type>
        <administrative-state>inService</administrative-state>
        <supporting-circuit-pack-name>1/0/DEG{deg_num}-WSS</supporting-circuit-pack-name>
        <supporting-port>DEG{deg_num}-TTP-TXRX</supporting-port>
        <supporting-interface-list>MC-TTP-DEG{deg_num}-{label}</supporting-interface-list>
        <nmc-ctp xmlns="http://org/openroadm/network-media-channel-interfaces">
          <frequency>{center_thz}</frequency>
          <width>{width_ghz}</width>
        </nmc-ctp>
      </interface>"""


def build_srg_nmc_xml(pp_num: int, label: str, center_thz: str, width_ghz: str) -> str:
    return f"""
      <interface>
        <name>NMC-CTP-SRG1-PP{pp_num}-{label}</name>
        <description>SRG1-PP{pp_num} Add/Drop NMC-CTP at {center_thz} THz ({width_ghz} GHz)</description>
        <type>openROADM-if:networkMediaChannelConnectionTerminationPoint</type>
        <administrative-state>inService</administrative-state>
        <supporting-circuit-pack-name>1/0/SRG1-WSS</supporting-circuit-pack-name>
        <supporting-port>SRG1-PP{pp_num}-TXRX</supporting-port>
        <nmc-ctp xmlns="http://org/openroadm/network-media-channel-interfaces">
          <frequency>{center_thz}</frequency>
          <width>{width_ghz}</width>
          <mc-slot-width>{width_ghz}</mc-slot-width>
        </nmc-ctp>
      </interface>"""


def build_conn_xml(conn_name: str, src_if: str, dst_if: str, mode: str = 'off', power_dbm: str = None) -> str:
    pwr_xml = f"\n        <target-output-power>{power_dbm}</target-output-power>" if power_dbm is not None else ""
    return f"""
      <roadm-connections>
        <connection-name>{conn_name}</connection-name>
        <opticalControlMode>{mode}</opticalControlMode>{pwr_xml}
        <source>
          <src-if>{src_if}</src-if>
        </source>
        <destination>
          <dst-if>{dst_if}</dst-if>
        </destination>
      </roadm-connections>"""


def test_netconf_push():
    print(f"[*] Testing runtime OpenROADM v18.1.0 FlexGrid provisioning via NETCONF to {HOST}:{PORT}...")

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
        # ---------------------------------------------------------------------
        # 1. Push C-Band Add/Drop, L-Band Add/Drop, and Express Cross-Connects
        # ---------------------------------------------------------------------
        interfaces_xml = "".join([
            # C-band 800G Mode 7 (193.100 THz, 125.0 GHz -> [193.0375, 193.1625] THz) on DEG1 + SRG1-PP1
            build_mc_nmc_xml(1, '193.10THz', '193.10000000', '193.03750000', '193.16250000', '125.00000'),
            build_srg_nmc_xml(1, '193.10THz', '193.10000000', '125.00000'),
            # L-band 800G Mode 8 (187.400 THz, 150.0 GHz -> [187.3250, 187.4750] THz) on DEG2 + SRG1-PP5
            build_mc_nmc_xml(2, '187.40THz', '187.40000000', '187.32500000', '187.47500000', '150.00000'),
            build_srg_nmc_xml(5, '187.40THz', '187.40000000', '150.00000'),
            # Express Transit at 193.100 THz (125.0 GHz) bypassing SRG1 between DEG3 and DEG4
            build_mc_nmc_xml(3, '193.10THz', '193.10000000', '193.03750000', '193.16250000', '125.00000'),
            build_mc_nmc_xml(4, '193.10THz', '193.10000000', '193.03750000', '193.16250000', '125.00000'),
        ])

        connections_xml = "".join([
            # Bidirectional C-band Add + Drop (SRG1-PP1 <-> DEG1)
            build_conn_xml(
                'SRG1-PP1-to-DEG1-193.10THz',
                'NMC-CTP-SRG1-PP1-193.10THz',
                'NMC-CTP-DEG1-193.10THz',
                mode='power',
                power_dbm='0.00',
            ),
            build_conn_xml(
                'DEG1-to-SRG1-PP1-193.10THz',
                'NMC-CTP-DEG1-193.10THz',
                'NMC-CTP-SRG1-PP1-193.10THz',
                mode='off',
            ),
            # Bidirectional L-band Add + Drop (SRG1-PP5 <-> DEG2)
            build_conn_xml(
                'SRG1-PP5-to-DEG2-187.40THz',
                'NMC-CTP-SRG1-PP5-187.40THz',
                'NMC-CTP-DEG2-187.40THz',
                mode='power',
                power_dbm='1.00',
            ),
            build_conn_xml(
                'DEG2-to-SRG1-PP5-187.40THz',
                'NMC-CTP-DEG2-187.40THz',
                'NMC-CTP-SRG1-PP5-187.40THz',
                mode='off',
            ),
            # Bidirectional Express Transit (DEG3 <-> DEG4)
            build_conn_xml(
                'DEG3-to-DEG4-EXPRESS-193.10THz',
                'NMC-CTP-DEG3-193.10THz',
                'NMC-CTP-DEG4-193.10THz',
                mode='power',
                power_dbm='0.00',
            ),
            build_conn_xml(
                'DEG4-to-DEG3-EXPRESS-193.10THz',
                'NMC-CTP-DEG4-193.10THz',
                'NMC-CTP-DEG3-193.10THz',
                mode='power',
                power_dbm='0.00',
            ),
        ])

        edit_payload = f"""
        <config xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
          <org-openroadm-device xmlns="http://org/openroadm/device"
                                xmlns:openROADM-if="http://org/openroadm/interfaces">
            {interfaces_xml}
            {connections_xml}
          </org-openroadm-device>
        </config>
        """

        print("[*] Pushing 10 MC-TTP/NMC-CTP interfaces and 6 ROADM cross-connects to candidate datastore...")
        m.edit_config(target='candidate', config=edit_payload)
        print("[*] Committing candidate transaction to running datastore...")
        m.commit()
        print("[+] Provisioning commit succeeded!")

        # ---------------------------------------------------------------------
        # 2. Verify Provisioned State in Running Datastore
        # ---------------------------------------------------------------------
        print("[*] Reading back running configuration to verify active wavelengths & cross-connects...")
        verify_filter = """
        <org-openroadm-device xmlns="http://org/openroadm/device">
          <interface/>
          <roadm-connections/>
        </org-openroadm-device>
        """
        reply = m.get_config(source='running', filter=('subtree', verify_filter))
        root = ET.fromstring(reply.xml)

        conns = root.findall('.//ordm:roadm-connections', NS)
        intfs = root.findall('.//ordm:interface', NS)
        print(f"[+] Active roadm-connections in running datastore: {len(conns)} (expected 6)")
        for c in conns:
            cname = c.find('ordm:connection-name', NS).text
            src = c.find('ordm:source/ordm:src-if', NS).text
            dst = c.find('ordm:destination/ordm:dst-if', NS).text
            cmode = c.find('ordm:opticalControlMode', NS).text
            print(f"    - {cname:<34} : {src} --> {dst} (mode={cmode})")

        assert len(conns) == 6, f"Expected 6 roadm-connections, got {len(conns)}"
        assert len(intfs) == 18, f"Expected 18 total interfaces (8 initial + 10 dynamic), got {len(intfs)}"

        # ---------------------------------------------------------------------
        # 3. Clean Teardown to Restore Clean Initial State
        # ---------------------------------------------------------------------
        created_connections = [
            'SRG1-PP1-to-DEG1-193.10THz',
            'DEG1-to-SRG1-PP1-193.10THz',
            'SRG1-PP5-to-DEG2-187.40THz',
            'DEG2-to-SRG1-PP5-187.40THz',
            'DEG3-to-DEG4-EXPRESS-193.10THz',
            'DEG4-to-DEG3-EXPRESS-193.10THz',
        ]
        created_interfaces = [
            'NMC-CTP-DEG1-193.10THz',
            'MC-TTP-DEG1-193.10THz',
            'NMC-CTP-SRG1-PP1-193.10THz',
            'NMC-CTP-DEG2-187.40THz',
            'MC-TTP-DEG2-187.40THz',
            'NMC-CTP-SRG1-PP5-187.40THz',
            'NMC-CTP-DEG3-193.10THz',
            'MC-TTP-DEG3-193.10THz',
            'NMC-CTP-DEG4-193.10THz',
            'MC-TTP-DEG4-193.10THz',
        ]

        del_conns_xml = "".join(
            f'<roadm-connections nc:operation="delete"><connection-name>{cn}</connection-name></roadm-connections>'
            for cn in created_connections
        )
        del_intfs_xml = "".join(
            f'<interface nc:operation="delete"><name>{iname}</name></interface>'
            for iname in created_interfaces
        )
        cleanup_payload = f"""
        <config xmlns="urn:ietf:params:xml:ns:netconf:base:1.0"
                xmlns:nc="urn:ietf:params:xml:ns:netconf:base:1.0">
          <org-openroadm-device xmlns="http://org/openroadm/device">
            {del_conns_xml}
            {del_intfs_xml}
          </org-openroadm-device>
        </config>
        """

        print("\n[*] Cleaning up test cross-connects & media channels to restore initial state...")
        m.edit_config(target='candidate', config=cleanup_payload)
        m.commit()

        reply_after = m.get_config(source='running', filter=('subtree', verify_filter))
        root_after = ET.fromstring(reply_after.xml)
        conns_after = root_after.findall('.//ordm:roadm-connections', NS)
        intfs_after = root_after.findall('.//ordm:interface', NS)

        print(f"[+] Post-cleanup roadm-connections: {len(conns_after)} (expected 0)")
        print(f"[+] Post-cleanup interfaces:        {len(intfs_after)} (expected 8 initial OTS/OMS)")
        assert len(conns_after) == 0, f"Expected 0 roadm-connections after cleanup, got {len(conns_after)}"
        assert len(intfs_after) == 8, f"Expected 8 interfaces after cleanup, got {len(intfs_after)}"

        print("\n[+] RUNTIME OPENROADM v18.1.0 PROVISIONING & TEARDOWN TEST PASSED 100%!")


if __name__ == '__main__':
    test_netconf_push()
