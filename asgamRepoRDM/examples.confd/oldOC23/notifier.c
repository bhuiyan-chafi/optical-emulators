///////////////////////////////////////////////
//                Copyright
//          ConfD OpenConfig agent  
//                  developed by
//              Andrea Sgambelluri
////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/poll.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>

//#define MAXDEPTH 40  
//#define MAXKEYLEN 10   
#include <confd_lib.h>
#include <confd_dp.h>
#include <confd_cdb.h>
#include <confd_maapi.h>

#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <math.h>

#include <sys/inotify.h>

#include "openconfig-terminal-device.h"
#include "openconfig-telemetry.h"
#include "vlan.h"
#include "openconfig-interfaces.h"
#include "openconfig-bgp.h"
#include "openconfig-protocols.h"
#include "openconfig-if-ip.h"
#include "openconfig-if-ethernet.h"


#include "transponder_driver.h"
#include "filter_driver.h"
#include "uthash.h"
#include "utlist.h"
#include "parameters.h"

#define OK(val) (assert((val) == CONFD_OK))

// Returns number of elements in the array x
#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

#define TRUE  1
#define FALSE 0



//log file
FILE *logger;
#define LOGFILE "NETCONFagent.log"


static char *user = "admin";
static const char *groups[] = {"admin"};
static char *context = "maapi";
static char *confd_addr = "127.0.0.1";

static pthread_t spo_thread_id;
pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t grpc_thread_id;

static pthread_t copy_pthread;


static volatile int keepRunning = 1;
static int threadsKeepRunning = 1;

static int ctlsock, workersock, cdb_sock, sub_sock, maapis;
static struct confd_daemon_ctx *deamon_p;

struct thread_args {
  char path[BUFSIZ];
  int32_t sample_frequency;
};

typedef struct pthread_t_ll { //linked list of thread ids
    pthread_t thread_id;
    struct pthread_t_ll *next, *prev;
} pthread_t_ll;

struct subscriber {
    int sub_id;        /* key */
    pthread_t_ll *thread_ids;
    UT_hash_handle hh;  /* makes this structure hashable */
};

struct key_value {
  char key[BUFSIZ];
  char value[BUFSIZ];
};

struct subscriber *tel_subs_htable = NULL;    /* important! initialize to NULL */

static int copy_thread_td_activated=0;
static int copy_thread_c_activated=0;

struct copy {
  char root[BUFSIZ];
  uint32_t index;
  char name[BUFSIZ];;
};


struct notif {
    struct confd_datetime eventTime;
    confd_tag_value_t *vals;
    int nvals;
};

struct subscription {
  char port[BUFSIZ];
  char sample_interval[BUFSIZ];
  char heartbeat_interval[BUFSIZ];
  char suppress_redundant[BUFSIZ];
  char destination_address[BUFSIZ];
  char protocol[BUFSIZ];
  char encoding[BUFSIZ];
  char path[BUFSIZ];
  uint8_t init;
};

struct subscription subscriptions[9000];

/*
*/
float rx=0.0f;
float rx_min_gap=0.5f;


//ber values
float bersum[20000] = {0.0f};
float beravg[20000] = {0.0f};
float bermax[20000] = {0.0f};
float bermin[20000] = {0.0f};
float count_ber[20000] = {0.0f};
//q values
float qsum[20000] = {0.0f};
float qavg[20000] = {0.0f};
float qmax[20000] = {0.0f};
float qmin[20000] = {0.0f};
float count_q[20000] = {0.0f};
//cd values
float cdsum[20000] = {0.0f};
float cdavg[20000] = {0.0f};
float cdmax[20000] = {0.0f};
float cdmin[20000] = {0.0f};
float count_cd[20000] = {0.0f};
//q values
float esnrsum[20000] = {0.0f};
float esnravg[20000] = {0.0f};
float esnrmax[20000] = {0.0f};
float esnrmin[20000] = {0.0f};
float count_esnr[20000] = {0.0f};


struct Floats {
    float favg;
    float fmin;
    float fmax;
};


struct Ports {
  int port_id;
  int type; //1: SPO, 2: pluggable
  int config; //0: not configured, 1: configured
  char notes[BUFSIZ]; //additional id for SPO
};

struct Ports  monPorts[24]={{0,0,0,""}};

int numMonPorts=2;




static pthread_t grpc_sub_thread_id;

struct struct_subscription {
  uint64_t id;
};


static pthread_t bgp_thread_id;

struct rest_bgp {
  char local_if[BUFSIZ];
  char local_as[BUFSIZ];
  char remote_as[BUFSIZ];
  char remote_ip[BUFSIZ];
  char status[BUFSIZ];
};

static int rest_activated=0;

struct rest_bgp rest_bgp_list[2];

struct rest_bgp bgp_peer[100];
int bgp_count = 0;


struct if_elem {
  char port[BUFSIZ];
  char ip[BUFSIZ];
  char mask[BUFSIZ];
};


struct if_elem if_list[100];
int if_count = 0;


//notification variables

/* Our replay buffer is kept in memory in this example.  It's a circular
 * buffer of struct notif.
 */
#define MAX_BUFFERED_NOTIFS 4
static struct notif replay_buffer[MAX_BUFFERED_NOTIFS];
static unsigned int first_replay_idx = 0;
static unsigned int next_replay_idx = 0;

static struct confd_datetime replay_creation;

#define MAX_REPLAYS 10
struct replay {
    int active;
    int started;
    unsigned int idx;
    struct confd_notification_ctx *ctx;
    struct confd_datetime start;
    struct confd_datetime stop;
    int has_stop;
};
/* Keep tracks of active replays */
static struct replay replay[MAX_REPLAYS];

static int replay_has_aged_out = 0;
static struct confd_datetime replay_aged_time;

/* The notification context (filled in by ConfD) for the live feed */
static struct confd_notification_ctx *live_ctx;





void intHandler(int dummy) {
    int status;
    int status1;
    keepRunning = 0;
    status = pthread_kill( spo_thread_id, SIGUSR1);                                     
    if ( status <  0)                                                              
      perror("pthread_kill failed");
    status1 = pthread_kill( grpc_thread_id, SIGUSR1); 
    if ( status1 <  0)                                                              
      perror("pthread_kill failed");
}


static int GetSock(struct addrinfo *addr_p, enum confd_sock_type sock_type) {
    int sock;

    if ((sock =
         socket(addr_p->ai_family, addr_p->ai_socktype, addr_p->ai_protocol)) < 0)
        return -1;
    if (confd_connect(deamon_p, sock, sock_type,
                      addr_p->ai_addr, addr_p->ai_addrlen) != CONFD_OK) {
        close(sock);
        return -1;
    }
    return sock;
}

#define GetCtrlSock(addr_p) GetSock(addr_p, CONTROL_SOCKET)
#define GetWorkerSock(addr_p) GetSock(addr_p, WORKER_SOCKET)


int GetCdbSock(struct sockaddr_in *addr_p, enum cdb_sock_type s_type) {
  int sock;
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  if (cdb_connect(sock, s_type, (struct sockaddr *)addr_p,
                 sizeof(struct sockaddr_in)) != CONFD_OK) {
    close(sock);
    return -1;
  }
  return sock;
}

#define GetCdbDataSock(addr_p) GetCdbSock(addr_p, CDB_DATA_SOCKET)
#define GetCdbSubSock(addr_p) GetCdbSock(addr_p, CDB_SUBSCRIPTION_SOCKET)

static int GetMaapiSock(struct sockaddr_in *addr_p) {
    int maapi_sock;
    struct confd_ip ip;

    if ((maapi_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
        confd_fatal("Failed to open socket\n");

    if (maapi_connect(maapi_sock, (struct sockaddr*)addr_p,
                      sizeof (struct sockaddr_in)) < 0)
        confd_fatal("Failed to confd_connect() to confd \n");

    ip.af = AF_INET;
    inet_pton(AF_INET, confd_addr, &ip.ip.v4);

    OK(maapi_start_user_session(maapi_sock, user, context, groups, 1,
                                &ip, CONFD_PROTO_TCP));

    return maapi_sock;
}


static void getdatetime(struct confd_datetime *datetime)
{
    struct tm tm;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    gmtime_r(&tv.tv_sec, &tm);

    memset(datetime, 0, sizeof(*datetime));
    datetime->year = 1900 + tm.tm_year;
    datetime->month = tm.tm_mon + 1;
    datetime->day = tm.tm_mday;
    datetime->sec = tm.tm_sec;
    datetime->micro = tv.tv_usec;
    datetime->timezone = 0;
    datetime->timezone_minutes = 0;
    datetime->hour = tm.tm_hour;
    datetime->min = tm.tm_min;
}



void bcSender(char const* const time, char const* const message)
{
    char command[BUFSIZ];
    sprintf(command,"python2.7 %sbcSender.py -p %d -i %s -t \"%s\" -m \"%s\"", BCPATH, BCPORT, BCIP, time, message);
    fprintf(stderr, "LOG - BC communication performed:\n ->time=%s,\n ->message=%s\n", time, message);
    system(command);
}



void logx(int type, char const* const message)
{
    struct timeval tmnow;
    struct tm *tm;
    char buf[30], usec_buf[6];
    gettimeofday(&tmnow, NULL);
    tm = localtime(&tmnow.tv_sec);
    strftime(buf,30,"%Y:%m:%dT%H:%M:%S", tm);
    strcat(buf,".");
    sprintf(usec_buf,"%dZ",(int)tmnow.tv_usec);
    strcat(buf,usec_buf);
    double tstamp = (tmnow.tv_sec)*1000 + (tmnow.tv_usec)/1000;


    logger = fopen(LOGFILE,"a");
    if(logger == NULL){
      printf("Error!");
      exit(1);
    }

    if (type==0){
      fprintf(logger,"%f\t%s\tAdd connection command received\n",tstamp, buf);
      if (BC_ENABLED)
         bcSender(buf, "OC NETCONFAgent: Add connection command received");
    }

    if (type==1){
      fprintf(logger,"%f\t%s\tDelete connection command received\n", tstamp, buf);
      if (BC_ENABLED)
         bcSender(buf, "OC NETCONFAgent: Delete connection command received");
    }
    if (type==2){
      fprintf(logger,"%f\t%s\t%s\n", tstamp, buf, message);
      if (BC_ENABLED)
         bcSender(buf, message);
    }
    fclose(logger);
}







///////////////////////////////////////////////
// Split a string based on a delimiter.
// The result is returned in ret_str which
// is an array of strings.
// The function returns the number of strings
// that has been found.
////////////////////////////////////////////////
static int string_split(char ret_str[100][1000], char * input_str, char * delimiter_str) {
    char *token = strtok(input_str, delimiter_str);
    int jj=0;
    while(token) {
        strcpy(ret_str[jj], token);
        jj++;
        token = strtok(NULL, delimiter_str);
    }
    return jj;
}




static int datetime_le(struct confd_datetime *a, struct confd_datetime *b)
{
    unsigned int ax, bx;
    if (a->year < b->year) { return 1; }
    if (a->year>b->year) { return 0;}
    if (a->month<b->month) { return 1; }
    if (a->month>b->month) { return 0;}
    if (a->day<b->day) { return 1;}
    if (a->day>b->day) { return 0;}
    ax = a->hour - a->timezone;
    bx = b->hour - b->timezone;
    if (ax<bx) { return 1;}
    if (ax>bx) { return 0;}
    ax = a->min - a->timezone_minutes;
    bx = b->min - b->timezone_minutes;
    if (ax<bx) { return 1;}
    if (ax>bx) { return 0;}
    if (a->sec<b->sec) { return 1;}
    if (a->sec>b->sec) { return 0;}
    if (a->micro<b->micro) { return 1;}
    if (a->micro>b->micro) { return 0;}
    return 1;
}



static int continue_replay(struct replay *r)
{
    int done = 0;

    if (!r->started) {
        /* search for first notif to send */
        r->idx = first_replay_idx;
        do {
            if (datetime_le(&r->start, &replay_buffer[r->idx].eventTime))
                break;
            r->idx = (r->idx + 1) % MAX_BUFFERED_NOTIFS;
        } while (r->idx != next_replay_idx);
        r->started = 1;
    }

    /* send one until stop time */
    if (!r->has_stop ||
        datetime_le(&replay_buffer[r->idx].eventTime, &r->stop)) {
        printf("sending notif %d\n", r->idx);
        OK(confd_notification_send(r->ctx,
                                   &replay_buffer[r->idx].eventTime,
                                   replay_buffer[r->idx].vals,
                                   replay_buffer[r->idx].nvals));
        r->idx = (r->idx + 1) % MAX_BUFFERED_NOTIFS;
        if (r->idx == next_replay_idx)
            done = 1;
    } else
        done = 1;

    if (done) {
        /* tell ConfD we're done with replay */
        OK(confd_notification_replay_complete(r->ctx));
        r->active = 0;
    }
    return CONFD_OK;
}


/*
static int cdb_conn_get_value(confd_value_t *ret_val_ptr, char const* const path) {
    struct sockaddr_in addr;
    int rsock, st = CONFD_OK;

    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CONFD_PORT);

    if ((rsock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
        return CONFD_ERR;
    if (cdb_connect(rsock, CDB_READ_SOCKET, (struct sockaddr*)&addr,
                    sizeof (struct sockaddr_in)) < 0)
        return CONFD_ERR;
    if (cdb_start_session(rsock, CDB_RUNNING) != CONFD_OK)
        return CONFD_ERR;

    // set to transponder namespace
    cdb_set_namespace(rsock, oc_opt_term__ns);
    
    //st = cdb_get_str(rsock, buf, BUFSIZ, tmppath); 
    st = cdb_get(rsock, ret_val_ptr, path);
    //st = cdb_get_decimal64(rsock, ret_val_ptr, path);

    cdb_end_session(rsock),
    cdb_close(rsock);
    return st;
}
*/

int updateDb(char *path, char *value_str) {
    int transaction_id;

    if ((transaction_id = maapi_start_trans(maapis, CONFD_RUNNING, CONFD_READ_WRITE)) < 0) {
        confd_fatal("failed to start transition \n");
        return -1;
    }

    maapi_set_elem2(maapis, transaction_id, value_str, path);

    if (maapi_apply_trans(maapis, transaction_id, 0) != CONFD_OK) {
        confd_fatal("failed to apply the transition \n");
        return -1;
    }

    maapi_finish_trans(maapis,transaction_id);

    return 0;
}


int updateDbVals(int vals, char paths[100][1000], char values_str[100][1000]) {
    int transaction_id;
    int i;
    if ((transaction_id = maapi_start_trans(maapis, CONFD_RUNNING, CONFD_READ_WRITE)) < 0) {
        confd_fatal("failed to start transition \n");
        return -1;
    }
    for(i = 0; i < vals; i++ ){
       maapi_set_elem2(maapis, transaction_id, values_str[i], paths[i]);
    }

    if (maapi_apply_trans(maapis, transaction_id, 0) != CONFD_OK) {
          confd_fatal("failed to apply the transition \n");
          return -1;
    }
    maapi_finish_trans(maapis,transaction_id);

    return 0;
}



static void* copythread(void *vargp) {
    struct copy *thr_args = vargp;
    char rootx[BUFSIZ];
    strcpy(rootx, thr_args->root);
    
    //strcpy(value_str, thr_args->value);   
    //confd_value_t ret_val;
    //int i = 0;
    usleep( 2000000 );
    int transaction_id;

    if (strcmp(rootx,"terminal-device")==0 ){
        int index=thr_args->index;
    
        if ((transaction_id = maapi_start_trans(maapis, CONFD_RUNNING, CONFD_READ_WRITE)) < 0) {
                confd_fatal("failed to start transition \n");
                //return -1;
        }
        char from[BUFSIZ], to[BUFSIZ];// pathx[BUFSIZ];
        //confd_value_t ret_val;
        sprintf(from,"/terminal-device/logical-channels/channel{%d}/config/", index);
        sprintf(to,"/terminal-device/logical-channels/channel{%d}/state/", index);
        maapi_copy_tree(maapis, transaction_id, from, to);


        if (maapi_apply_trans(maapis, transaction_id, 0) != CONFD_OK) {
            confd_fatal("failed to apply the transition \n");
            //return -1;
        }
        maapi_finish_trans(maapis,transaction_id);
        if ((transaction_id = maapi_start_trans(maapis, CONFD_RUNNING, CONFD_READ_WRITE)) < 0) {
                confd_fatal("failed to start transition \n");
                //return -1;
        }
        copy_thread_td_activated=0;
    }else
    if (strcmp(rootx,"components")==0 ){
        usleep( 500000 );
        char namex[BUFSIZ];
        strcpy(namex, thr_args->name);
    
        if ((transaction_id = maapi_start_trans(maapis, CONFD_RUNNING, CONFD_READ_WRITE)) < 0) {
                confd_fatal("failed to start transition \n");
                //return -1;
        }
        char from[BUFSIZ], to[BUFSIZ];// pathx[BUFSIZ];
        //confd_value_t ret_val;
        sprintf(from,"/components/component{%s}/optical-channel/config/", namex);
        sprintf(to,"/components/component{%s}/optical-channel/state/", namex);
        maapi_copy_tree(maapis, transaction_id, from, to);


        if (maapi_apply_trans(maapis, transaction_id, 0) != CONFD_OK) {
            confd_fatal("failed to apply the transition \n");
            //return -1;
        }
        maapi_finish_trans(maapis,transaction_id);
        if ((transaction_id = maapi_start_trans(maapis, CONFD_RUNNING, CONFD_READ_WRITE)) < 0) {
                confd_fatal("failed to start transition \n");
                //return -1;
        }
        copy_thread_c_activated=0;
    }

        /*
        //get direction
        sprintf(pathx,"/xponder-ne/transponder{%d}/config/direction/", tp_id);
        if (cdb_conn_get_value(&ret_val,pathx) != CONFD_OK) {
        //if (cdb_conn_get_value(&ret_val,"/finite-state-machine/states/state[0]/events/event[0]/reaction/operation[0]/simple/local-address") != CONFD_OK) {
            perror("error getting direction");
        }
        char direction[BUFSIZ];
        confd_pp_value(direction, BUFSIZ, &ret_val);
        fprintf(stderr, "Value of direction: %s \n", direction);
        if ((strcmp(direction,"enum<1>")==0) || (strcmp(direction,"enum<2>")==0)){
            char path[BUFSIZ];
            //osnr
            sprintf(path,"/xponder-ne/transponder{%d}/state/receiver/osnr/", tp_id);
            maapi_create(maapis, transaction_id, path);
            maapi_set_elem2(maapis, transaction_id, "0.0", path);
            //pmd
            sprintf(path,"/xponder-ne/transponder{%d}/state/receiver/pmd/", tp_id);
            maapi_create(maapis, transaction_id, path);
            maapi_set_elem2(maapis, transaction_id, "0.0", path);
            //cd
            sprintf(path,"/xponder-ne/transponder{%d}/state/receiver/cd/", tp_id);
            maapi_create(maapis, transaction_id, path);
            maapi_set_elem2(maapis, transaction_id, "0.0", path);
            //snr
            sprintf(path,"/xponder-ne/transponder{%d}/state/receiver/snr/", tp_id);
            maapi_create(maapis, transaction_id, path);
            maapi_set_elem2(maapis, transaction_id, "0.0", path);
            //q-factor
            sprintf(path,"/xponder-ne/transponder{%d}/state/receiver/q-factor/", tp_id);
            maapi_create(maapis, transaction_id, path);
            maapi_set_elem2(maapis, transaction_id, "0.0", path);
            //input-power
            //sprintf(path,"/xponder-ne/transponder{%d}/state/receiver/input-power/", tp_id);
            //maapi_create(maapis, transaction_id, path);
            //maapi_set_elem2(maapis, transaction_id, "0.0",path);
            //pre-fec-ber
            sprintf(path,"/xponder-ne/transponder{%d}/state/receiver/pre-fec-ber/", tp_id);
            maapi_create(maapis, transaction_id, path);
            maapi_set_elem2(maapis, transaction_id, "0.0", path);
        }
        
        if (maapi_apply_trans(maapis, transaction_id, 0) != CONFD_OK) {
            confd_fatal("failed to apply the transition \n");
            //return -1;
        }

        maapi_finish_trans(maapis,transaction_id);
        */
        
        
        //return 0;
   
    return NULL;
    //else
    //    return -1;
}





//notification block




/* Try to start a new replay.  This function just allocates a replay
 * entry; no notifications are sent from this callback.  Notifications
 * are sent from the main poll loop.
 */
static int start_replay(struct confd_notification_ctx *nctx,
                        struct confd_datetime *start,
                        struct confd_datetime *stop)
{
    int rnum;

    for (rnum = 0; rnum < MAX_REPLAYS; rnum++) {
        if (!replay[rnum].active) {
            replay[rnum].active = 1;
            replay[rnum].started = 0;
            replay[rnum].idx = first_replay_idx;
            replay[rnum].ctx = nctx;
            replay[rnum].start = *start;
            if (stop) {
                replay[rnum].has_stop = 1;
                replay[rnum].stop = *stop;
            } else
                replay[rnum].has_stop = 0; /* stop when caught up to live */
            return CONFD_OK;
        }
    }
    confd_notification_seterr(nctx, "Max no. of replay requests reached");
    return CONFD_ERR;
}




static void send_notification(confd_tag_value_t *vals, int nvals)
{
    int sz;
    struct confd_datetime now;
    struct notif *notif;

    getdatetime(&now);
    notif = &replay_buffer[next_replay_idx];
    if (notif->vals) {
        /* we're aging out this notification */
        replay_has_aged_out = 1;
        replay_aged_time = notif->eventTime;
        first_replay_idx = (first_replay_idx + 1) % MAX_BUFFERED_NOTIFS;
        free(notif->vals);
    }
    notif->eventTime = now;
    sz = nvals * sizeof(confd_tag_value_t);
    notif->vals = malloc(sz);
    memcpy(notif->vals, vals, sz);
    notif->nvals = nvals;
    next_replay_idx = (next_replay_idx + 1) % MAX_BUFFERED_NOTIFS;
    OK(confd_notification_send(live_ctx,
                               &notif->eventTime,
                               notif->vals,
                               notif->nvals));
}


static void send_notif_failure(char const* const port, char const* const reason, char const* const lev)
{
    confd_tag_value_t vals[5];
    struct confd_datetime time;
    getdatetime(&time);
    int i = 0;
    /*
    char state_ch[BUFSIZ];
    if (strcmp(state,"enum<1>") == 0)
       strcpy(state_ch, "UP");
    else if (strcmp(state,"enum<2>") == 0)
       strcpy(state_ch, "DOWN");
    else if (strcmp(state,"enum<2>") == 0)
       strcpy(state_ch, "DEGRADED");
    else
       strcpy(state_ch, state);
    */
    CONFD_SET_TAG_XMLBEGIN(&vals[i],  oc_opt_term_failure,  oc_opt_term__ns);                i++;
    //CONFD_SET_TAG_DATETIME(&vals[i],  org_openroadm_device_date_time,    time);                      i++;
    CONFD_SET_TAG_STR(&vals[i],       oc_opt_term_port,         port);                  i++;
    CONFD_SET_TAG_STR(&vals[i],       oc_opt_term_reason,       reason);                i++;
    CONFD_SET_TAG_STR(&vals[i],       oc_opt_term_level,        lev);                   i++;
    CONFD_SET_TAG_XMLEND(&vals[i],    oc_opt_term_failure,  oc_opt_term__ns);           i++;
    send_notification(vals, i);
}


static void send_notif_db_change(char const* const port, char const* const elem, char const* const old, char const* const new)
{
    confd_tag_value_t vals[6];
    struct confd_datetime time;
    getdatetime(&time);
    int i = 0;
    /*
    char state_ch[BUFSIZ];
    if (strcmp(state,"enum<1>") == 0)
       strcpy(state_ch, "UP");
    else if (strcmp(state,"enum<2>") == 0)
       strcpy(state_ch, "DOWN");
    else if (strcmp(state,"enum<2>") == 0)
       strcpy(state_ch, "DEGRADED");
    else
       strcpy(state_ch, state);
    */
    CONFD_SET_TAG_XMLBEGIN(&vals[i],  oc_opt_term_db_change,  oc_opt_term__ns);           i++;
    //CONFD_SET_TAG_DATETIME(&vals[i],  org_openroadm_device_date_time,    time);                      i++;
    CONFD_SET_TAG_STR(&vals[i],       oc_opt_term_port,            port);                 i++;
    CONFD_SET_TAG_STR(&vals[i],       oc_opt_term_element,         elem);                 i++;
    CONFD_SET_TAG_STR(&vals[i],       oc_opt_term_old_value,       old);                  i++;
    CONFD_SET_TAG_STR(&vals[i],       oc_opt_term_new_value,       new);                  i++;
    CONFD_SET_TAG_XMLEND(&vals[i],    oc_opt_term_db_change,  oc_opt_term__ns);           i++;
    send_notification(vals, i);
}

static int log_times(struct confd_notification_ctx *nctx)
{
    struct confd_datetime *aged;

    if (replay_has_aged_out)
        aged = &replay_aged_time;
    else
        aged = NULL;

    return confd_notification_reply_log_times(nctx, &replay_creation, aged);
}




static void register_mellanox_stream_notification(struct confd_daemon_ctx *dctx_ptr) {
    struct confd_notification_stream_cbs ncb;
    memset(&ncb, 0, sizeof(ncb));
    ncb.fd = workersock;
    ncb.get_log_times = log_times;
    ncb.replay = start_replay;
    strcpy(ncb.streamname, "mellanox");
    ncb.cb_opaque = NULL;
    if (confd_register_notification_stream(dctx_ptr, &ncb, &live_ctx) != CONFD_OK) {
        confd_fatal("Couldn't register stream %s\n", ncb.streamname);
    }
}


static int cdb_conn_get_value(confd_value_t *ret_val_ptr, char const* const path) {
    struct sockaddr_in addr;
    int rsock, st = CONFD_OK;

    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CONFD_PORT);

    if ((rsock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
        return CONFD_ERR;
    if (cdb_connect(rsock, CDB_READ_SOCKET, (struct sockaddr*)&addr,
                    sizeof (struct sockaddr_in)) < 0)
        return CONFD_ERR;
    if (cdb_start_session(rsock, CDB_RUNNING) != CONFD_OK)
        return CONFD_ERR;

    // set to transponder namespace
    cdb_set_namespace(rsock, oc_prt__ns);

    //st = cdb_get_str(rsock, buf, BUFSIZ, tmppath);
    st = cdb_get(rsock, ret_val_ptr, path);
    //st = cdb_get_decimal64(rsock, ret_val_ptr, path);

    cdb_end_session(rsock),
    cdb_close(rsock);
    return st;
}



static void callbacks_registration(struct confd_daemon_ctx *dctx_ptr) {
    // -- REGISTER NOTIFICATION Callback -----
    //register_transponder_stream_notification(dctx_ptr);
    register_mellanox_stream_notification(dctx_ptr);

    //----- REGISTER RPC Callback -----
    //register_outage_record_rpc(dctx_ptr);
    //register_telemetry_subscribe_rpc(dctx_ptr);

    // finish to register callbacks
    if (confd_register_done(dctx_ptr) != CONFD_OK)  {
        confd_fatal("Failed to complete callback registration \n");
    }
}


//GRPC
/*
struct struct_subscription {
  uint64_t id;
};
*/
void *grpc_server(void *vargp) {

    char command[BUFSIZ];
    sprintf(command,"pkill -9 python");
    system(command);
    sprintf(command,"python2.7 %sserver_telemetry3.0.py %s %s", GRPCPATH, GRPC_SERVER_PORT, GRPC_SERVER_IP);
    //sprintf(commandtx,"python %snetconf-console --proto=tcp --port=2023 --host=%s --user=admin --password=admin --edit-conf  '%s'", local_path, r_addr,"/conf.xml");
    system(command);
    return NULL;
}
/*
struct subscription {
  char port[BUFSIZ];
  char sample_interval[BUFSIZ];
  char heartbeat_interval[BUFSIZ];
  char suppress_redundant[BUFSIZ];
  char destination_address[BUFSIZ];
  char path[BUFSIZ];
  uint8_t init;
};
subscribe inputs
  s           list of ip:port splitted by # 10.30.2.24:50082#10.30.2.24:50083
  k           list of paths splitted by # /terminal-device/logical-
              channels/channel[index=11811]/otn/state/pre-fec-ber/instant
  l           Telemetry server IP:port 10.30.2.37:50051
  c           1=avoid duplicated data
  i           interval in seconds

*/

void *suscribe_grpc(void *vargp) {

    struct struct_subscription *thr_args = vargp;
    uint64_t idy=thr_args->id;
    char command[BUFSIZ];
    usleep( 1000000 );

    if ((strcmp(subscriptions[idy].protocol,"STREAM_GRPC") == 0)&&(strcmp(subscriptions[idy].encoding,"ENC_PROTO3") == 0)){
        if(strcmp(subscriptions[idy].suppress_redundant,"true") == 0){
            sprintf(command,"python2.7 %ssubscribeTelemetry3.0.py %s:%s %s %s:%s 1 %s %s %" PRIu64, GRPCPATH, subscriptions[idy].destination_address, subscriptions[idy].port, subscriptions[idy].path, GRPC_SERVER_IP, GRPC_SERVER_PORT, subscriptions[idy].heartbeat_interval, subscriptions[idy].sample_interval, idy);
        } 
        else{
            sprintf(command,"python2.7 %ssubscribeTelemetry3.0.py %s:%s %s %s:%s 0 %s %s %" PRIu64, GRPCPATH, subscriptions[idy].destination_address, subscriptions[idy].port, subscriptions[idy].path, GRPC_SERVER_IP, GRPC_SERVER_PORT, subscriptions[idy].heartbeat_interval, subscriptions[idy].sample_interval, idy);
        }
        //sprintf(commandtx,"python %snetconf-console --proto=tcp --port=2023 --host=%s --user=admin --password=admin --edit-conf  '%s'", local_path, r_addr,"/conf.xml");
        //fprintf(stderr, "Command: %s\n", command);
        system(command);

    }
    else{
        fprintf(stderr, "Not GRPC or Proto buf v3\n");
    }
    return NULL;
}

/*
struct rest_bgp {
  char local_if[BUFSIZ];
  char local_as[BUFSIZ];
  char remote_as[BUFSIZ];
  char remote_ip[BUFSIZ];
  char status[BUFSIZ];
};


struct rest_bgp bgp_peer[100];
int bgp_count = 0;

*/


void add_bgp(char const* const l_as, char const* const r_as, char const* const ip){
   int count = bgp_count;
   if (bgp_count > 0){
      for (int i = 0; i< bgp_count; i++){
         if (strcmp(bgp_peer[i].local_as, l_as) == 0){
            if (strcmp(bgp_peer[i].remote_as, r_as) == 0){
               if (strcmp(bgp_peer[i].remote_ip, ip) == 0){
                  count = i;
                  fprintf(stderr, "BGP already exixts\n");
                  break;
               }

            }
         }
      }
   }
   strcpy(bgp_peer[count].remote_as, r_as);
   strcpy(bgp_peer[count].remote_ip, ip);
   strcpy(bgp_peer[count].local_as, l_as);
   if (count == bgp_count)
        bgp_count = bgp_count + 1;
}

void  del_bgp(char const* const ip){
   int found = 0;
   if (bgp_count > 0){
      for (int i = 0; i< bgp_count; i++){
         if (strcmp(bgp_peer[i].remote_ip, ip) == 0){
            bgp_config(0, bgp_peer[i].local_as, bgp_peer[i].remote_as, bgp_peer[i].remote_ip, NULL);
            strcpy(bgp_peer[i].local_as, "");
            strcpy(bgp_peer[i].remote_as, "");
            strcpy(bgp_peer[i].remote_ip, "");
            found = 1;
            break;

         }
      }
   }
   if (found ==0)
      bgp_config(0, "64545", "64545", ip, NULL);
}





void *send_bgp_rest(void *vargp) {
    usleep( 700000 );
    if (strcmp(rest_bgp_list[0].status,"true") == 0){
       if (strcmp(rest_bgp_list[0].remote_as,"") == 1){
          bgp_config(1, rest_bgp_list[0].local_as, rest_bgp_list[0].remote_as, rest_bgp_list[0].remote_ip, rest_bgp_list[0].local_if);
       }
       else{
          /*
/bgp-instance/bgp/neighbors/neighbor{192.168.2.1}/config/neighbor-if
/bgp-instance/bgp/neighbors/neighbor{192.168.2.1}/config/peer-as
/bgp-instance/bgp/neighbors/neighbor{192.168.2.1}/config/local-as
          */
          char pathx[BUFSIZ];
          confd_value_t ret_val;
          //get local_as
          sprintf(pathx,"/bgp-instance/bgp/neighbors/neighbor{%s}/config/local-as", rest_bgp_list[0].remote_ip);
          if (cdb_conn_get_value(&ret_val,pathx) != CONFD_OK) {
             perror("error getting local as");
          }
          char l_as[BUFSIZ];
          confd_pp_value(l_as, BUFSIZ, &ret_val);
          //get peer_as
          sprintf(pathx,"/bgp-instance/bgp/neighbors/neighbor{%s}/config/peer-as", rest_bgp_list[0].remote_ip);
          if (cdb_conn_get_value(&ret_val,pathx) != CONFD_OK) {
             perror("error getting peer as");
          }
          char r_as[BUFSIZ];
          confd_pp_value(r_as, BUFSIZ, &ret_val);
          //get local_as
          sprintf(pathx,"/bgp-instance/bgp/neighbors/neighbor{%s}/config/neighbor-if", rest_bgp_list[0].remote_ip);
          if (cdb_conn_get_value(&ret_val,pathx) != CONFD_OK) {
             perror("error getting if");
          }
          char itf[BUFSIZ];
          confd_pp_value(itf, BUFSIZ, &ret_val);
          bgp_config(1, l_as, r_as, rest_bgp_list[0].remote_ip, itf);
       }
    } else
    if (strcmp(rest_bgp_list[0].status,"false") == 0){
       bgp_config(0, NULL, NULL, rest_bgp_list[0].remote_ip, NULL);
    }
    rest_activated = 0;

    strcpy(rest_bgp_list[0].remote_as, "");
    strcpy(rest_bgp_list[0].local_as, "");
    strcpy(rest_bgp_list[0].remote_ip, "");
    strcpy(rest_bgp_list[0].local_if, "");
    strcpy(rest_bgp_list[0].status, "");

}



void remove_spaces(char* s) {
    const char* d = s;
    do {
        while (*d == ' ') {
            ++d;
        }
    } while (*s++ = *d++);
}


void remove_endline(char* s) {
    const char* d = s;
    do {
        while (*d == '\n') {
            ++d;
        }
    } while (*s++ = *d++);
}


int exec_command(char value[4096], char * command ){
  char ss[100][4096];
  char temp[4096];
  FILE *fp;
  /* Open the command for reading. */
  fp = popen(command, "r");
  int j=0;
  if (fp == NULL) {
    printf("Failed to run the command\n" );
    return 1;
  }
  /* Read the output a line at a time - output it. */
  while (fgets(temp, sizeof(temp), fp) != NULL) {
    strcpy(ss[j], temp);
    j++;
  }
  /* close */
  pclose(fp);
  remove_endline(ss[0]);
  if(value != NULL)
     strcpy(value, ss[0]);
  return 0;
}

/*
struct if_elem {
  char port[BUFSIZ];
  char ip[BUFSIZ];
  char mask[BUFSIZ];
};


struct if_elem if_list[100];
int if_count = 0;

*/

void add_if(char const* const portname, char const* const ip, char const* const mask){
   int count = if_count;
   if (if_count > 0){
      for (int i = 0; i< if_count; i++){
         if (strcmp(if_list[i].port, portname) == 0){
            if (strcmp(if_list[i].ip, ip) == 0){
               count = i;
               fprintf(stderr, "interface %s found\n", portname);
               break;
               
            }
         }
      }
   }
   strcpy(if_list[count].port, portname);
   strcpy(if_list[count].ip, ip);
   strcpy(if_list[count].mask, mask);
   if (count == if_count)
          if_count = if_count + 1;
}

void  del_if(char const* const portname, char const* const ip){
   int found = 0;
   if (if_count > 0){
      for (int i = 0; i< if_count; i++){
         if (strcmp(if_list[i].port, portname) == 0){
            if (strcmp(if_list[i].ip, ip) == 0){
               intf_config(0, ip, if_list[i].mask, portname, NULL);
               found = 1;
               fprintf(stderr, "interface %s found\n", portname);
               strcpy(if_list[i].port, "");
               strcpy(if_list[i].ip, "");
               strcpy(if_list[i].mask, "");
               break;

            }
         }
      }
   }
   if (found ==0)
      intf_config(0, ip, "30", portname, NULL);
}



// Function called each time a database change occurs
static enum cdb_iter_ret Iter(confd_hkeypath_t *kp,
                              enum cdb_iter_op operation,
                              confd_value_t *oldv,
                              confd_value_t *newv,
                              void *state) {
    char buf[BUFSIZ];
    //int cdbsock = *((int *)state);
    confd_pp_kpath(buf, BUFSIZ, kp);
    // keypath is
    // /terminal-device/logical-channels/channel{$key}/config/....
    //        len-1         -2            -3      -4    -5
    // /components/component{$key}/optical-channel/config/...
    //    len-1       -2      -3       -4            -5
    ///switched-vlans/vlan{100}/untagged-members/member{port-10}
    //    len-1       -2   -3       -4            -5      -6
    switch (operation) {
                case MOP_CREATED: {
                    fprintf(stderr, "Created %s\n", buf);
                    confd_value_t *root = &kp->v[kp->len-1][0];
                    char root_char[BUFSIZ];
                    confd_pp_value(root_char, BUFSIZ, root);
                    //vlan
                    if (strcmp(root_char,"switched-vlans") == 0){
                      if (kp->len==6){
                          char command[2000];
                          uint8_t vlan = CONFD_GET_UINT8(&kp->v[kp->len-3][0]);
                          fprintf(stderr, "Create: %s\n", buf);
                          char portname[BUFSIZ];
                          strcpy(portname, (char*)CONFD_GET_BUFPTR(&kp->v[kp->len-6][0]));
                          confd_value_t *member_type = &kp->v[kp->len-4][0];
                          char member_type_char[BUFSIZ];
                          confd_pp_value(member_type_char, BUFSIZ, member_type);
                          if (strcmp(member_type_char,"untagged-members") == 0){
                             fprintf(stderr, "Vlan config-> id:%d; port:%s; type:untagged\n", vlan, portname);
			     //send REST PUT to configure the VLAN
                             sprintf(command,"/vlans/%d/%s/1", vlan, portname);
                             //sprintf(command,"/vlans/%d/1/1", vlan);
                             if (CONFSONIC == 1 ){
                                sender("PUT", command, NULL, RESTSONIC_PORT, RESTSONIC_IP);
                                //sprintf(command,"python2.7 %s/%s %d %s 1",SCRIPT_PATH, VSET, vlan, portname);
                                //int res= exec_command(NULL, command);
                                fprintf(stderr, "Vlan command sent\n");
                             }
                          }
                          else{
                             fprintf(stderr, "Vlan config-> id:%d; port:%s; type:tagged\n", vlan, portname);
			     //send REST PUT to configure the VLAN
                             sprintf(command,"/vlans/%d/%s/0", vlan, portname);
                             //sprintf(command,"/vlans/%d/1/0", vlan);
                             if(CONFSONIC==1)
                                 sender("PUT", command, NULL, RESTSONIC_PORT, RESTSONIC_IP);
                             fprintf(stderr, "Vlan command sent\n");
                          }
                       }
                    }
                }
                break;
                case MOP_MODIFIED: {
                        fprintf(stderr, "Modified %s\n", buf);
                }
                break;
                case MOP_VALUE_SET: {
                    confd_value_t *root = &kp->v[kp->len-1][0];
                    //conversion to string of new value
                    char newval[BUFSIZ];
                    confd_pp_value(newval, BUFSIZ, newv);
                    fprintf(stderr, "Value Set: %s --> (%s)\n", buf, newval);
                    //get root value
                    char root_char[BUFSIZ];
                    confd_pp_value(root_char, BUFSIZ, root);
                    //case root value is terminal-device
                    if (strcmp(root_char,"terminal-device") == 0){
                        uint32_t lchannel = CONFD_GET_UINT32(&kp->v[kp->len-4][0]);
                        //fprintf(stderr, "Key is t: %d\n", lchannel);
                        //we match the tag of the value that has been set
                        confd_value_t *ctag = &kp->v[kp->len-6][0];
                        switch (CONFD_GET_XMLTAG(ctag)) {
                            //admin-state
                            case oc_opt_term_admin_state:  {
                                //if TX
                                if (strcmp(newval,"enum<0>") == 0){
                                    char msg[BUFSIZ];
                                    sprintf(msg,"OC NETCONFAgent: Received the admin-state configuration: tp=%d, admin-state=ENABLE", lchannel);
                                    logx(2,msg);
                                    int i;
                                    for(i = 0; i < numMonPorts; i++ ){
                                       if (monPorts[i].type == 1){
                                          if(CONF_TRANSPONDER==1){
                                               if(monPorts[i].port_id==lchannel){
                                                   uint32_t valx = atoi(monPorts[i].notes);
                                                   lch_set_admin_state(valx,"ENABLE");
                                                   monPorts[i].config = 1;
                                               }
					  }
                                       }
                                       if (monPorts[i].type == 2){
                                          if (CONFSONIC == 1 ){
                                               if(monPorts[i].port_id==lchannel){
                                                  char command[2000];
                                                  sprintf(command,"/ports/%d", lchannel);
                                                  sender("PUT", command, NULL, RESTSONIC_PORT, RESTSONIC_IP);
                                                  break;
                                               }
                                          }
                                       }
                                    }
                                }
                                else if (strcmp(newval,"enum<1>") == 0){
                                    char msg[BUFSIZ];
                                    sprintf(msg,"OC NETCONFAgent: Received the admin-state configuration: tp=%d, admin-state=DISABLE", lchannel);
                                    logx(2,msg);
                                    int i;
                                    for(i = 0; i < numMonPorts; i++ ){
                                       if (monPorts[i].type == 1){
                                          if(CONF_TRANSPONDER==1){
                                               if(monPorts[i].port_id==lchannel){
                                                   uint32_t valx = atoi(monPorts[i].notes);
                                                   lch_set_admin_state(valx,"DISABLE");
                                                   monPorts[i].config = 0;
                                               }
                                          }
                                       }
                                       if (monPorts[i].type == 2){
                                          if (CONFSONIC == 1 ){
                                              if(monPorts[i].port_id==lchannel){
                                                 char command[2000];
                                                 sprintf(command,"/ports/%d", lchannel);
                                                 sender("DELETE", command, NULL, RESTSONIC_PORT, RESTSONIC_IP);
                                                 break;
                                              }
                                          }
                                       }
                                    }
                                }
                                struct copy *thr_args=  malloc(sizeof(struct copy)); 
                                strcpy(thr_args->root, root_char);
                                thr_args->index=lchannel;
                                if (copy_thread_td_activated==0){
                                    copy_thread_td_activated=1;
                                    pthread_create(&copy_pthread, NULL, copythread, thr_args);
                                }
                            }
                            break;
                        }
                    }else
                    //case root value is components
                    if(strcmp(root_char,"components") == 0){
                        char name[BUFSIZ];
                        strcpy(name, (char*)CONFD_GET_BUFPTR(&kp->v[kp->len-3][0]));
                        //fprintf(stderr, "Key is t: %s\n", name);
                        confd_value_t *ctag = &kp->v[kp->len-6][0];
                        switch (CONFD_GET_XMLTAG(ctag)) {
                            //target-output-power
			    
                            case oc_opt_term_target_output_power:  {
                                char msg[BUFSIZ];
                                sprintf(msg,"OC NETCONFAgent: Received output-power configuration: tp=%s, freq=%s", name, newval);
                                logx(2,msg);
                                /*if(CONF_TRANSPONDER==1)
                                    component_set_target_power(name,newval);
                                */
                                struct copy *thr_args=  malloc(sizeof(struct copy)); 
                                strcpy(thr_args->root, root_char);
                                strcpy(thr_args->name, name);
                                if (copy_thread_c_activated==0){
                                    copy_thread_c_activated=1;
                                    pthread_create(&copy_pthread, NULL, copythread, thr_args);
                                }
                            }
                            break;
                            
                            //frequency
			    case oc_opt_term_frequency:  {
                                char msg[BUFSIZ];
                                sprintf(msg,"OC NETCONFAgent: Received the frequency configuration: tp=%s, freq=%s", name, newval);
                                logx(2,msg);
                                int i;
                                if(CONF_TRANSPONDER==1){
                                   char parts[3][100];
                                   int n = string_splitter(parts, (char *) name,"-");
                                   if (n==2) {
                                      uint32_t lchx = strtoul(parts[1], NULL, 10);
                                      for(i = 0; i < numMonPorts; i++ ){
                                         if (monPorts[i].type == 1){
                                            if(monPorts[i].port_id==lchx){
                                                char namex[BUFSIZ];
                                                sprintf(namex,"channel-%s", monPorts[i].notes);
                                                component_set_frequency(namex,newval);
				            }
				         }
				      }
				   }
				}
                                struct copy *thr_args=  malloc(sizeof(struct copy)); 
                                strcpy(thr_args->root, root_char);
                                strcpy(thr_args->name, name);
                                if (copy_thread_c_activated==0){
                                    copy_thread_c_activated=1;
                                    pthread_create(&copy_pthread, NULL, copythread, thr_args);
                                }
                            }
                            break;
                            //operational-mode
                            case oc_opt_term_operational_mode:  {
                                char msg[BUFSIZ];
                                sprintf(msg,"OC NETCONFAgent: Received op-mode configuration: tp=%s, op=%s", name, newval);
                                logx(2,msg);

                                char oldval[BUFSIZ];
                                confd_pp_value(oldval, BUFSIZ, oldv);
                                if(strcmp(oldval,"0") != 0){
                                   send_notif_db_change(name, "operational-mode", oldval, newval);
                                   if(CONF_TRANSPONDER==1)
                                       component_set_operational_mode(name,newval);
                                }
                            }
                            break;
                        }
                    }
                    /*
    telemetry-system/subscriptions/dynamic-subscriptions/dynamic-subscription[id]/state/destination
    //    len-1          -2               -3                     -4           -5    -6    -7
    telemetry-system/subscriptions/dynamic-subscriptions/dynamic-subscription[id]/sensor-paths/sensor-path[path]/state/path
    //    len-1          -2               -3                     -4           -5    -6             -7       -8    -9    -10

    /telemetry-system/subscriptions/dynamic-subscriptions/dynamic-subscription{1}/state/heartbeat-interval --> (3)
    /telemetry-system/subscriptions/dynamic-subscriptions/dynamic-subscription{1}/state/suppress-redundant --> (true)
    /telemetry-system/subscriptions/dynamic-subscriptions/dynamic-subscription{1}/state/destination-port --> (50082)
    /telemetry-system/subscriptions/dynamic-subscriptions/dynamic-subscription{1}/state/sample-interval --> (300)
    /telemetry-system/subscriptions/dynamic-subscriptions/dynamic-subscription{1}/state/destination-address --> (10.30.2.24)



                    */
                    if(strcmp(root_char,"telemetry-system") == 0){
                        uint64_t id = CONFD_GET_UINT64(&kp->v[kp->len-5][0]);
                        //fprintf(stderr, "ID is: %d\n", id);
                        
                        confd_value_t *ctag = &kp->v[kp->len-6][0];
                        switch (CONFD_GET_XMLTAG(ctag)) {
                            
                            case oc_telemetry_state:  {
                                confd_value_t *ctag2 = &kp->v[kp->len-7][0];
                                switch (CONFD_GET_XMLTAG(ctag2)) {
                                    case oc_telemetry_heartbeat_interval:  {
                                        strcpy(subscriptions[id].heartbeat_interval, newval);
                                    }
                                    break;
                                    case oc_telemetry_suppress_redundant:  {
                                        strcpy(subscriptions[id].suppress_redundant, newval);
                                    }
                                    break;
                                    case oc_telemetry_destination_port:  {
                                        strcpy(subscriptions[id].port, newval);
                                    }
                                    break;
                                    case oc_telemetry_sample_interval:  {
                                        strcpy(subscriptions[id].sample_interval, newval);
                                    }
                                    break;
                                    case oc_telemetry_destination_address:  {
                                        strcpy(subscriptions[id].destination_address, newval);
                                        struct struct_subscription *thr_args1=  malloc(sizeof(struct struct_subscription)); 
                                        thr_args1->id=id;
                                        //fprintf(stderr, "thread enabling subscription %d\n", id);
                                        pthread_create(&grpc_sub_thread_id, NULL, suscribe_grpc, thr_args1);
                                        //fprintf(stderr, "thread enabled\n");
                                    }
                                    break;
                                    case oc_telemetry_encoding:  {
                                        strcpy(subscriptions[id].encoding, newval);
                                    }
                                    break;
                                    case oc_telemetry_protocol:  {
                                        strcpy(subscriptions[id].protocol, newval);
                                    }
                                    break;
                                }

                            }
                            break;
                            case oc_telemetry_sensor_paths:  {
                                confd_value_t *ctag2 = &kp->v[kp->len-9][0];

                                switch (CONFD_GET_XMLTAG(ctag2)) {
                                    case oc_telemetry_state:  {
                                        char path[BUFSIZ];
                                        if (subscriptions[id].init!=1){
                                            //if (strcmp(subscriptions[id].path,"") == 0 ) {
                                            strcpy(path, (char*)CONFD_GET_BUFPTR(&kp->v[kp->len-8][0]));
                                            strcpy(subscriptions[id].path, path);
                                            subscriptions[id].init=1;
                                        }
                                        else{
                                            sprintf(path,"%s#%s", subscriptions[id].path,(char*)CONFD_GET_BUFPTR(&kp->v[kp->len-8][0]));                                    
                                            strcpy(subscriptions[id].path, path);
                                        }
                                        //fprintf(stderr, "The paths are: %s\n", subscriptions[id].path);
                                    }
                                    break;
                                }
                            }
                            break;
                        }
                        
                    } else

/*
/interfaces/interface{eth0}/ipv4/addresses/address{10.0.0.1}/ip
/interfaces/interface{eth0}/ipv4/addresses/address{10.0.0.1}/config/prefix-length
/interfaces/interface{eth0}/ipv4/addresses/address{10.0.0.1}/config/ip
/interfaces/interface{eth0}/ethernet/config/port-speed
 -1            -2      -3    -4      -5      -6       -7      -8


*/
                    //interfaces
                    if (strcmp(root_char,"interfaces") == 0){ 
                       //ip config
                       if (kp->len>8){
                          char newval[BUFSIZ];
                          confd_pp_value(newval, BUFSIZ, newv);
                          char portname[BUFSIZ];
                          strcpy(portname, (char*)CONFD_GET_BUFPTR(&kp->v[kp->len-3][0]));
                          confd_value_t *proto_type = &kp->v[kp->len-4][0];
                          char proto_char[BUFSIZ];
                          confd_pp_value(proto_char, BUFSIZ, proto_type);
                          if (strcmp(proto_char,"ipv4") == 0){
                              confd_value_t *ctag = &kp->v[kp->len-9][0];
                              switch (CONFD_GET_XMLTAG(ctag)) {
                                 case oc_ip_prefix_length:  {
                                    char ip_char[BUFSIZ];
                                    confd_value_t *ip_type = &kp->v[kp->len-7][0];
                                    confd_pp_value(ip_char, BUFSIZ, ip_type);
                                    fprintf(stderr, "Configuring interface %s with IP %s/%s\n", portname, ip_char, newval);
                                    intf_config(1, ip_char, newval, portname, NULL);
                                    add_if(portname, ip_char, newval); 
                                 }
                                 break;
                              }
                          }
                      } else
                      //ethernet config
                      if (kp->len > 5){
                          char newval[BUFSIZ];
                          confd_pp_value(newval, BUFSIZ, newv);
                          char portname[BUFSIZ];
                          strcpy(portname, (char*)CONFD_GET_BUFPTR(&kp->v[kp->len-3][0]));
                          
                          confd_value_t *proto_type = &kp->v[kp->len-4][0];
                          char proto_char[BUFSIZ];
                          confd_pp_value(proto_char, BUFSIZ, proto_type);
                          if (strcmp(proto_char,"ethernet") == 0){
                              confd_value_t *ctag = &kp->v[kp->len-6][0];
                              switch (CONFD_GET_XMLTAG(ctag)) {
                                 case oc_eth_port_speed:  {
                                    fprintf(stderr, "Configuring interface %s with speed %s\n", portname, newval);
                                    intf_config(11, NULL, NULL, portname, newval);
                                 }
                                 break;
                              }

                          }    
                      } else    
                      if (kp->len == 5){
                          char newval[BUFSIZ];
                          confd_pp_value(newval, BUFSIZ, newv);
                          char portname[BUFSIZ];
                          strcpy(portname, (char*)CONFD_GET_BUFPTR(&kp->v[kp->len-3][0]));
                          confd_value_t *proto_type = &kp->v[kp->len-4][0];
                          char proto_char[BUFSIZ];
                          confd_pp_value(proto_char, BUFSIZ, proto_type);
                          if (strcmp(proto_char,"config") == 0){
                              confd_value_t *ctag = &kp->v[kp->len-5][0];
                              switch (CONFD_GET_XMLTAG(ctag)) {
                                 case oc_if_enabled:  {
                                    fprintf(stderr, "Setting interface %s with status %s\n", portname, newval);
                                    if (strcmp(newval,"true") == 0){
                                       if (oldv != NULL) {
                                          char oldval[BUFSIZ];
                                          confd_pp_value(oldval, BUFSIZ, oldv);
                                          if (strcmp(oldval,"false") == 0){
                                             intf_config(21, NULL, NULL, portname, NULL);
                                          } 
                                       }
                                    }
                                    else{
                                        intf_config(20, NULL, NULL, portname, NULL);
                                    }
                                 }
                              }
                          }
                      }
                    } else   
/*
/bgp-instance/bgp/global/config/as
/bgp-instance/bgp/global/config/router-id
/bgp-instance/bgp/global/config/address-family/ipv4-unicast/connected
/bgp-instance/bgp/global/config/address-family/ipv4-unicast/static
   -1         -2    -3    -4      -5             -6          -7


/bgp-instance/bgp/neighbors/neighbor{192.168.2.1}/config/enabled
/bgp-instance/bgp/neighbors/neighbor{192.168.2.1}/config/peer-as
/bgp-instance/bgp/neighbors/neighbor{192.168.2.1}/config/neighbor-address
   -1         -2    -3      -4          -5          -6     -7

static pthread_t bgp_thread_id;

struct rest_bgp {
  char local_if[BUFSIZ];
  char local_as[BUFSIZ];
  char remote_as[BUFSIZ];
  char remote_ip[BUFSIZ];
};

static int rest_activated=0;

struct rest_bgp rest_bgp_list[1];




strcpy(rest_bgp_list[0].local_if, newval);
strcpy(rest_bgp_list[0].local_as, newval);
strcpy(rest_bgp_list[0].remote_as, newval);
strcpy(rest_bgp_list[0].remote_ip, newval);
                                   if (rest_activated == 0)
                                        pthread_create(&bgp_thread_id, NULL, send_bgp_rest, NULL);


*/
                    //bgp
                    if (strcmp(root_char,"bgp-instance") == 0){ 
                          char class_char[BUFSIZ];
                          confd_value_t *class_type = &kp->v[kp->len-3][0];
                          confd_pp_value(class_char, BUFSIZ, class_type);
                          if (strcmp(class_char,"global") == 0){
                             if (kp->len == 5){
                                confd_value_t *ctag = &kp->v[kp->len-5][0];
                                switch (CONFD_GET_XMLTAG(ctag)) {
                                    case oc_bgp_as:  {
                                       fprintf(stderr, "Configuring BGP with AS %s\n", newval);
                                    }
                                    break;
                                    case oc_bgp_router_id: {
                                       fprintf(stderr, "Configuring BGP with router_id %s\n", newval);
                                    }
                                    break;
                                }
                             } else
                             if (kp->len == 7){
                                confd_value_t *ctag = &kp->v[kp->len-7][0];
                                switch (CONFD_GET_XMLTAG(ctag)) {
                                    case oc_bgp_connected:  {
                                       fprintf(stderr, "Configuring BGP connected: %s\n", newval);
                                    }
                                    break;
                                    case oc_bgp_static: {
                                       fprintf(stderr, "Configuring BGP static: %s\n", newval);
                                    }
                                    break;
                                }
                             }
                          } else
                          if (strcmp(class_char,"neighbors") == 0){
                             if (kp->len == 7){
                                confd_value_t *ctag = &kp->v[kp->len-7][0];
                                switch (CONFD_GET_XMLTAG(ctag)) {
                                    case oc_bgp_peer_as:  {
                                       fprintf(stderr, "Configuring BGP peer AS %s\n", newval);
                                       strcpy(rest_bgp_list[0].remote_as, newval);
                                       if (rest_activated == 0){
                                          rest_activated = 1;
                                          pthread_create(&bgp_thread_id, NULL, send_bgp_rest, NULL);
                                       }
                                    }
                                    break;
                                    case oc_bgp_enabled: {
                                       fprintf(stderr, "Configuring BGP neig enabled %s\n", newval);
                                       strcpy(rest_bgp_list[0].status, newval);
                                       //if (strcmp(newval,"false") == 0){
                                          char ip_char[BUFSIZ];
                                          confd_value_t *ip_type = &kp->v[kp->len-5][0];
                                          confd_pp_value(ip_char, BUFSIZ, ip_type);
                                          strcpy(rest_bgp_list[0].remote_ip, ip_char);
                                       //}
                                       if (rest_activated == 0){
                                          rest_activated = 1;
                                          pthread_create(&bgp_thread_id, NULL, send_bgp_rest, NULL);
                                       }
                                    }
                                    break;
                                    /*case oc_bgp_neighbor_address: {
                                       fprintf(stderr, "Configuring BGP neig with address %s\n", newval);
                                       strcpy(rest_bgp_list[0].remote_ip, newval);
                                       if (rest_activated == 0){
                                          rest_activated = 1;
                                          pthread_create(&bgp_thread_id, NULL, send_bgp_rest, NULL);
                                       }
                                    }
                                    break;*/
                                    case oc_bgp_neighbor_if: {
                                       fprintf(stderr, "Configuring BGP neig on if %s\n", newval);
                                       strcpy(rest_bgp_list[0].local_if, newval);
                                       if (rest_activated == 0){
                                          rest_activated = 1;
                                          pthread_create(&bgp_thread_id, NULL, send_bgp_rest, NULL);
                                       }
                                    }
                                    break;
                                    case oc_bgp_local_as: {
                                       fprintf(stderr, "Configuring BGP neig with local AS %s\n", newval);
                                       strcpy(rest_bgp_list[0].local_as, newval);
                                       if (rest_activated == 0){
                                          rest_activated = 1;
                                          pthread_create(&bgp_thread_id, NULL, send_bgp_rest, NULL);
                                       }
                                    }
                                    break;
                                }

                             }
                          }

                    }
                }
                break;
                case MOP_DELETED:{
                    fprintf(stderr, "Delete: %s\n", buf);
                    confd_value_t *root = &kp->v[kp->len-1][0];
                    char root_char[BUFSIZ];
                    confd_pp_value(root_char, BUFSIZ, root);
                    if (strcmp(root_char,"switched-vlans") == 0){
                       if (kp->len==6){
                          char command[2000];
                          uint8_t vlan = CONFD_GET_UINT8(&kp->v[kp->len-3][0]);
                          char portname[BUFSIZ];
                          strcpy(portname, (char*)CONFD_GET_BUFPTR(&kp->v[kp->len-6][0]));
                          fprintf(stderr, "Vlan delete-> id:%d; port:%s; type:untagged\n", vlan, portname);
			  //send REST DELETE to configure the VLAN
                          sprintf(command,"/vlans/%d/%s/0", vlan, portname);
                          //sprintf(command,"/vlans/%d/1/0", vlan);
                          if( CONFSONIC == 1) 
                              sender("DELETE", command, NULL, RESTSONIC_PORT, RESTSONIC_IP);
                          fprintf(stderr, "Vlan delete commenad sent\n");
                       }
 ///interfaces/interface{Ethernet104}/ipv4/addresses/address{192.168.245.1}
                    } else
                    if (strcmp(root_char,"interfaces") == 0){
                       if (kp->len==7){
                          char portname[BUFSIZ];
                          strcpy(portname, (char*)CONFD_GET_BUFPTR(&kp->v[kp->len-3][0]));
                          char ip[BUFSIZ];
                          strcpy(ip, (char*)CONFD_GET_BUFPTR(&kp->v[kp->len-7][0]));
                          fprintf(stderr, "Deleteguring IP %s/30 on port %s\n", ip, portname);
                          del_if(portname, ip);
                       }
///bgp-instance/bgp/neighbors/neighbor{192.168.255.2}
                    }else
                    if (strcmp(root_char,"bgp-instance") == 0){
                       if (kp->len == 5){
                          char ip[BUFSIZ];
                          strcpy(ip, (char*)CONFD_GET_BUFPTR(&kp->v[kp->len-5][0]));
                          fprintf(stderr, "Deleting BGP peer %s\n", ip);
                          del_bgp(ip);
                       }

                    }
                }
                break;
                default:
                    /* We should never get MOP_MOVED_AFTER or MOP_ATTR_SET */
                    fprintf(stderr, "Unexpected operation %d for %s\n", operation, buf);
                      break;
    }
    return ITER_RECURSE;
}

static int strConcat(char *strFinal, char *str1, char *str2, int position) {
    if (position > strlen(str1) || position < 0) return -1;
    strncpy(strFinal,str1, position);
    strFinal[position] = '\0';
    strcat(strFinal,str2);
    strcat(strFinal,str1+position);
    return 0;
}



static int confd_decimal64_to_string(char *strFinal, 
                                    struct confd_decimal64 const * const d64_ptr) {
    char str1[25], str2[25];
    int pos_to_insert;
    //Create the string with the value
    sprintf(str1, "%ld", d64_ptr->value);
    pos_to_insert = strlen(str1) - d64_ptr->fraction_digits;
    if (pos_to_insert<0) {
        strcpy(str2, "0.");
        for (;pos_to_insert<0;pos_to_insert++) {
            strcat(str2, "0");
        }
    } else {
        strcpy(str2, ".");
    }
    strConcat(strFinal,str1,str2,pos_to_insert);
    if (d64_ptr->fraction_digits == 0) {
        strcat(strFinal, "0");
    }

    if (strFinal[0]=='.') {
        strcpy(str2, strFinal);
        strConcat(strFinal,str2,"0",0);
    }

    if(strlen(strFinal)>18)
        strFinal[18] = '\0';

    return -1;
}


static struct confd_decimal64 string_to_confd_decimal64(char *input_str) {
    char parts[100][1000];
    char parts1[100][1000];
    char *float_part;
    char *endptr;
    struct confd_decimal64 d64;
    int n,n2;
    
    n = string_split(parts,input_str,"e");
    if (1 == n) {
        n = string_split(parts,input_str,"E");
    }

    float_part = parts[0];
    //printf("float_part %s\n",float_part);
    n2 = string_split(parts1, float_part, ".");
    if (n2>1) {
        char temp_str[80];
        strcpy(temp_str, parts1[0]);
        strcat(temp_str, parts1[1]);
        //printf("temp_str %s\n",temp_str);
        d64.value = strtoimax(temp_str,&endptr,10);
        d64.fraction_digits = strlen(parts1[1]);
    } else {
        d64.value = strtoimax(float_part,&endptr,10);
        d64.fraction_digits = 0;
    }
    
    // deal with the exponential part (e.g. E-3)
    if (n>=2) {
        char *exp_part = parts[1];
        int exponent = strtoimax(exp_part,&endptr,10);
        //printf("exp_part %s\n",exp_part);
        if (exponent<=0) {
            d64.fraction_digits += exponent*(-1);
        } else 
        if (exponent>=d64.fraction_digits) {
            int64_t multiplier;
            exponent -= d64.fraction_digits;
            d64.fraction_digits = 0;
            multiplier = pow(10, exponent); //10^exponent
            d64.value = d64.value*multiplier;
        } else {
            d64.fraction_digits -= exponent;
        }
    }

    //printf("d64 int part %ld\n",d64.value);
    //printf("d64 digits part %d\n",d64.fraction_digits);
    return d64;
}


struct Floats parseValuesFloat(float val, char const* const type, int channel){
    struct Floats result;
    if (strcmp(type,"BER") == 0 ) {
        count_ber[channel]=count_ber[channel]+1.0f;
        bersum[channel]=bersum[channel]+val;
        beravg[channel]=bersum[channel]/count_ber[channel];
        if(bermin[channel]==0.0f){
            bermin[channel]=val;            
        }
        else{
            if(val<bermin[channel])
               bermin[channel]=val;  
        }
        if(bermax[channel]==0.0f){
            bermax[channel]=val;
        }
        else{
            if(val>bermax[channel])
               bermax[channel]=val; 
        }
        result.favg=beravg[channel];
        result.fmin=bermin[channel];
        result.fmax=bermax[channel];
    }else
    if (strcmp(type,"QVAL") == 0 ) {
        count_q[channel]=count_q[channel]+1.0f;
        qsum[channel]=qsum[channel]+val;
        qavg[channel]=qsum[channel]/count_q[channel];
        if(qmin[channel]==0.0f){
            qmin[channel]=val;            
        }
        else{
            if(val<qmin[channel])
               qmin[channel]=val;  
        }
        if(qmax[channel]==0.0f){
            qmax[channel]=val;
        }
        else{
            if(val>qmax[channel])
               qmax[channel]=val; 
        }
        result.favg=qavg[channel];
        result.fmin=qmin[channel];
        result.fmax=qmax[channel];
    }else
    if (strcmp(type,"ESNR") == 0 ) {
        count_esnr[channel]=count_esnr[channel]+1.0f;
        esnrsum[channel]=esnrsum[channel]+val;
        esnravg[channel]=esnrsum[channel]/count_esnr[channel];
        if(esnrmin[channel]==0.0f){
            esnrmin[channel]=val;            
        }
        else{
            if(val<esnrmin[channel])
               esnrmin[channel]=val;  
        }
        if(esnrmax[channel]==0.0f){
            esnrmax[channel]=val;
        }
        else{
            if(val>esnrmax[channel])
               esnrmax[channel]=val; 
        }
        result.favg=esnravg[channel];
        result.fmin=esnrmin[channel];
        result.fmax=esnrmax[channel];
    }else
    if (strcmp(type,"CD") == 0 ) {
        count_cd[channel]=count_cd[channel]+1.0f;
        cdsum[channel]=cdsum[channel]+val;
        cdavg[channel]=cdsum[channel]/count_cd[channel];
        if(cdmin[channel]==0.0f){
            cdmin[channel]=val;            
        }
        else{
            if(val<cdmin[channel])
               cdmin[channel]=val;  
        }
        if(cdmax[channel]==0.0f){
            cdmax[channel]=val;
        }
        else{
            if(val>cdmax[channel])
               cdmax[channel]=val; 
        }
        result.favg=cdavg[channel];
        result.fmin=cdmin[channel];
        result.fmax=cdmax[channel];
    }
    return result;
}



void *monitoringSocket(void *vargp) {
    int    run=1;
    int    len, rc, on = 1;
    int    listen_sd = -1, new_sd = -1;
    int    compress_array = FALSE;
    int    close_conn;
    char   buffer[1000];
    struct sockaddr_in   addr;
    int    timeout;
    struct pollfd fds[200];
    int    nfds = 1, current_size = 0, i, j;
    //#define SERVER_PORT  12346

    printf("Monitoring thread started\n");

    ///////////////////////////////////////////////////////////////
    // Create an AF_INET stream socket to receive incoming       //
    // connections on                                            //
    ///////////////////////////////////////////////////////////////
    listen_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sd < 0) {
        perror("socket() failed");
        return NULL;
    }

    ///////////////////////////////////////////////////////////////
    // Allow socket descriptor to be reuseable                   //
    ///////////////////////////////////////////////////////////////
    rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
                    (char *)&on, sizeof(on));
    if (rc < 0) {
        perror("setsockopt() failed");
        close(listen_sd);
        return NULL;
    }

    ///////////////////////////////////////////////////////////////
    // Set socket to be nonblocking. All of the sockets for      //
    // the incoming connections will also be nonblocking since   //
    // they will inherit that state from the listening socket.   //
    ///////////////////////////////////////////////////////////////
    rc = ioctl(listen_sd, FIONBIO, (char *)&on);
    if (rc < 0) {
        perror("ioctl() failed");
        close(listen_sd);
        return NULL;
    }

    ///////////////////////////////////////////////////////////////
    // Bind the socket                                           //
    ///////////////////////////////////////////////////////////////
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(SERVER_PORT);
    rc = bind(listen_sd,
                (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0) {
        perror("bind() failed");
        close(listen_sd);
        return NULL;
    }

    ///////////////////////////////////////////////////////////////
    // Set the listen back log                                   //
    ///////////////////////////////////////////////////////////////
    rc = listen(listen_sd, 32);
    if (rc < 0) {
        perror("listen() failed");
        close(listen_sd);
        return NULL;
    }

    
    ///////////////////////////////////////////////////////////////
    // Initialize the pollfd structure                           //
    ///////////////////////////////////////////////////////////////
    memset(fds, 0 , sizeof(fds));
    ///////////////////////////////////////////////////////////////
    // Set up the initial listening socket                       //
    ///////////////////////////////////////////////////////////////
    fds[0].fd = listen_sd;
    fds[0].events = POLLIN;

    ///////////////////////////////////////////////////////////////
    // Loop waiting for incoming connects or for incoming data   //
    // on any of the connected sockets.                          //
    ///////////////////////////////////////////////////////////////
    timeout = (10 * 1000); //10 seconds
    do {
        switch (poll(fds, nfds, timeout)) {
            case -1:
                perror("  poll() failed");
                break;

            default:
                /////////////////////////////////////////////////////////////
                // One or more descriptors are readable.  Need to          //
                // determine which ones they are.                          //
                /////////////////////////////////////////////////////////////
                current_size = nfds;
                for (i = 0; i < current_size; i++) {
                    
                    if (fds[i].revents & POLLIN) {
                        if (fds[i].fd == listen_sd) {
                            /////////////////////////////////////////////////////////
                            // Listening descriptor is readable.                   //
                            /////////////////////////////////////////////////////////
                            printf("  Listening socket is readable\n");

                            /////////////////////////////////////////////////////////
                            // Accept all incoming connections that are            //
                            // queued up on the listening socket before we         //
                            // loop back and call poll again.                      //
                            /////////////////////////////////////////////////////////
                            do {
                                /////////////////////////////////////////////////////
                                // Accept each incoming connection. If             //
                                // accept fails with EWOULDBLOCK, then we          //
                                // have accepted all of them. Any other            //
                                // failure on accept will cause us to end the      //
                                // server.                                         //
                                /////////////////////////////////////////////////////
                                new_sd = accept(listen_sd, NULL, NULL);
                                if (new_sd < 0) {
                                    if (errno != EWOULDBLOCK) {
                                        perror("  accept() failed\n");
                                    }
                                    break;
                                }

                                /////////////////////////////////////////////////////
                                // Add the new incoming connection to the          //
                                // pollfd structure                                //
                                /////////////////////////////////////////////////////
                                printf("  New device connected - %d\n", new_sd);
                                fds[nfds].fd = new_sd;
                                fds[nfds].events = POLLIN;
                                nfds++;

                                /////////////////////////////////////////////////////
                                // Loop back up and accept another incoming        //
                                // connection                                      //
                                /////////////////////////////////////////////////////
                            } while (new_sd != -1);
                        }

                        else {
                            /////////////////////////////////////////////////////////
                            // This is not the listening socket, therefore an      //
                            // existing connection must be readable                //
                            /////////////////////////////////////////////////////////
                            do {
                                int num_iterations,iter_i;
                                char buffer2[10][1000];

                                printf("  Descriptor %d is readable\n", fds[i].fd);
                                close_conn = FALSE;
                                /////////////////////////////////////////////////////
                                // Receive all incoming data on this socket        //
                                // before we loop back and call poll again.        //
                                /////////////////////////////////////////////////////

                                /////////////////////////////////////////////////////
                                // Receive data on this connection until the       //
                                // recv fails with EWOULDBLOCK. If any other       //
                                // failure occurs, we will close the               //
                                // connection.                                     //
                                /////////////////////////////////////////////////////
                                memset(buffer, 0, sizeof(buffer));
                                rc = recv(fds[i].fd, buffer, sizeof(buffer), 0);
                                if (rc < 0) {
                                    if (errno != EWOULDBLOCK) {
                                      perror("  recv() failed");
                                      close_conn = TRUE;
                                    }
                                    break;
                                }

                                /////////////////////////////////////////////////////
                                // Check to see if the connection has been         //
                                // closed by the client                            //
                                /////////////////////////////////////////////////////
                                if (rc == 0) {
                                    printf("  Device disconnected closed\n");
                                    close_conn = TRUE;
                                    break;
                                }

                                /////////////////////////////////////////////////////
                                // Data was received                               //
                                /////////////////////////////////////////////////////
                                len = rc;
                                printf("  %d bytes received\n", len);
                                printf("  Received: %s\n", buffer);

                                num_iterations = string_split(buffer2,buffer,"&&");


                                for (iter_i=0; iter_i<num_iterations; iter_i++) {
                                    /////////////////////////////////////////////////////
                                    // Split data by the delimiter                     //
                                    /////////////////////////////////////////////////////
                                    char tokens[10][1000];
                                    // parse the received message
                                    int num_tokens = string_split(tokens,buffer2[iter_i],"###");
                                    if (num_tokens >= 2) {
                                    /////////////////////////////////////////////
                                    // UPDATE THE DATABASE                     //
                                    ///////////////////////////////////////////// 
                                    int logchan = atoi(tokens[0]);
                                    //logical-channels monitoring parameters 
                                    //parsing of PRE-FEC-BER values
                                    if (strcmp(tokens[1],"PRE-FEC-BER") == 0 ) {
                                        if (num_tokens >=6) {
                                            
                                            char value_str[25], path[100];
                                            struct confd_decimal64 ber_d64;
                                            printf("Updating PREFECBER of logical channel:%d \n",logchan);
                                            //instant pre-fec-ber
                                            ber_d64 = string_to_confd_decimal64(tokens[2]);
                                            confd_decimal64_to_string(value_str,&ber_d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "instant");
                                            updateDb(path,value_str);
                                            //mod Andrea
                                            
                                            //avg pre-fec-ber
                                            ber_d64 = string_to_confd_decimal64(tokens[3]);
                                            confd_decimal64_to_string(value_str,&ber_d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "avg");
                                            updateDb(path,value_str);
                                            //min pre-fec-ber
                                            ber_d64 = string_to_confd_decimal64(tokens[4]);
                                            confd_decimal64_to_string(value_str,&ber_d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "min");
                                            updateDb(path,value_str);
                                            //max pre-fec-ber
                                            ber_d64 = string_to_confd_decimal64(tokens[5]);
                                            confd_decimal64_to_string(value_str,&ber_d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "max");
                                            updateDb(path,value_str);
                                        } else
                                        if (num_tokens ==3) {
                                            float binstant=0.0f;
                                            char value_str[25], path[100];
                                            struct confd_decimal64 ber_d64;
                                            printf("Updating PREFECBER of logical channel:%d \n",logchan);
                                            //instant pre-fec-ber
                                            ber_d64 = string_to_confd_decimal64(tokens[2]);
                                            confd_decimal64_to_string(value_str,&ber_d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "instant");
                                            updateDb(path,value_str);
                                            //mod Andrea
                                            binstant=atof(value_str);
                                            struct Floats ff = parseValuesFloat(binstant, "BER", logchan);
                                            printf("after %f \n", ff.favg);
                                            //avg pre-fec-ber
                                            
                                            sprintf(value_str, "%f", ff.favg);
                                            //snprintf(value_str, sizeof value_str, '%f', ff.favg);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "avg");
                                            updateDb(path,value_str);
                                            printf("before\n");
                                            //min pre-fec-ber
                                            sprintf(value_str, "%f", ff.fmin);
                                            //snprintf(value_str, sizeof value_str, '%f', ff.favg);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "min");
                                            updateDb(path,value_str);
                                            //max pre-fec-ber
                                            sprintf(value_str, "%f", ff.fmax);
                                            //snprintf(value_str, sizeof value_str,  '%f', ff.favg);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "max");
                                            updateDb(path,value_str);
                                            //printf("Summary BER sum: %f, count:%f, min:%f max%f \n", bersum, count_ber, bermin, bermax);
                                        } else
                                            printf("WARNING, pre-fec-ber update with %d elements\n",num_tokens);
                                    } else
                                    //parsing of Q-VALUE values
                                    if (strcmp(tokens[1],"Q-VALUE") == 0) {
                                        if (num_tokens >=6) {
                                            char value_str[25], path[100] ;
                                            struct confd_decimal64 q64;
                                            printf("Updating Q-FACTOR of logical channel:%d\n",logchan);
                                            //instant q-factor
                                            q64 = string_to_confd_decimal64(tokens[2]);
                                            confd_decimal64_to_string(value_str,&q64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "instant");
                                            updateDb(path,value_str);
                                            //avg q-factor
                                            q64 = string_to_confd_decimal64(tokens[3]);
                                            confd_decimal64_to_string(value_str,&q64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "avg");
                                            updateDb(path,value_str);
                                            //min q-factor
                                            q64 = string_to_confd_decimal64(tokens[4]);
                                            confd_decimal64_to_string(value_str,&q64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "min");
                                            updateDb(path,value_str);
                                            //max q-factor
                                            q64 = string_to_confd_decimal64(tokens[5]);
                                            confd_decimal64_to_string(value_str,&q64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "max");
                                            updateDb(path,value_str);
                                        } else
                                        if (num_tokens ==3) {
                                            float qinstant=0.0f;
                                            
                                            char value_str[25], path[100];
                                            struct confd_decimal64 ber_d64;
                                            printf("Updating Q-FACTOR of logical channel:%d \n",logchan);
                                            //instant pre-fec-ber
                                            ber_d64 = string_to_confd_decimal64(tokens[2]);
                                            confd_decimal64_to_string(value_str,&ber_d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "instant");
                                            updateDb(path,value_str);
                                            
                                            //mod Andrea
                                            qinstant=atof(value_str);
                                            struct Floats ff1 = parseValuesFloat(qinstant, "QVAL", logchan);
                                            //avg pre-fec-ber
                                            sprintf(value_str, "%.2f", ff1.favg);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "avg");
                                            updateDb(path,value_str);
                                            
                                            //min pre-fec-ber
                                            sprintf(value_str, "%.2f", ff1.fmin);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "min");
                                            updateDb(path,value_str);
                                            
                                            //max pre-fec-ber
                                            sprintf(value_str, "%.2f", ff1.fmax);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "max");
                                            updateDb(path,value_str);
                                        } else
                                            printf("WARNING, Q-FACTOR update with %d elements\n",num_tokens);
                                    } else
                                    //parsing of ESNR values
                                    if (strcmp(tokens[1],"ESNR") == 0) {
                                        if (num_tokens >=6) {
                                            char value_str[25], path[100];
                                            struct confd_decimal64 d64;
                                            printf("Updating ESNR of logical channel:%d \n",logchan);
                                            //instant pre-fec-ber
                                            d64 = string_to_confd_decimal64(tokens[2]);
                                            confd_decimal64_to_string(value_str,&d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "instant");
                                            updateDb(path,value_str);
                                            //avg pre-fec-ber
                                            d64 = string_to_confd_decimal64(tokens[3]);
                                            confd_decimal64_to_string(value_str,&d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "avg");
                                            updateDb(path,value_str);
                                            //min pre-fec-ber
                                            d64 = string_to_confd_decimal64(tokens[4]);
                                            confd_decimal64_to_string(value_str,&d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "min");
                                            updateDb(path,value_str);
                                            //max pre-fec-ber
                                            d64 = string_to_confd_decimal64(tokens[5]);
                                            confd_decimal64_to_string(value_str,&d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "max");
                                            updateDb(path,value_str);
                                        } else
                                        if (num_tokens ==3) {
                                            float einstant=0.0f;
                                            
                                            char value_str[25], path[100];
                                            struct confd_decimal64 ber_d64;
                                            printf("Updating ESNR of logical channel:%d \n",logchan);
                                            //instant pre-fec-ber
                                            ber_d64 = string_to_confd_decimal64(tokens[2]);
                                            confd_decimal64_to_string(value_str,&ber_d64);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "instant");
                                            updateDb(path,value_str);
                                            
                                            //mod Andrea
                                            einstant=atof(value_str);
                                            struct Floats ff2 = parseValuesFloat(einstant, "ESNR", logchan);
                                            //avg pre-fec-ber
                                            sprintf(value_str, "%.2f", ff2.favg);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "avg");
                                            updateDb(path,value_str);
                                            
                                            //min pre-fec-ber
                                            sprintf(value_str, "%.2f", ff2.fmin);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "min");
                                            updateDb(path,value_str);
                                            
                                            //max pre-fec-ber
                                            sprintf(value_str, "%.2f", ff2.fmax);
                                            sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "max");
                                            updateDb(path,value_str);
                                        } else
                                            printf("WARNING, ESNR update with %d elements\n",num_tokens);
                                    } else
                                    //components monitoring parameters 
                                    
                                    //parsing of CHROMATIC-DISPERSION value
                                    if (strcmp(tokens[1],"CHROMATIC-DISPERSION") == 0) {
                                        if (num_tokens >=6) {
                                            char value_str[25], path[100];
                                            struct confd_decimal64 d64;
                                            printf("Updating Chromatic Dispersion of of component %s\n",tokens[0]);
                                            //instant pre-fec-bers
                                            d64 = string_to_confd_decimal64(tokens[2]);
                                            confd_decimal64_to_string(value_str,&d64);
                                            sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/instant");
                                            updateDb(path,value_str);
                                            //avg pre-fec-ber
                                            d64 = string_to_confd_decimal64(tokens[3]);
                                            confd_decimal64_to_string(value_str,&d64);
                                            sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/avg");
                                            updateDb(path,value_str);
                                            //min pre-fec-ber
                                            d64 = string_to_confd_decimal64(tokens[4]);
                                            confd_decimal64_to_string(value_str,&d64);
                                            sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/min");
                                            updateDb(path,value_str);
                                            //max pre-fec-ber
                                            d64 = string_to_confd_decimal64(tokens[5]);
                                            confd_decimal64_to_string(value_str,&d64);
                                            sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/max");
                                            updateDb(path,value_str);
                                        } else
                                        if (num_tokens ==3) {
                                            float cinstant=0.0f;
                                            
                                            char value_str[25], path[100];
                                            struct confd_decimal64 ber_d64;
                                            printf("Updating Chromatic Dispersion of logical channel:%d \n",logchan);
                                            //instant pre-fec-ber
                                            ber_d64 = string_to_confd_decimal64(tokens[2]);
                                            confd_decimal64_to_string(value_str,&ber_d64);
                                            sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/instant");
                                            updateDb(path,value_str);
                                            
                                            //mod Andrea
                                            cinstant=atof(value_str);
                                            struct Floats ff3 = parseValuesFloat(cinstant, "CD", logchan);
                                            //avg pre-fec-ber
                                            sprintf(value_str, "%.2f", ff3.favg);
                                            sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/avg");
                                            updateDb(path,value_str);
                                            
                                            //min pre-fec-ber
                                            sprintf(value_str, "%.2f", ff3.fmin);
                                            sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/min");
                                            updateDb(path,value_str);
                                            
                                            //max pre-fec-ber
                                            sprintf(value_str, "%.2f", ff3.fmax);
                                            sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/max");
                                            updateDb(path,value_str);
                                        } else
                                            printf("WARNING, Chromatic-dispersion update with %d elements\n",num_tokens);
                                    } else
                                    //parsing of FREQUENCY value
                                    if (strcmp(tokens[1],"FREQUENCY") == 0) {
                                        char value_str[25], path[100];
                                        strncpy(value_str, tokens[2], 9);
                                        //value_str=tokens[2];
                                        //confd_decimal64_to_string(value_str,&d64);
                                        printf("Updating Central Frequency of component %s\n",tokens[0]);
                                        ///components/component[name="32/1/18/11"]/optical-channel/state
                                        sprintf(path,"/components/component{channel-%s}/optical-channel/state/%s/", tokens[0], "frequency");
                                        updateDb(path,value_str);
                                    } else
                                    if (strcmp(tokens[1],"DOWN") == 0) {
                                        printf("Subcarrier:%d DOWN \n", logchan);
                                    }
                                    else
                                        printf("WARNING:Update unknown for logical channel %d \n", logchan);
                                    }                                
                                }                         

                            } while(TRUE);

                            /////////////////////////////////////////////////////////
                            // If the close_conn flag was turned on, we need       //
                            // to clean up this active connection. This            //
                            // clean up process includes removing the              //
                            // descriptor.                                         //
                            /////////////////////////////////////////////////////////
                            if (close_conn) {
                                close(fds[i].fd);
                                fds[i].fd = -1;
                                compress_array = TRUE;
                            }

                        }
                    }
                }

                /////////////////////////////////////////////////////////////
                // If the compress_array flag was turned on, we need       //
                // to squeeze together the array and decrement the number  //
                // of file descriptors. We do not need to move back the    //
                // events and revents fields because the events will always//
                // be POLLIN in this case, and revents is output.          //
                /////////////////////////////////////////////////////////////
                if (compress_array) {
                    compress_array = FALSE;
                    for (i = 0; i < nfds; i++) {
                        if (fds[i].fd == -1) {
                            for(j = i; j < nfds; j++) {
                                fds[j].fd = fds[j+1].fd;
                            }
                            nfds--;
                        }
                    }
                }

                break;
        } // switch
        
        
        pthread_mutex_lock( &running_mutex );
        run = threadsKeepRunning;
        pthread_mutex_unlock( &running_mutex );

    } while (run);

    /////////////////////////////////////////////////////////////
    // Clean up all of the sockets that are open               //
    /////////////////////////////////////////////////////////////
    for (i = 0; i < nfds; i++) {
        if(fds[i].fd >= 0)
            close(fds[i].fd);
    }
    close(listen_sd);

    printf("Monitoring thread terminated\n");
    return NULL;
}



/*
garbage....
{"ber": 0.0011950469, "osnr": 16.2, "q-value": 9.0, "cd": 5258.0}

ber: 0.0012650689, osnr: 16.0, q-value: 9.0, cd: 5248.0
*/



void stateSPOUpdater(int logchan, char ber[1000], char qf[1000], char esnr[1000], char cd[1000]){
      //PRE-FEC-BER
      char paths[100][1000];
      char values_str[100][1000];
      int elements=0;

      float binstant=0.0f;
      char value_str[25], path[100], reas[100], port[40];
      struct confd_decimal64 ber_d64;
      printf("Updating PREFECBER of logical channel:%d \n",logchan);
      sprintf(port,"%d", logchan);
      //instant pre-fec-ber
      ber_d64 = string_to_confd_decimal64(ber);
      confd_decimal64_to_string(value_str,&ber_d64);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "instant");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "instant");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      binstant=atof(value_str);
      //check with threshold to send notification
      float bth1,bth2;
      bth1 = 0.0004;
      bth2 = 0.01;
      sprintf(reas,"pre-fec-ber=%s", ber);
      if ((binstant>bth1) && (binstant<bth2)){
        send_notif_failure(port, reas, "SOFT");
      }
      if (binstant>bth2){
        send_notif_failure(port, reas, "HARD");
      }

      struct Floats ff = parseValuesFloat(binstant, "BER", logchan);
      //avg pre-fec-ber
      sprintf(value_str, "%f", ff.favg);
      //snprintf(value_str, sizeof value_str, '%f', ff.favg);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "avg");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "avg");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      //min pre-fec-ber
      sprintf(value_str, "%f", ff.fmin);
      //snprintf(value_str, sizeof value_str, '%f', ff.favg);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "min");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "min");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      //max pre-fec-ber
      sprintf(value_str, "%f", ff.fmax);
      //snprintf(value_str, sizeof value_str,  '%f', ff.favg);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "max");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/pre-fec-ber/%s/", logchan, "max");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      //printf("Summary BER sum: %f, count:%f, min:%f max%f \n", bersum, count_ber, bermin, bermax);



      //Q-FACTOR
      float qinstant=0.0f;
      printf("Updating Q-FACTOR of logical channel:%d \n",logchan);
      //instant q-fact
      ber_d64 = string_to_confd_decimal64(qf);
      confd_decimal64_to_string(value_str,&ber_d64);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "instant");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "instant");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      qinstant=atof(value_str);
      struct Floats ff1 = parseValuesFloat(qinstant, "QVAL", logchan);
      //avg q-fact
      sprintf(value_str, "%.2f", ff1.favg);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "avg");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "avg");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      //min q-fact
      sprintf(value_str, "%.2f", ff1.fmin);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "min");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "min");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      //max q-fact
      sprintf(value_str, "%.2f", ff1.fmax);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "max");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/q-value/%s/", logchan, "max");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);


      //ESNR
      float einstant=0.0f;
      printf("Updating ESNR of logical channel:%d \n",logchan);
      //instant esnr
      ber_d64 = string_to_confd_decimal64(esnr);
      confd_decimal64_to_string(value_str,&ber_d64);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "instant");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "instant");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      einstant=atof(value_str);
      float eth1,eth2;
      eth1 = 11.0;
      eth2 = 9.1;
      sprintf(reas,"osnr=%s", esnr);
      if ((einstant<eth1) && (einstant>eth2)){
        send_notif_failure(port, reas, "SOFT");
      }
      if (einstant<eth2){
        send_notif_failure(port, reas, "HARD");
      }
      struct Floats ff2 = parseValuesFloat(einstant, "ESNR", logchan);
      //avg esnr
      sprintf(value_str, "%.2f", ff2.favg);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "avg");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "avg");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      //min esnr
      sprintf(value_str, "%.2f", ff2.fmin);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "min");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "min");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      //max esnr
      sprintf(value_str, "%.2f", ff2.fmax);
      sprintf(path,"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "max");
      sprintf(paths[elements],"/terminal-device/logical-channels/channel{%d}/otn/state/esnr/%s/", logchan, "max");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);


      //CHROMATIC-DISPERSION
      float cinstant=0.0f;
      printf("Updating Chromatic Dispersion of logical channel:%d \n",logchan);
      //instant cd
      ber_d64 = string_to_confd_decimal64(cd);
      confd_decimal64_to_string(value_str,&ber_d64);
      sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/instant");
      sprintf(paths[elements],"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/instant");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      cinstant=atof(value_str);
      struct Floats ff3 = parseValuesFloat(cinstant, "CD", logchan);
      //avg cd
      sprintf(value_str, "%.2f", ff3.favg);
      sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/avg");
      sprintf(paths[elements],"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/avg");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      //min cd
      sprintf(value_str, "%.2f", ff3.fmin);
      sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/min");
      sprintf(paths[elements],"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/min");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      //max cd
      sprintf(value_str, "%.2f", ff3.fmax);
      sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/max");
      sprintf(paths[elements],"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "chromatic-dispersion/max");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      updateDbVals(elements, paths, values_str); 
}

void statePlugUpdater(int logchan, char * rxval){
      char paths[100][1000];
      char values_str[100][1000];
      int elements=0;
      float rxinstant=0.0f;
      //input-power
      char value_str[25], path[100], reas[50], port[40];
      struct confd_decimal64 ber_d64;
      printf("Updating input-power of logical channel:%d \n",logchan);
      sprintf(port,"%d", logchan);
      //instant input-power
      ber_d64 = string_to_confd_decimal64(rxval);
      confd_decimal64_to_string(value_str,&ber_d64);
      sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "input-power/instant");
      sprintf(paths[elements],"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "input-power/instant");
      strcpy(values_str[elements], value_str);
      elements=elements+1;
      //updateDb(path,value_str);
      float rth1,rth2;
      rth1 = -22.0;
      rth2 = -30.0;
      sprintf(reas,"input-power=%s", rxval);
      rxinstant=atof(value_str);
      if(rx!= 0.0f){
          if (rxinstant < (rx - rx_min_gap)){
             if ((rxinstant<rth1) && (rxinstant>rth2)){
                send_notif_failure(port, reas, "SOFT");
             }
             if (rxinstant<rth2){
                send_notif_failure(port, reas, "HARD");
             }
          }
      }
      rx=rxinstant;
      /*
      struct Floats ff3 = parseValuesFloat(cinstant, "RX", logchan);
      //avg rx
      sprintf(value_str, "%.2f", ff3.favg);
      sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "input-power/avg");
      updateDb(path,value_str);
      //min rx
      sprintf(value_str, "%.2f", ff3.fmin);
      sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "input-power/min");
      updateDb(path,value_str);
      //max rx
      sprintf(value_str, "%.2f", ff3.fmax);
      sprintf(path,"/components/component{channel-%d}/optical-channel/state/%s/", logchan, "input-power/max");
      updateDb(path,value_str);
      */
      updateDbVals(elements, paths, values_str); 
}


//static int process_reply(char ret_str[100][1000], char * input_str) {
static int process_reply(int channel, char * input_str) {
    const char s[10] = "{,:}\"";
    char *tok = strtok(input_str, s);
    char ss[100][1000];
    int j=0;
    //printf("%s", input_str);
    while(tok) {
      remove_spaces(tok);
      strcpy(ss[j], tok);
      //printf("position %d:  %s\n", j, tok);
      j++;
      tok = strtok(NULL, s);
    }
    printf("%s:%s, %s:%s, %s:%s, %s:%s\n", ss[8], ss[9], ss[11],ss[12], ss[14],ss[15],ss[17],ss[18]);
    stateSPOUpdater(channel, ss[9], ss[12], ss[15], ss[18]);
    
    return j;
}




void *monitoringPolling(void *vargp) {
    int run = 1;
    char path[2000];
    char response[4096];


    //sprintf(path,"/Monitoring/start");
    //sender("PUT", path, NULL, 0, NULL);
    printf("Monitoring REST thread started\n");
    /*
    monPorts[0].port_id=19;
    monPorts[0].type=1;
    //monPorts[0].notes="18/11";
    sprintf(monPorts[0].notes, "%s", "18/11");
    */
    int i;
    do {
        for(i = 0; i < numMonPorts; i++ ){ 
           if (monPorts[i].type == 1){
               if (monPorts[i].port_id!=0){
                   printf("SPO port\n");
                   if (monPorts[i].config == 1){
                   	sprintf(path,"/Monitoring/GetPortStats/%s/%s", SPONAME, monPorts[i].notes);
                   	sender("GET", path, response, 0, NULL);
                   	printf("Stats:\n%s\n",response);
                   	process_reply(monPorts[i].port_id, response); //it also updates the db
                   }
               }
           }else
           if (monPorts[i].type == 2){
               if (monPorts[i].port_id!=0){
                  printf("Pluggable port\n");
                  sprintf(path,"python2.7 %s/%s 1",SCRIPT_PATH, GET);
                  //sprintf(path,"python2.7 pp.py");
                  int res= exec_command(response, path);
                  if (res == 0)
                    statePlugUpdater(monPorts[i].port_id, response);
                  else
                    printf("Error executing the python command: %s\n", path);
                  if(monPorts[0].port_id==0)
                     usleep( 1000000 );
               }
           }
        }
	//check the exit signal
	pthread_mutex_lock( &running_mutex );
        run = threadsKeepRunning;
        pthread_mutex_unlock( &running_mutex );
        //usleep(POLLING*1000);
    } while (run);
    //sprintf(path,"/Monitoring/stop");
    //sender("PUT", path, NULL, 0, NULL);

    printf("Monitoring REST thread terminated\n");
    return NULL;

}


void *files_monitoring(void *vargp) {

  #define EVENT_SIZE  ( sizeof (struct inotify_event) )
  #define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )
  #define BER_FILENAME "monitoredFiles/ber.dat"
  #define VALUES_SEPARATOR " "

  int length, i = 0, run=1;
  int fd;
  int wd;
  char buffer[EVENT_BUF_LEN];

  /*creating the INOTIFY instance*/
  fd = inotify_init();
  /*checking for error*/
  if ( fd < 0 ) {
    perror( "inotify_init error" );
  }

  
  if( access( BER_FILENAME, F_OK ) == -1 ) {
      // file doesn't exist
      printf( "File %s does not extist\n", BER_FILENAME);
      return NULL;
  } else if( access( BER_FILENAME, R_OK ) == -1 ) {
      // read not allowed on the file
      printf( "File %s cannot be read (NO read permission)\n", BER_FILENAME);
      return NULL;
  }

  wd = inotify_add_watch( fd, BER_FILENAME, IN_MODIFY);//IN_CREATE | IN_DELETE | IN_ACCESS | IN_MODIFY | IN_OPEN );
  
  do {
      // read to determine the event change happens on the files/directory on the watch list. 
      // Actually this read blocks until the change event occurs 
      length = read( fd, buffer, EVENT_BUF_LEN ); 
      /* checking for error */
      if ( length < 0 ) {
        perror( "read" );
        return NULL;
      }  

      
      i=0;
      //  Actually read return the list of change events happens. 
      //  Here, read the change event one by one and process it accordingly.
      while ( i < length ) {
        struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
        char *name;
        char fixName[] = BER_FILENAME;

        if( event->len == 0) {
          // For a single file watching, the event->name is empty, and event->len = 0
          name = fixName;
        } else {
          name = event->name; 
        }

        if( event->mask & IN_MODIFY ) {
            if ( event->mask & IN_ISDIR ) {
              printf( "Directory %s modified.\n", name );
            } else {
              printf(" File %s modified. \n", name );

              {
                char *line = NULL;
                size_t len = 0;
                ssize_t read;
                FILE *fp = fopen(BER_FILENAME, "r" );

                while ((read = getline(&line, &len, fp)) != -1) {
                    char tokens[10][1000];
                    printf("Retrieved line of length %zu :\n", read);
                    printf("%s", line);
                    int num_tokens = string_split(tokens,line,VALUES_SEPARATOR);
                    if (num_tokens>1) {
                      char path[100];
                      char *value_str = tokens[1];
                      printf("Update pre-FEC-BER:%s of subcarrier:%d to DATABASE \n",value_str,1);
                      // create database path string 
                      sprintf(path,"/transponder/subcarrier-module{%d}/state/receiver/%s/", 1, "pre-fec-ber");
                      updateDb(path,value_str);
                    }
                    break;
                }

                free(line);
              }

            }
        } else if ( event->mask & IN_CREATE ) {
            if ( event->mask & IN_ISDIR ) {
              printf( "New directory %s created.\n", name );
            } else {
              printf( "New file %s created.\n", name );
            }
        } else if ( event->mask & IN_DELETE ) {
            if ( event->mask & IN_ISDIR ) {
              printf( "Directory %s deleted.\n", name );
            } else {
              printf( "File %s deleted.\n", name );
            }
        } else if( event->mask & IN_ACCESS ) {
            if ( event->mask & IN_ISDIR ) {
              printf( "Directory %s accessed.\n", name );
            } else {
              printf(" File %s accessed. \n", name );
            }
        } else if( event->mask & IN_OPEN ) {
            if ( event->mask & IN_ISDIR ) {
              printf( "Directory %s opened.\n", name );
            } else {
              printf(" File %s opened. \n", name );
            }
        } else {
            printf( "Directory or File is accessed by other mode\n");
        }
        
        i += EVENT_SIZE + event->len;
      }

      pthread_mutex_lock( &running_mutex );
      run = threadsKeepRunning;
      pthread_mutex_unlock( &running_mutex );

  } while(run);

  /* removing the “/tmp/test_inotify” directory from the watch list. */
  inotify_rm_watch( fd, wd );

  /* closing the INOTIFY instance */
  close( fd );
  
  return NULL;
}


/*
static void send_notif_failure(char const* const port, char const* const reason, char const* const lev)
static void send_notif_db_change(char const* const elem, char const* const old, char const* const new)
*/

//------------------------------MAIN-----------------------------//

int main(int argc, char **argv) {
    char confd_port[16];
    struct addrinfo hints;
    struct addrinfo *addr = NULL;
    int debuglevel = CONFD_DEBUG; //CONFD_SILENT
    struct sockaddr_in s_addr;
    int i;
    int c;
    char *p, *dname;
    int ret;
    int timeout;
    int rnum;
    int spoint; 

    snprintf(confd_port, sizeof(confd_port), "%d", CONFD_PORT);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    //MONITORING
    //port monitoring
    //port4
    monPorts[0].port_id=4;
    monPorts[0].type=1;
    monPorts[0].config=1;
    sprintf(monPorts[0].notes, "%s", "11811");
    //port1
    monPorts[1].port_id=1;
    monPorts[1].type=2;
    //monPorts[1].config=0;
    sprintf(monPorts[1].notes, "%s", "pluggable");

    while ((c = getopt(argc, argv, "dtprc:")) != -1) {
        switch (c) {
        case 'd':
            debuglevel = CONFD_DEBUG;
            break;
        case 't':
            debuglevel = CONFD_TRACE;
            break;
        case 'p':
            debuglevel = CONFD_PROTO_TRACE;
            break;
        case 'c':
            if ((p = strchr(optarg, '/')) != NULL)
                *p++ = '\0';
            else
                p = confd_port;
            if (getaddrinfo(optarg, p, &hints, &addr) != 0) {
                if (p != confd_port) {
                    *--p = '/';
                    p = "/port";
                } else {
                    p = "";
                }
                fprintf(stderr, "%s: Invalid address%s: %s\n",
                        argv[0], p, optarg);
                exit(1);
            }
            break;
        default:
            fprintf(stderr,
                    "Usage: %s [-dtpr] [-c address[/port]]\n",
                    argv[0]);
            exit(1);
        }
    }
    //Connection to the ConfD database
    if (addr == NULL &&
        ((i = getaddrinfo("127.0.0.1", confd_port, &hints, &addr)) != 0))
        /* "Can't happen" */
        confd_fatal("%s: Failed to get address for ConfD: %s\n",
                    argv[0], gai_strerror(i));
    if ((dname = strrchr(argv[0], '/')) != NULL)
        dname++;
    else
        dname = argv[0];
    /* Init library */
    confd_init(dname, stderr, debuglevel);

      if ((deamon_p = confd_init_daemon(dname)) == NULL)
      confd_fatal("Failed to initialize ConfD\n");
      if ((ctlsock = GetCtrlSock(addr)) < 0)
          confd_fatal("Failed to connect to ConfD\n");
      if ((workersock = GetWorkerSock(addr)) < 0)
          confd_fatal("Failed to connect to ConfD\n");


    memset(replay_buffer, 0, sizeof(replay_buffer));
    memset(replay, 0, sizeof(replay));
    getdatetime(&replay_creation);

       
    // SUBSCRIBE TO DATABASE CHANGE NOTIFICATIONS
    s_addr.sin_addr.s_addr = inet_addr(confd_addr);
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(CONFD_PORT);

    if ((cdb_sock = GetCdbDataSock(&s_addr)) < 0)
      confd_fatal("Failed to connect to ConfD\n");

    if (confd_load_schemas((struct sockaddr*)&s_addr,
                         sizeof (struct sockaddr_in)) != CONFD_OK)
      confd_fatal("%s: Failed to load schemas from confd\n", argv[0]);

    if ((sub_sock = GetCdbSubSock(&s_addr)) < 0)
      confd_fatal("Failed to connect to ConfD\n");

    // terminal-devices config subscription
    if ((cdb_subscribe(sub_sock, 1, oc_opt_term__ns, &spoint,
                        "/terminal-device/logical-channels/channel/config/"))!= CONFD_OK) {
      confd_fatal("Terminate: subscribe \n");
    }
    // components config subscription
    if ((cdb_subscribe(sub_sock, 2, oc_opt_term__ns, &spoint,
                        "/components/component/optical-channel/config/"))!= CONFD_OK) {
      confd_fatal("Terminate: subscribe \n");
    }
    // telemetry subscription
    if ((cdb_subscribe(sub_sock, 2, oc_telemetry__ns, &spoint,
                        "/telemetry-system/subscriptions/dynamic-subscriptions/"))!= CONFD_OK) {
      confd_fatal("Terminate: subscribe \n");
    }
    // vlan tagged subscription
    if ((cdb_subscribe(sub_sock, 3, sv__ns, &spoint,
                        "/switched-vlans/vlan/tagged-members/member"))!= CONFD_OK) {
      confd_fatal("Terminate: subscribe \n");
    }
    // vlan untagged subscription
    if ((cdb_subscribe(sub_sock, 4, sv__ns, &spoint,
                        "/switched-vlans/vlan/untagged-members/member"))!= CONFD_OK) {
      confd_fatal("Terminate: subscribe \n");
    }
    // interface
    if ((cdb_subscribe(sub_sock, 5, oc_if__ns, &spoint,
                        "/interfaces/interface"))!= CONFD_OK) {
      confd_fatal("Terminate: subscribe \n");
    }
    // bgp
    if ((cdb_subscribe(sub_sock, 6, oc_prt__ns, &spoint,
                        "/bgp-instance/bgp"))!= CONFD_OK) {
      confd_fatal("Terminate: subscribe \n");
    }
    /*
    // vlan untagged subscription
    if ((cdb_subscribe(sub_sock, 4, sv__ns, &spoint,
                        "/switched-vlans/vlan/untagged-members/member"))!= CONFD_OK) {
      confd_fatal("Terminate: subscribe \n");
    }
    */
    // Tell to ConfD when the Application is ready to receive notifications
    // of CDB updates from ConfD
    if (cdb_subscribe_done(sub_sock) != CONFD_OK)
      confd_fatal("cdb_subscribe_done() failed");


    //----- REGISTER Callbacks -----
    callbacks_registration(deamon_p);

    // GET MAAPI SOCKET used to write into the CDB
    maapis = GetMaapiSock(&s_addr);


    
    // start monitoring thread
    if (MONITORING==1)
        pthread_create(&spo_thread_id, NULL, monitoringSocket, NULL);
    else if (MONITORING==2)
        pthread_create(&spo_thread_id, NULL, monitoringPolling, NULL);

    if (ENABLE_GRPC)
        pthread_create(&grpc_thread_id, NULL, grpc_server, NULL);
    // start thread for configuration of transponder
    fprintf(stderr,
"\n\n*** ConfD OpenConfig NETCONF agent ***\n\
***          developed by          ***\n\
***       Andrea Sgambelluri       ***\n\
***           CNIT-SSSA            ***\n\n\n");
    if(CONF_TRANSPONDER==1) intHardwareCommunication();
    
    // register handler for CRTL+C command
    signal(SIGINT, intHandler); 

    while (keepRunning) {
        struct pollfd set[4];
        set[0].fd = STDIN_FILENO; // fd of standard input (0)
        set[0].events = POLLIN;
        set[0].revents = 0;

        set[1].fd = ctlsock;
        set[1].events = POLLIN;
        set[1].revents = 0;

        set[2].fd = workersock;
        set[2].events = POLLIN;
        set[2].revents = 0;

        set[3].fd = sub_sock;
        set[3].events = POLLIN;
        set[3].revents = 0;

        /* if we're doning a replay, don't use a timeout */
        timeout = -1;
        for (rnum = 0; rnum < MAX_REPLAYS; rnum++) {
            if (replay[rnum].active) {
                timeout = 0;
                break;
            }
        }

        switch (poll(set, NELEMS(set), timeout)) {
        case -1:
            break;

        default:
            
            if (set[1].revents & POLLIN) { /* ctlsock */
                if ((ret = confd_fd_ready(deamon_p, ctlsock)) == CONFD_EOF) {
                    confd_fatal("Control socket closed\n");
                } else if (ret == CONFD_ERR && confd_errno != CONFD_ERR_EXTERNAL) {
                    confd_fatal("Error on control socket request: %s (%d): %s\n",
                                    confd_strerror(confd_errno),
                                    confd_errno,
                                    confd_lasterr());
                }
            }

            if (set[2].revents & POLLIN) { /* workersock */
                if ((ret = confd_fd_ready(deamon_p, workersock)) == CONFD_EOF) {
                    confd_fatal("Worker socket closed\n");
                } else if (ret == CONFD_ERR && confd_errno != CONFD_ERR_EXTERNAL) {
                    confd_fatal("Error on worker socket request: %s (%d): %s\n",
                                    confd_strerror(confd_errno),
                                    confd_errno,
                                    confd_lasterr());
                }
            }

            if (set[3].revents & POLLIN) {
                int sub_points[1];
                int reslen;
                int status;

                if ((status = cdb_read_subscription_socket(sub_sock,
                                                       &sub_points[0],
                                                       &reslen)) != CONFD_OK) {
                    confd_fatal("terminate sub_read: %d\n", status);
                }
                if (reslen > 0) {
                    fprintf(stderr, "*** Config updated \n");

                    if ((status = cdb_start_session(cdb_sock,CDB_RUNNING)) != CONFD_OK)
                        confd_fatal("Cannot start session\n");
                    if ((status = cdb_set_namespace(cdb_sock, oc_opt_term__ns)) != CONFD_OK)
                        confd_fatal("Cannot set namespace\n");

                    cdb_diff_iterate(sub_sock, sub_points[0], Iter,
                                     ITER_WANT_PREV, (void*)&cdb_sock);
                    cdb_end_session(cdb_sock);
                }

                if ((status = cdb_sync_subscription_socket(sub_sock,
                                                           CDB_DONE_PRIORITY))
                    != CONFD_OK) {
                    confd_fatal("failed to sync subscription: %d\n", status);
                }
            }

            if (set[0].revents & (POLLIN|POLLHUP)) { /* stdin */
                char c;
                if (!read(0, &c, 1))
                    exit(0);
                switch (c) {
                case 'f':
                  {
                    char port[BUFSIZ], reason[BUFSIZ], lev[BUFSIZ];
                    sprintf(port,"19");
                    sprintf(reason,"ber");
                    sprintf(lev,"0.002");
                    send_notif_failure(port, reason, lev);
                    printf("sending failure notification\n");
                  }
                  break;
                case 'g':
                  {
                    char port[BUFSIZ], elem[BUFSIZ], old[BUFSIZ], new[BUFSIZ];
                    sprintf(port,"19");
                    sprintf(elem,"operational-mode");
                    sprintf(old,"1");
                    sprintf(new,"2");
                    send_notif_db_change(port, elem, old, new);
                    printf("sending port change notification\n");
                  }
                  break;
                case '\n':
                  break;
                default:
                  printf("unknown character <%c>\n", c);
                  break;
                }
            }



            if (timeout == 0) {
                /* continue replay */
                for (rnum = 0; rnum < MAX_REPLAYS; rnum++) {
                    if (replay[rnum].active) {
                        continue_replay(&replay[rnum]);
                    }
                }
            }
        }
    } // while(keepRunning)

    close(ctlsock);
    close(workersock);
    close(sub_sock);
    close(cdb_sock);
    close(maapis);

    pthread_mutex_lock( &running_mutex );
    threadsKeepRunning = 0;
    pthread_mutex_unlock( &running_mutex );
    pthread_join(spo_thread_id, NULL);
    if (ENABLE_GRPC)
        pthread_join(grpc_thread_id, NULL);
    if(CONF_TRANSPONDER==1 && REST==0)
        stopHardwareCommunication();
    printf("\nMAIN Terminated\n");

    return 0;
}
