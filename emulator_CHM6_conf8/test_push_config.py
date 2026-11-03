#!/usr/bin/env python3
"""
Test script verifying that a user / SDN controller can push runtime configurations
via NETCONF <edit-config> and <commit> to the Nokia CHM6 digital twin.
"""

from ncclient import manager
import xml.etree.ElementTree as ET

import os
import sys

HOST = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('NETCONF_HOST', '10.10.102.1')
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else int(os.environ.get('NETCONF_PORT', '2022'))
USER = 'admin'
PASS = 'admin'

def test_netconf_push():
    print(f"[*] Testing runtime configuration push via NETCONF to {HOST}:{PORT}...")
    
    with manager.connect(host=HOST, port=PORT, username=USER, password=PASS,
                         hostkey_verify=False, look_for_keys=False, allow_agent=False) as m:
        
        # 1. Edit Config: Re-tune channel-1 to 193.500 THz and set target power to 2.0 dBm
        edit_xml = """
        <config xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
            <components xmlns="http://openconfig.net/yang/platform">
                <component>
                    <name>channel-1</name>
                    <config>
                        <name>channel-1</name>
                    </config>
                    <optical-channel xmlns="http://openconfig.net/yang/terminal-device">
                        <config>
                            <frequency>193500000</frequency>
                            <target-output-power>2.00</target-output-power>
                            <operational-mode>8</operational-mode>
                        </config>
                    </optical-channel>
                </component>
                <component>
                    <name>transceiver-17</name>
                    <config>
                        <name>transceiver-17</name>
                    </config>
                    <transceiver xmlns="http://openconfig.net/yang/platform/transceiver">
                        <config>
                            <enabled>true</enabled>
                        </config>
                    </transceiver>
                </component>
            </components>
            <terminal-device xmlns="http://openconfig.net/yang/terminal-device">
                <logical-channels>
                    <channel>
                        <index>1</index>
                        <config>
                            <index>1</index>
                            <admin-state>ENABLED</admin-state>
                        </config>
                    </channel>
                </logical-channels>
            </terminal-device>
        </config>
        """
        print("[*] Sending <edit-config> payload...")
        m.edit_config(target='candidate', config=edit_xml)
        
        print("[*] Committing transaction...")
        m.commit()
        print("[+] Commit successful!")
        
        # 2. Verify changes in running datastore
        print("[*] Verifying applied changes in running datastore...")
        verify_filter = """
        <components xmlns="http://openconfig.net/yang/platform">
            <component>
                <name>channel-1</name>
                <optical-channel xmlns="http://openconfig.net/yang/terminal-device"/>
            </component>
        </components>
        """
        reply = m.get_config(source='running', filter=('subtree', verify_filter))
        root = ET.fromstring(reply.xml)
        
        freq = root.find('.//{http://openconfig.net/yang/terminal-device}frequency').text
        pwr = root.find('.//{http://openconfig.net/yang/terminal-device}target-output-power').text
        mode = root.find('.//{http://openconfig.net/yang/terminal-device}operational-mode').text
        
        print(f"[+] Confirmed updated parameters in running datastore:")
        print(f"    - Frequency: {int(freq)/1e6:.3f} THz (expected 193.500 THz)")
        print(f"    - Target Power: {pwr} dBm (expected 2.00 dBm)")
        print(f"    - Operational Mode: Mode {mode} (expected Mode 8)")
        
        assert freq == '193500000', f"Unexpected frequency: {freq}"
        assert float(pwr) == 2.0, f"Unexpected power: {pwr}"
        assert mode == '8', f"Unexpected mode: {mode}"
        
        print("\n[+] RUNTIME CONFIGURATION PUSH TEST PASSED 100%!")

if __name__ == '__main__':
    test_netconf_push()
