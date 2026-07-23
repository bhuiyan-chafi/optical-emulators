from ncclient import manager
import logging

# Enable logging to see raw NETCONF traffic
logging.basicConfig(level=logging.DEBUG)

HOST = 'localhost'
PORT = 2024
USER = 'admin'
PASS = 'admin'

def simple_verify():
    print(f"Connecting to {HOST}:{PORT}...")
    try:
        with manager.connect(host=HOST, port=PORT, username=USER, password=PASS,
                             hostkey_verify=False, look_for_keys=False) as m:
            print("Connected!")
            
            # Try getting just the platform components first (usually single root list)
            filter_xml = """
            <components xmlns="http://openconfig.net/yang/platform">
                <component/>
            </components>
            """
            
            print("Fetching components...")
            reply = m.get_config(source='running', filter=('subtree', filter_xml))
            print("Got reply!")
            print(reply)

    except Exception as e:
        print(f"Error: {e}")

if __name__ == '__main__':
    simple_verify()
