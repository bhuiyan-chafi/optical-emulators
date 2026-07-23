#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/msg.h>
#include <netdb.h>


#include <pthread.h>
#include <signal.h>

#include "transponder_driver.h"
#include "parameters.h"


static pthread_t config_thread_id;

static pthread_t send_pthread;

static int msg_queue_id;

struct senddata {
  char type[BUFSIZ];
  char path[BUFSIZ];;
  char ip[BUFSIZ];;
  char port[BUFSIZ];;
};



struct message_s {
  long int mtype;
  char     mtext[256];
};

static const size_t msg_size = sizeof(struct message_s) - sizeof(long int);

#define MESSAGE_TYPE 1
#define IPC_WAIT 0
#define MESSAGE_QUEUE_KEY 1235


#define send_rest 1
//struct subscriber *tel_subs_htable = NULL;    // important! initialize to NULL //



int string_splitter(char ret_str[3][100], char * input_str, char * delimiter_str) {
    char *token = strtok(input_str, delimiter_str);
    int jj=0;
    while(token) {
        strcpy(ret_str[jj], token);
        jj++;
        token = strtok(NULL, delimiter_str);
    }
    return jj;
}


int freq_adapter(char str[10],  char * val) {
      char parts1[3][100];
      printf("Frequency:\n%s\n",val);
      char temp[5];
      int e = string_splitter(parts1, val, ".");
      //no point present in the frequency value
      if (e == 1) {
        //printf("No point\n");
        //19 frequency to short
        if (strlen(val)<3){
           perror("Bad frquency value");
           return 1; 
        }
        //strncpy(temp, val, 3);
        //192
        if (strlen(val)==3){
           sprintf(str,"%c%c%c.00", val[0], val[1], val[2]);
           //sprintf(str,"%s%s", temp, ".00");
        }else
        //1925
        if (strlen(val)==4){
           sprintf(str,"%c%c%c.%c0", val[0], val[1], val[2], val[3]);
           //sprintf(str,"%s%s%c%s", temp, ".", val[3], "0");
        }else
        //19250->00000
        if (strlen(val)>4){
           sprintf(str,"%c%c%c.%c%c", val[0], val[1], val[2], val[3], val[4]);
        }
      }else
      //the frequency value includes a point parts1[0]=integer, parts1[1]=decimal
      if (e == 2) {
        //19 frequency to short
        if (strlen(parts1[0])<3){
           perror("Bad frquency value");
           return 1; 
        }
        //strncpy(temp, parts1[0], 3);
        //192
        if (strlen(parts1[0])==3){
           //check decimal if it is longer than 1
           if (strlen(parts1[1])>1){
              sprintf(str,"%c%c%c.%c%c", parts1[0][0], parts1[0][1],parts1[0][2], parts1[1][0], parts1[1][1]);
           }else
           if (strlen(parts1[1])==1){
              sprintf(str,"%c%c%c.%c0", parts1[0][0], parts1[0][1],parts1[0][2], parts1[1][0]);
              //sprintf(str,"%s%s%c%s", temp, ".", parts1[1][0], "0");
           }else
           if (strlen(parts1[1])==0){
              sprintf(str,"%c%c%c.00", parts1[0][0], parts1[0][1],parts1[0][2]);
              //sprintf(str,"%s%s", temp, ".00");
           }
        }else
        //1925
        if (strlen(parts1[0])==4){
           if (strlen(parts1[1])>0){
              sprintf(str,"%c%c%c.%c%c", parts1[0][0], parts1[0][1],parts1[0][2], parts1[0][3], parts1[1][0]);
              //sprintf(str,"%s%s%c%c", temp, ".", parts1[0][3], parts1[1][0]);
           }else
           if (strlen(parts1[1])==0){
              sprintf(str,"%c%c%c.%c0", parts1[0][0], parts1[0][1],parts1[0][2], parts1[0][3]);
              //sprintf(str,"%s%s%c%s", temp, ".", parts1[0][3], "0");
           }
        }else
        //19250->00000
        if (strlen(parts1[0])>4){
           sprintf(str,"%c%c%c.%c%c", parts1[0][0], parts1[0][1],parts1[0][2], parts1[0][3], parts1[0][4]);
           //sprintf(str,"%s%s%c%c", temp, ".", parts1[0][3], parts1[0][4]);
        }
      }
      return 0;
}



int sender(char *type, char *path, char *resp, int portno, char *host){

    if (portno == 0)
        portno = atoi(API_PORT);
    if (host == NULL)
        host = API_IP;

    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd, bytes, sent, received, total, message_size;
    char *message, response[4096];
    message_size=0;

    if(!strcmp(type,"GET"))
    {
        //message_size+=strlen("%s %s%s%s HTTP/1.0\r\n");        /* method         */
        message_size+=strlen("%s %s HTTP/1.1\r\n");            /* method         */
        message_size+=strlen(type);                            /* path           */
        message_size+=strlen(path);                            /* headers        */
        message_size+=strlen("\r\n");                          /* blank line     */
    }
    else
    {
        message_size+=strlen("%s %s HTTP/1.0\r\n");
        message_size+=strlen(type);                            /* method         */
        message_size+=strlen(path);                            /* path           */
        message_size+=strlen("\r\n");                          /* blank line     */
    }

    /* allocate space for the message */
    message=malloc(message_size);

    /* fill in the parameters */
    sprintf(message,"%s %s HTTP/1.1\r\n", type, path);
    strcat(message,"\r\n");                                /* blank line     */

    //printf("Sending REST request\n");

    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    /* lookup the ip address */
    server = gethostbyname(host);
    if (server == NULL) error("ERROR, no such host");

    /* fill in the structure */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

    /* connect the socket */
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    /* send the request */
    total = strlen(message);
    sent = 0;
    do {
        bytes = write(sockfd,message+sent,total-sent);
        if (bytes < 0)
            error("ERROR writing message to socket");
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);

    /* receive the response */
    memset(response,0,sizeof(response));
    total = sizeof(response)-1;
    received = 0;
    do {
        bytes = read(sockfd,response+received,total-received);
        if (bytes < 0)
            error("ERROR reading response from socket");
        if (bytes == 0)
            break;
        received+=bytes;
    } while (received < total);

    if (received == total)
        error("ERROR storing complete response from socket");

    /* close the socket */
    close(sockfd);

    //printf("Response:\n%s\n",response);
    if (resp != NULL)
        strcpy(resp, response);

    /* print response */

    free(message);
    return 0;


}


//int sender(char *type, char *path){
void *sender_th(void *vargp){
    struct senddata *thr_args = vargp;
    char type[BUFSIZ];
    char path[BUFSIZ];
    strcpy(type, thr_args->type);
    strcpy(path, thr_args->path);
    //char *host = API_IP;
    char *host;
    if (thr_args->ip != NULL)
       //strcpy(host, thr_args->ip);
       host = thr_args->ip;
    else
       host = API_IP;
    //int portno = atoi(API_PORT);
    int portno;
    if (thr_args->port != NULL)
       portno = atoi(thr_args->port);
    else
       portno = atoi(API_PORT);

    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd, bytes, sent, received, total, message_size;
    char *message, response[4096];

    message_size=0;

    if(!strcmp(type,"GET"))
    {
        message_size+=strlen("%s %s%s%s HTTP/1.0\r\n");        /* method         */
        message_size+=strlen(type);                            /* path           */
        message_size+=strlen(path);                            /* headers        */
        message_size+=strlen("\r\n");                          /* blank line     */
    }
    else
    {
        message_size+=strlen("%s %s HTTP/1.0\r\n");
        message_size+=strlen(type);                            /* method         */
        message_size+=strlen(path);                            /* path           */
        message_size+=strlen("\r\n");                          /* blank line     */
    }

    /* allocate space for the message */
    message=malloc(message_size);

    /* fill in the parameters */
    sprintf(message,"%s %s HTTP/1.0\r\n", type, path);
    strcat(message,"\r\n");                                /* blank line     */

    printf("Sending REST request\n");

    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");

    /* lookup the ip address */
    server = gethostbyname(host);
    if (server == NULL) error("ERROR, no such host");

    /* fill in the structure */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

    /* connect the socket */
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    /* send the request */
    total = strlen(message);
    sent = 0;
    do {
        bytes = write(sockfd,message+sent,total-sent);
        if (bytes < 0)
            error("ERROR writing message to socket");
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);

    /* receive the response */
    memset(response,0,sizeof(response));
    total = sizeof(response)-1;
    received = 0;
    do {
        bytes = read(sockfd,response+received,total-received);
        if (bytes < 0)
            error("ERROR reading response from socket");
        if (bytes == 0)
            break;
        received+=bytes;
    } while (received < total);

    if (received == total)
        error("ERROR storing complete response from socket");

    /* close the socket */
    close(sockfd);

    /* process response */
    //printf("Response:\n%s\n",response);

    free(message);
    //return 0;
    return NULL;
}



static void *config_communication_job(void *vargp) {
    struct hostent *hostnm;    /* server host name information        */
    struct sockaddr_in server; /* server address                      */
    int sock_id;                     /* client socket                       */
    int rc;
    struct message_s mymsg;


    //
    // Get the server address.
    //
    hostnm = gethostbyname(TRANSPONDER_ADDR);
    if (hostnm == (struct hostent *) 0) {
        fprintf(stderr, "Gethostbyname failed\n");
        pthread_exit(NULL);
    }

    //
    // Put the server information into the server structure.
    // The port must be put into network byte order.
    //
    server.sin_family      = AF_INET;
    server.sin_port        = htons(TRANSPONDER_PORT);
    server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);

    //
    // Get a stream socket.
    //
    if ((sock_id = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket()");
    }

    //
    // Connect to the server.
    //
    while (connect(sock_id, (struct sockaddr *)&server, sizeof(server)) < 0) {
        sleep(5);//sleep 5 seconds and retry
        printf("Transponder Driver - Trying to connect...\n");
    }

    printf("Transponder Driver - Connected\n");

    while ( (rc = msgrcv(msg_queue_id, &mymsg, msg_size, MESSAGE_TYPE, IPC_WAIT)) >= 0) {

        // send the message to the device
        if (send(sock_id, mymsg.mtext, strlen(mymsg.mtext), 0) < 0) {
            perror("Send()");
        }
        printf("Transponder Driver - Sent command: %s\n", mymsg.mtext);
    }


    /*
    if (recv(sock_id, buf, sizeof(buf), 0) < 0) {
        tcperror("Recv()");
        exit(6);
    }*/

    // Close the socket.
    close(sock_id);

    printf("Transponder Driver - Disconnected\n");

    return NULL;
}


void intHardwareCommunication() {
    if(REST==0){
      // Set up the message queue
      msg_queue_id = msgget((key_t)MESSAGE_QUEUE_KEY, 0666 | IPC_CREAT);

      if (msg_queue_id == -1) {
        perror("msgget failed with error");
        exit(1);
      }

      // launch thread for configuring the device
      pthread_create(&config_thread_id, NULL, config_communication_job, NULL);

      printf("Transponder Driver Started\n");
    

    }else{
      char path[2000];
      sprintf(path,"/Monitoring/start");
      //sender("PUT", path);
    }

}

void stopHardwareCommunication() {
    int status;
    status = pthread_kill( config_thread_id, SIGUSR1);
    if ( status <  0)                                                              
      perror("pthread_kill failed");
}

void lch_set_admin_state(uint32_t lch, char const* const val) {
    struct message_s mymsg;

    if (REST==0){
       mymsg.mtype = MESSAGE_TYPE;
       sprintf(mymsg.mtext,"CONFIGOC###%d###ADMINSTATE###%s$$$", lch, val);

       if (msgsnd(msg_queue_id, &mymsg, msg_size, IPC_NOWAIT) != 0) {
         perror("failed admn_state_set"); 
         //maybe queue is full, check msgctl()
       }
    }
    else{
      //shelf+slot+port
      //11811
      uint32_t slot = (int) ((lch-10000)/100);
      //printf("slot: %d\n", slot);
      uint32_t port=lch-10000-(slot*100);
      //printf("port: %d\n", port);
      char path[2000];
      if (port > 10)
         sprintf(path,"/Configuration/%s/Transponder/%d/%d", SPONAME, slot, port);
      else
         sprintf(path,"/Configuration/%s/Port/%d/%d", SPONAME, slot, port);
      //printf("input: %s\n", argv[1]);
      struct senddata *thr_args=  malloc(sizeof(struct senddata));
      strcpy(thr_args->path, path);

      if (strcmp(val,"ENABLE") == 0 ){
        //sender("PUT", path);
        strcpy(thr_args->type, "PUT");
        strcpy(thr_args->ip, API_IP);
        strcpy(thr_args->port, API_PORT);
        //sprintf(path,"/Monitoring/port/%s/%d/%d", SPONAME, slot, port);
        //sender("PUT",path);
      }
      else{
        strcpy(thr_args->type, "DELETE");
        strcpy(thr_args->ip, API_IP);
        strcpy(thr_args->port, API_PORT);
        //sender("DELETE", path);
      }
      pthread_create(&send_pthread, NULL, sender_th, thr_args);
    }
}

void component_set_target_power(char const* const name, char const* const val) {
    struct message_s mymsg;

    if (REST==0){
       mymsg.mtype = MESSAGE_TYPE;
       sprintf(mymsg.mtext,"CONFIG###%s###TARGETPOWER###%s$$$", name, val);

       if (msgsnd(msg_queue_id, &mymsg, msg_size, IPC_NOWAIT) != 0) {
          perror("failed target_power_set"); 
          //maybe queue is full, check msgctl()
       }
    }
}



void component_set_frequency(char const* const name, char const* const val) {
    struct message_s mymsg;
    if (REST==0){
       mymsg.mtype = MESSAGE_TYPE;
       sprintf(mymsg.mtext,"CONFIGOC###%s###FREQUENCY###%s$$$", name, val);

       if (msgsnd(msg_queue_id, &mymsg, msg_size, IPC_NOWAIT) != 0) {
         perror("failed frequency_set"); 
         //maybe queue is full, check msgctl()
       }
    }
    else{
      char parts[3][100];
      char freq[10];
      int a = freq_adapter(freq, (char *) val);
      if (a>0) 
        sprintf(freq,"193.90");
      int n = string_splitter(parts, (char *) name,"-");
      if (n==2) {
        uint32_t lch = strtoul(parts[1], NULL, 10);
        uint32_t slot = (int) ((lch-10000)/100);
        //printf("slot: %d\n", slot);
        uint32_t port=lch-10000-(slot*100);
        //printf("port: %d\n", port);
        char path[2000];
        sprintf(path,"/Configuration/%s/Wave/%d/%d/%s", SPONAME, slot, port, freq);
        printf("path: %s\n", path);
        //sender("PUT", path);
        struct senddata *thr_args=  malloc(sizeof(struct senddata));
        strcpy(thr_args->path, path);
        strcpy(thr_args->type, "PUT");
        strcpy(thr_args->ip, API_IP);
        strcpy(thr_args->port, API_PORT);
        pthread_create(&send_pthread, NULL, sender_th, thr_args);
      }
    }
}

void component_set_operational_mode(char const* const name, char const* const val) {
    struct message_s mymsg;

    if (REST==0){
       mymsg.mtype = MESSAGE_TYPE;
       sprintf(mymsg.mtext,"CONFIGOC###%s###OPERATIONALMODE###%s$$$", name, val);

       if (msgsnd(msg_queue_id, &mymsg, msg_size, IPC_NOWAIT) != 0) {
         perror("failed operational_mode_set"); 
         //maybe queue is full, check msgctl()
       }
    }
}

void error(const char *msg) { perror(msg); exit(0); }


void bgp_config(uint32_t type, char const* const las, char const* const ras, char const* const ip, char const* const intf) {
    if (type == 1){
        char path[2000];
        sprintf(path,"/Sonic/bgp/%s/%s/%s/%s", las, ras, ip, intf);
        printf("path: PUT %s\n", path);
        //sender("PUT", path);
        struct senddata *thr_args=  malloc(sizeof(struct senddata));
        strcpy(thr_args->path, path);
        strcpy(thr_args->type, "PUT");
        strcpy(thr_args->ip, RESTSONIC);
        strcpy(thr_args->port, RESTSONIC_P_BGP);
        if (send_rest == 1) pthread_create(&send_pthread, NULL, sender_th, thr_args);
    }else
    if (type == 0){
        char path[2000];
        sprintf(path,"/Sonic/bgp/neighbor/%s/%s/%s", las, ras, ip);
        printf("path: DELETE %s\n", path);
        struct senddata *thr_args=  malloc(sizeof(struct senddata));
        strcpy(thr_args->path, path);
        strcpy(thr_args->type, "DELETE");
        strcpy(thr_args->ip, RESTSONIC);
        strcpy(thr_args->port, RESTSONIC_P_BGP);
        if (send_rest == 1) pthread_create(&send_pthread, NULL, sender_th, thr_args);

    }
}
///Interface/Add/<string:ip>/<int:mask>/<string:port>

void intf_config(uint32_t type, char const* const ip, char const* const mask, char const* const intf, char const* const speed) {
    if (type == 1){
        char path[2000];
        sprintf(path,"/Sonic/Interface/%s/%s/%s", ip, mask, intf);
        printf("path: PUT %s\n", path);
        //sender("PUT", path);
        struct senddata *thr_args=  malloc(sizeof(struct senddata));
        strcpy(thr_args->path, path);
        strcpy(thr_args->type, "PUT");
        strcpy(thr_args->ip, RESTSONIC);
        strcpy(thr_args->port, RESTSONIC_P_IF);
        if (send_rest == 1) pthread_create(&send_pthread, NULL, sender_th, thr_args);
    } else
    if (type == 0){
        char path[2000];
        sprintf(path,"/Sonic/Interface/%s/%s/%s", ip, mask, intf);
        printf("path: DELETE %s\n", path);
        struct senddata *thr_args=  malloc(sizeof(struct senddata));
        strcpy(thr_args->path, path);
        strcpy(thr_args->type, "DELETE");
        strcpy(thr_args->ip, RESTSONIC);
        strcpy(thr_args->port, RESTSONIC_P_IF);
        if (send_rest == 1) pthread_create(&send_pthread, NULL, sender_th, thr_args);
    } else
    if (type == 11){
        char path[2000];
        sprintf(path,"/Sonic/InterfaceConfig/%s/%s", intf, speed);
        printf("path: PUT %s\n", path);
        //sender("PUT", path);
        struct senddata *thr_args=  malloc(sizeof(struct senddata));
        strcpy(thr_args->path, path);
        strcpy(thr_args->type, "PUT");
        strcpy(thr_args->ip, RESTSONIC);
        strcpy(thr_args->port, RESTSONIC_P_IF);
        if (send_rest == 1) pthread_create(&send_pthread, NULL, sender_th, thr_args);
    }else
    if (type == 21){
        char path[2000];
        sprintf(path,"/Sonic/InterfaceStatus/%s", intf);
        printf("path: PUT %s\n", path);
        //sender("PUT", path);
        struct senddata *thr_args=  malloc(sizeof(struct senddata));
        strcpy(thr_args->path, path);
        strcpy(thr_args->type, "PUT");
        strcpy(thr_args->ip, RESTSONIC);
        strcpy(thr_args->port, RESTSONIC_P_IF);
        if (send_rest == 1) pthread_create(&send_pthread, NULL, sender_th, thr_args);
    }else
    if (type == 20){
        char path[2000];
        sprintf(path,"/Sonic/InterfaceStatus/%s", intf);
        printf("path: DELETE %s\n", path);
        //sender("PUT", path);
        struct senddata *thr_args=  malloc(sizeof(struct senddata));
        strcpy(thr_args->path, path);
        strcpy(thr_args->type, "DELETE");
        strcpy(thr_args->ip, RESTSONIC);
        strcpy(thr_args->port, RESTSONIC_P_IF);
        if (send_rest == 1) pthread_create(&send_pthread, NULL, sender_th, thr_args);
    }
}

