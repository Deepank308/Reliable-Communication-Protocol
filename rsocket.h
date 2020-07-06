#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <unistd.h>
#include <time.h> 

// drop message utility
#define DROP 1
#define DONTDROP 0

// message types
#define APP 0
#define ACK 1
#define RETRANS 2

// transmissions status
#define DONE 1
#define NOTDONE 0

// general socket utility
#define T 2                     // timeout(in secs)
#define P 0.10                  // drop probability
#define T_EPS 0.005             // delta epsilon time for precision
#define SIG_T 10000             // timer alarm interval time(in microsecs)
#define SOCK_MRP SOCK_DGRAM
#define SR_FLAG MSG_DONTWAIT

// memory sizes
#define MAX_MSG_SZ 105
#define MAX_BUFF_SZ 105
#define TBL_SZ 105

#define ERR -1
#define FOUND 1


int STATUS, transmissions, r_sockfd;

typedef struct message{

    int ID;
    int if_ack;
    
    struct sockaddr r_addr, s_addr;
    socklen_t r_addr_len, s_addr_len;
    
    char data[MAX_MSG_SZ];
    size_t data_len;

} Message;

typedef struct unack_msg{

    double lastsent_t;
    Message msg;

} UnACKMsg;

typedef struct unack_message_table{

    size_t size;
    UnACKMsg *table;

} UnACKTble;

typedef struct buffer{

    size_t size;
    Message *buff;

} Buffer;

typedef struct recvd_messages_tbl{

    size_t size;
    int *id_tbl;

} RecvdMsg;

// simulate dropping of messages in SignalHandler
int dropMessage();

// calls socket() and initializes all variables
int r_socket(const int __domain, const int __type, const int __protocol);

// Give the socket FD the local address ADDR (which is LEN bytes long).
int r_bind(int __fd, const struct sockaddr *__addr, socklen_t __len);

/*
Send N bytes of BUF on socket FD to peer at address ADDR (which is
ADDR_LEN bytes long). Returns the number sent, or -1 for errors.
*/
ssize_t r_sendto(int __fd, const char *__buf, size_t __n, int __flag,
                const struct sockaddr *__addr, socklen_t __addr_len);

/* 
Read message into BUF through socket FD.
If ADDR is not NULL, fill in *ADDR_LEN bytes of it with tha address of
the sender, and store the actual size of the address in *ADDR_LEN.
Returns the number of bytes read or -1 for errors.
*/
ssize_t r_recvfrom(int __fd, char **__restrict__ __buf, size_t __n, int __flag,
                    struct sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len);

// Close the file descriptor FD.
int r_close(int __fd);


// signal handlers
void SIGNALHANDLER();

void HandleReceive();
void HandleACKMsgRecv(Message *);
void HandleAppMsgRecv(Message *);

void HandleRetransmit();

void HandleTransmit();

void IFDONE();

void remove_timer();
void SIG_CLOSE();