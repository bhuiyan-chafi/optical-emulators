
import sys
import os
from ncclient import manager
from loguru import logger

# Constants
HOST = 'localhost'
PORT = 2024
USER = 'admin'
PASS = 'admin'

def main():
    try:
        logger.info(f"Connecting to {HOST}:{PORT}...")
        
        # Connect to ConfD NETCONF server
        # unknown_host_cb=lambda x, y: True needed because we don't have known_hosts entry usually
        with manager.connect(host=HOST, port=PORT, username=USER, password=PASS,
                             hostkey_verify=False, allow_agent=False, look_for_keys=False) as m:
            
            logger.success("Connected!")
            
            # 1. Get Capabilities
            logger.info("Fetching capabilities...")
            for cap in m.server_capabilities:
                logger.info(f"  - {cap}")
                if "openconfig-terminal-device" in cap:
                    logger.success("Found openconfig-terminal-device capability!")

            # 2. Try to get configuration
            logger.info("Getting running configuration...")
            config = m.get_config(source='running').data_xml
            logger.info(f"Config: {config[:200]}...") # Print first 200 chars

    except Exception as e:
        logger.error(f"Failed: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
