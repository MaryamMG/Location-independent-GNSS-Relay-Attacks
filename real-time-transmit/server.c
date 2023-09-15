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


int udprecv(int s, void *data, int size)
{
    struct sockaddr from;
    return recvfrom(s, data, size, 0, &from, 0);
}

int udpinit(short port)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(7531);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect");
        exit(1);
    }
    return sock;
}

void main()
{
    
    double data[10];

    int sock = udpinit(7531);
    
    printf("Receiving\n");
    struct sockaddr from;

    while (1)
    {
        udprecv(sock, data, 10*sizeof(double));
        printf("\nReceived: %f %d", data[9], (int)data[0]);
    }
}
