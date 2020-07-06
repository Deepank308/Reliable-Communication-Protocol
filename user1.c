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
    servaddr.sin_port = htons(56022);

    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = INADDR_ANY;
    cliaddr.sin_port = htons(56023);

    // Bind the socket with the server address 
    if(r_bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    }

    // read input from user
    char str[MAXLINE];
    
    char c[2];
    while(1)
    {
        printf("Enter a string:\n");
        scanf("%s", str);
        int i = 0;
        
        while(i < strlen(str))
        {   
            c[0] = str[i];
            c[1] = '\0';

            r_sendto(sockfd, (char *)c, sizeof(c), MSG_DONTWAIT, 
                    (const struct sockaddr *) &cliaddr, sizeof(cliaddr));
            i++;
        }
    }
    printf("Average no. of transmissions: %f\n", ((double)transmissions)/strlen(str));

    r_close(sockfd);
    return 0;
}
