//General info
#define SPONAME "SPO1"
//confd lib for docker agents
#define LOCALPATH  ((const unsigned char *)"/confd/bin/")
//confd lib for development agents
//#define LOCALPATH  ((const unsigned char *)"/home/mininet/docker_images/confd/bin/")

//Monitoring info
#define MONITORING 0 //0: disabled, 1: via Socket, 2: polling (via Get)
#define POLLING  2000
//Socket-based monitoring info
#define SERVER_PORT  12346

//Config info
#define CONF_TRANSPONDER 0
#define REST 1 //0: via Socket; 1: via REST API
#define CONFSONIC 0 //0: disabled; 1: enabled

//SOCKET parameters
#define TRANSPONDER_ADDR "10.30.2.24"//"10.30.2.24"
#define TRANSPONDER_PORT 16008
//REST parameters
#define API_IP "10.30.2.24"//"10.30.2.24"
#define API_PORT "5000" // 5000

//variables to Sonic environment scripts
//general script path (pointing to the shared directory
//#define SCRIPT_PATH  ((const unsigned char *)"/home/andrea/shtest")
#define SCRIPT_PATH  ((const unsigned char *)"/scripts")
//to get the rx power Sonic level
//#define GET  ((const unsigned char *)"pp.py")
#define GET  ((const unsigned char *)"eeprom_dom.py")
#define RESTSONIC_IP "10.30.2.44"
//#define RESTSONIC_IP "localhost"//"10.30.2.24"
#define RESTSONIC_PORT 8888

//to configure BGP and interfaces
#define RESTSONIC "127.0.0.1"
#define RESTSONIC_P_IF "3005"
#define RESTSONIC_P_BGP "3004"


//GRPC info
#define ENABLE_GRPC 0
//python grpc for development agents
//#define GRPCPATH  ((const unsigned char *)"/home/andrea/netconf-agent/grpc/python/OCtelemetry2.0/")
//python grpc for docker agents
#define GRPCPATH  ((const unsigned char *)"/grpc/")
#define GRPC_SERVER_IP  ((const unsigned char *)"127.0.0.1")
#define GRPC_SERVER_PORT  ((const unsigned char *)"50051")

//Block Chain info
#define BC_ENABLED 0
//#define BCPATH  ((const unsigned char *)"/home/andrea/netconf-agent/bc/")
#define BCPATH  ((const unsigned char *)"/bc/")
#define BCPORT 3002
#define BCIP  "10.30.2.95"

//#define GRPC_SERVER_IP  ((const unsigned char *)"127.0.0.1")
#define GRPC_SERVER_IP  ((const unsigned char *)"127.0.0.1")
