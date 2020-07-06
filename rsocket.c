#include "rsocket.h"

struct sockaddr my_addr;

// tables and buffers
Buffer send_buff;
Buffer recv_buff;
UnACKTble unack_tbl;
RecvdMsg recvd_tbl; 

int dropMessage()
{
    double r_num = (double)rand() / (double)RAND_MAX;

    if(r_num < P) return DROP;
    else return DONTDROP;
}

int r_socket(const int __domain, const int __type, const int __protocol)
{
    r_sockfd = ERR;
    if(__type != SOCK_MRP)
    {
        printf("Invalid socket type. Expected: 'SOCK_MRP'\n");
        return ERR;
    }
    
    // init timer params
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = SIG_T;

    struct itimerval itimer;
    itimer.it_interval = tv;
    itimer.it_value = tv;

    srand(getpid());

    // allocate memory
    
    // buffers
    send_buff.buff = (Message *)malloc(sizeof(Message)*MAX_BUFF_SZ);
    memset(send_buff.buff, 0, sizeof(Message)*MAX_BUFF_SZ);
    recv_buff.buff = (Message *)malloc(sizeof(Message)*MAX_BUFF_SZ);
    memset(recv_buff.buff, 0, sizeof(Message)*MAX_BUFF_SZ);
    send_buff.size = recv_buff.size = 0;
    
    // tables
    unack_tbl.table = (UnACKMsg *)malloc(sizeof(UnACKMsg)*TBL_SZ);
    memset(unack_tbl.table, 0, sizeof(UnACKMsg)*TBL_SZ);
    recvd_tbl.id_tbl = (int *)malloc(sizeof(int)*TBL_SZ);
    memset(recvd_tbl.id_tbl, 0, sizeof(int)*TBL_SZ);
    recvd_tbl.size = unack_tbl.size = 0;
    
    // bind signals to handlers
    if(signal(SIGALRM, SIGNALHANDLER) == SIG_ERR)
    {
        perror("Error binding SignalHandler");
        exit(EXIT_FAILURE);
    }

    // timer alarm
    if(setitimer(ITIMER_REAL, &itimer, NULL) < 0)
    {
        fprintf(stderr, "Setitimer failed to initialize\n");
        exit(EXIT_FAILURE);
    }
    
    // bind ctrl+c to handle correctly
    if(signal(SIGINT, SIG_CLOSE) == SIG_ERR)
    {
        perror("Error binding SignalHandler");
        exit(EXIT_FAILURE);
    }

    STATUS = NOTDONE;
    transmissions = 0;

    r_sockfd = socket(__domain, SOCK_MRP, __protocol);

    return r_sockfd;  
}

int r_bind(int __fd, const struct sockaddr *__addr, socklen_t __len)
{
    my_addr = *__addr;

    return bind(__fd, __addr, __len);
}

ssize_t r_sendto(int __fd, const char *__buf, size_t __n, int __flag,
                const struct sockaddr *__addr, socklen_t __addr_len)
{
    static int id_count = 0;
    if(__fd != r_sockfd)
    {
        printf("Invalid Socket Descriptor.\n");
        return ERR;
    }
    
    // assuming the buff to fit in one message.
    Message msg;
    strncpy(msg.data, __buf, __n);
    
    // message fields
    msg.if_ack = APP;
    msg.data_len = __n;

    msg.r_addr = *__addr;
    msg.r_addr_len = __addr_len;

    msg.s_addr = my_addr;
    msg.s_addr_len = sizeof(my_addr);
    
    // block until space available
    while(send_buff.size >= MAX_BUFF_SZ);

    msg.ID = id_count;
    id_count++;
    
    // add the message to the send buffer
    send_buff.buff[send_buff.size] = msg;
    send_buff.size++;

    return 0;
}

ssize_t r_recvfrom(int __fd, char **__restrict__ __buf, size_t __n, int __flag,
                    struct sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len)
{
    if(__fd != r_sockfd)
    {
        printf("Invalid Socket Descriptor.\n");
        return ERR;
    }

    // check if message exists and return
    while(recv_buff.size == 0);
    Message msg = recv_buff.buff[0];

    // printf("Recv ID: %d\n", msg.ID);

    // update the buff and other fields
    strncpy(*__buf, msg.data, __n);
    __addr = (struct sockaddr *__restrict__)&msg.s_addr;
    __addr_len = &msg.s_addr_len;

    // delete message from receive buffer
    size_t i = 0;
    for(i = 1; i < recv_buff.size; i++) recv_buff.buff[i - 1] = recv_buff.buff[i];
    recv_buff.size--;

    return msg.data_len;
}

int r_close(int __fd)
{
    // free-up any dynamic memory 
    free(send_buff.buff);
    free(recv_buff.buff);
    free(unack_tbl.table);
    free(recvd_tbl.id_tbl);

    remove_timer();

    return close(__fd);
}


/******************************** 
    Signal Handling functions
********************************/
void SIGNALHANDLER()
{
    // call recvfrom() and get the next message
    // handle cases of ACK/APP
    HandleReceive();

    // retransmit msg due to message loss after timeout
    HandleRetransmit();

    // sends APP/ACK message using sendto() call
    HandleTransmit();

    IFDONE();
    
    return;
}

void HandleReceive()
{
    Message r_msg;

    int r_size = 0;
    if((r_size = recvfrom(r_sockfd, &r_msg, sizeof(Message), SR_FLAG, NULL, NULL)) == ERR) return;
    else if(r_size == 0) return;

    if(dropMessage() == DROP)
    {
        // printf("Dropped ID: %d\n", r_msg.ID);
        return;
    }

    if(r_msg.if_ack == ACK)
    {
        HandleACKMsgRecv(&r_msg);
    }
    else
    {
        HandleAppMsgRecv(&r_msg);
    }

    return;
}

void HandleACKMsgRecv(Message *ack)
{
    // check if duplicate
    size_t i = 0;
    int found = 0;
    for(i = 0; i < unack_tbl.size; i++)
    {
        if(ack->ID == unack_tbl.table[i].msg.ID)
        {
            found = FOUND;
            break;
        }
    }

    if(!found) return;

    // if not duplicate, delete the message
    unack_tbl.size--;
    while(i < unack_tbl.size)
    {
        unack_tbl.table[i] = unack_tbl.table[i + 1];
        i++;
    }

    // printf("ACK ID: %d\n", ack->ID);
    return;
}

void HandleAppMsgRecv(Message *app)
{
    // check for duplicate
    size_t i = 0;
    int found = 0;
    for(i = 0; i < recvd_tbl.size; i++)
    {
        if(app->ID == recvd_tbl.id_tbl[i])
        {
            found = FOUND;
            break;
        }
    }

    // if it's a new message
    if(!found)
    {
        recv_buff.buff[recv_buff.size] = *app;
        recv_buff.size++;

        // update receive table
        recvd_tbl.id_tbl[recvd_tbl.size] = app->ID;
        recvd_tbl.size = (recvd_tbl.size + 1)%TBL_SZ;
    }

    // printf("APP ID: %d\n", app->ID);

    // SEND BUFF MAY BE FULL
    if(send_buff.size >= MAX_BUFF_SZ) return;

    // send ACK
    app->if_ack = ACK;

    app->r_addr = app->s_addr;
    app->r_addr_len = app->s_addr_len;

    app->s_addr = my_addr;
    app->s_addr_len = sizeof(my_addr);

    send_buff.buff[send_buff.size] = *app;
    send_buff.size++;

    return;
}

void HandleRetransmit()
{
    // SEND BUFF MAY BE FULL
    if(send_buff.size >= MAX_BUFF_SZ) return;
    
    // check for timeout
    size_t i = 0;
    for(i = 0; i < unack_tbl.size; i++)
    {
        if(time(NULL) - unack_tbl.table[i].lastsent_t > T - T_EPS)
        {
            unack_tbl.table[i].lastsent_t = time(NULL);
            unack_tbl.table[i].msg.if_ack = RETRANS;

            send_buff.buff[send_buff.size] = unack_tbl.table[i].msg;
            send_buff.size++;

            // printf("ReTransmit ID: %d\n", unack_tbl.table[i].msg.ID);
        }
    }

    return;
}

void HandleTransmit()
{
    while(unack_tbl.size != TBL_SZ && send_buff.size)
    {
        size_t i = 0;
        int min_id = 10001, min_idx = -1;
        for(i = 0; i < send_buff.size; i++)
        {
            if(min_id > send_buff.buff[i].ID)
            {
                min_id = send_buff.buff[i].ID;
                min_idx = i;
            }
        }

        Message msg = send_buff.buff[min_idx];

        int sent = 0;
        if((sent = sendto(r_sockfd, &msg, sizeof(msg), SR_FLAG, &msg.r_addr, msg.r_addr_len)) <= 0)
        {
            perror("Error sending.");
            exit(EXIT_FAILURE);
        }

        // number of transmissions(includes ACK) to completely send the data
        transmissions++;

        // add msg to Un-ACK Table, if not ACK or RETRANS
        if(msg.if_ack == APP)
        {
            UnACKMsg unack_msg;
            unack_msg.lastsent_t = time(NULL);
            unack_msg.msg = msg;
            unack_tbl.table[unack_tbl.size] = unack_msg;
            unack_tbl.size++;
        }

        // remove msg from send buffer
        send_buff.size--;
        while(min_idx < send_buff.size)
        {
            send_buff.buff[min_idx] = send_buff.buff[min_idx + 1];
            min_idx++;
        }

        // printf("Sent ID: %d\n", msg.ID);
    }

    return;
}

void IFDONE()
{
    if(send_buff.size == 0 && unack_tbl.size == 0) STATUS = DONE;
    else STATUS = NOTDONE;

    return;
}

void remove_timer()
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    struct itimerval itimer;
    itimer.it_interval = tv;
    itimer.it_value = tv;
    
    // remove the timer
    setitimer(ITIMER_REAL, &itimer, NULL);

    printf("Timer Removed...\n");
}

void SIG_CLOSE()
{
    fflush(NULL);

    remove_timer();

    if(r_sockfd != ERR) close(r_sockfd);
    exit(EXIT_SUCCESS);
}