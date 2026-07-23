
import sys
import logging
from ncclient import manager

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s | %(levelname)-8s | %(name)s:%(funcName)s:%(lineno)d - %(message)s')
logger = logging.getLogger(__name__)

def main():
    # Connection details for ROADM (Host Port 2026)
    HOST = 'localhost'
    PORT = 2026
    USER = 'admin'
    PASS = 'admin'

    logger.info(f"Connecting to {HOST}:{PORT}...")

    try:
        with manager.connect(host=HOST, port=PORT, username=USER, password=PASS,
                             hostkey_verify=False,
                             device_params={'name': 'default'},
                             look_for_keys=False,
                             allow_agent=False) as m:
            
            logger.info("Connected to ROADM Emulator!")
            
            # 1. List Capabilities
            logger.info("Server Capabilities:")
            for capability in m.server_capabilities:
                logger.info(f"  - {capability}")
                
            # Check for ROADM specific models
            if any("optical-amplifier" in cap for cap in m.server_capabilities):
                logger.info("SUCCESS: Found openconfig-optical-amplifier capability!")
            else:
                logger.error("FAILURE: openconfig-optical-amplifier NOT found in capabilities.")

            if any("transport-line-protection" in cap for cap in m.server_capabilities):
                logger.info("SUCCESS: Found openconfig-transport-line-protection capability!")
            else:
                logger.warning("WARNING: openconfig-transport-line-protection NOT found.")

            # 2. Get Running Config (Empty but tests retrieval)
            logger.info("Getting running configuration...")
            config = m.get_config(source='running')
            logger.info(f"Config: {config.data_xml[:100]}...")

    except Exception as e:
        logger.error(f"Failed to connect: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
