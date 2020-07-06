#include "rsocket.h"

#define MAXLINE 128

int main(int argv, char **argc)
{
    int sockfd; 
    struct sockaddr_in servaddr, cliaddr; 
  
    // Creating socket file descriptor 
    sockfd = r_socket(AF_INET, SOCK_MRP, 0);
    if(sockfd < 0)
    { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    }
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }
  
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
      
    // Server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(56023);

    // Bind the socket with the server address 
    if(r_bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    }
    
    char *buff;
    buff = (char *)malloc(sizeof(char)*MAXLINE);

    while(1)
    {
        socklen_t len;
        memset(buff, 0, sizeof(char)*MAXLINE);
        int recv = r_recvfrom(sockfd, &buff, MAXLINE, MSG_DONTWAIT, (struct sockaddr *)&cliaddr, &len);
        
        if(recv <= 0) continue;
        printf("%s", buff);
        fflush(stdout);
    }

    printf("\n");
    r_close(sockfd);
    return 0;
}