#!/usr/bin/env python3
import sys
import xml.dom.minidom
from ncclient import manager

def main():
    HOST = '127.0.0.1'
    PORT = 2028
    USER = 'admin'
    PASS = 'admin'

    print(f"[*] Connecting to Optical Amplifier NETCONF server at {HOST}:{PORT}...")

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
            timeout=15
        ) as m:
            print("[+] Successfully connected to Optical Amplifier Emulator!\n")

            # 1. Check Capabilities
            has_amp = any("optical-amplifier" in cap or "optical-amplfier" in cap for cap in m.server_capabilities)
            if has_amp:
                print("[+] SUCCESS: Found openconfig-optical-amplifier capability in NETCONF server hello!")
            else:
                print("[-] WARNING: openconfig-optical-amplifier not listed in capabilities.")

            # 2. Query Running Configuration
            print("\n[*] Fetching Running Configuration via <get-config>...")
            filter_amp = """
            <optical-amplifier xmlns="http://openconfig.net/yang/optical-amplfier"/>
            """
            config = m.get_config(source='running', filter=('subtree', filter_amp))
            parsed_xml = xml.dom.minidom.parseString(config.data_xml)
            print(parsed_xml.toprettyxml(indent="  "))

            # 3. Query Operational State Telemetry
            print("\n[*] Fetching Full Operational Telemetry via <get>...")
            data = m.get(filter=('subtree', filter_amp))
            parsed_data = xml.dom.minidom.parseString(data.data_xml)
            print(parsed_data.toprettyxml(indent="  "))

    except Exception as e:
        print(f"[-] Connection failed: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
