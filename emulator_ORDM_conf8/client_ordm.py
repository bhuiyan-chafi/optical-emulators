
import sys
import logging
from ncclient import manager

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s | %(levelname)-8s | %(name)s:%(funcName)s:%(lineno)d - %(message)s')
logger = logging.getLogger(__name__)

def main():
    # Connection details for OpenROADM (Host Port 2028)
    HOST = 'localhost'
    PORT = 2028
    USER = 'admin'
    PASS = 'admin'

    logger.info(f"Connecting to {HOST}:{PORT}...")

    try:
        with manager.connect(host=HOST, port=PORT, username=USER, password=PASS,
                             hostkey_verify=False,
                             device_params={'name': 'default'},
                             look_for_keys=False,
                             allow_agent=False) as m:
            
            logger.info("Connected to OpenROADM Emulator!")
            
            # 1. List Capabilities
            logger.info("Server Capabilities:")
            found_device = False
            for capability in m.server_capabilities:
                if "org-openroadm-device" in capability:
                    logger.info(f"  - {capability} (TARGET FOUND)")
                    found_device = True
                else:
                    logger.debug(f"  - {capability}")
            
            if found_device:
                logger.info("SUCCESS: verified org-openroadm-device capability.")
            else:
                logger.error("FAILURE: org-openroadm-device capability NOT found.")

            # 2. Get Running Config
            logger.info("Getting running configuration...")
            config = m.get_config(source='running')
            logger.info(f"Config: {config.data_xml[:200]}...")

    except Exception as e:
        logger.error(f"Failed to connect: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
