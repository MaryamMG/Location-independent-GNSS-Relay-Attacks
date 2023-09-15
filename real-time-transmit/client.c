#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/time.h>

void main()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(7531);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    double data[10] = {0,0,0,0,0,0,0,0,0,198823.23134};

    while (1)
    {
        sendto(sock, data,10*sizeof(double), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr));
        sleep(2);
        printf("Data sent\n");
    }
    
}