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
#include <glib.h>

/*
WARNING - HARDCODE CHANNEL TO PRN MAP FOR STREAM TX AS WELL AS STREAM RX - THERE IS NO CHANNEL TO PRN CHECK AS OF NOW
******************** TODO **********************
*/

struct hash_queue {
    GQueue* *queue;
    GHashTable* hashtable;
};

int sockinit(short port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect");
        exit(1);
    }

    return sock;
}

void sockclose(int s)
{
    close(s);
}

void socksend(int s, void *data, int siz)
{
    send(s, data, siz, 0);
}

long int timem()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000 + t.tv_usec / 1000;
}

int udpinit(short port)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect");
        exit(1);
    }
    return sock;
}

int udprecv(int s, void *data, int size)
{
    struct sockaddr from;
    return recvfrom(s, data, size, 0, &from, 0);
}

int raw_bits[MAX_CHAN];
double llhr[3] = {42.34937020,-71.08817058,100};
double dynamic_dt = 0;

double tow_correction;
bool tow_rx;
bool tow_fix;
bool use_hash = false;


void bitstreamer_thread(void *in_struct)
{
    struct hash_queue *hq = in_struct;
    GQueue* *queues = hq->queue;
    GHashTable* sm = hq->hashtable;


    printf("Listening for bit stream\n");

    printf("Initializing %d channels with 0 bit\n", MAX_CHAN);

    int s = udpinit(7531);
    tow_rx = false;
    tow_fix = false;
    char chan_str[20];
    char sat_prn_str[20];
    int sat_prn =0;
    int bit = 0;
    int assigned_chan = 0;
    int content;

    while (1)
    {
        udprecv(s, raw_bits, MAX_CHAN * sizeof(int));
        
        for (int i = 0; i< MAX_CHAN; i++)
        {
            content = raw_bits[i];

            sat_prn = content / 10;
            bit = content % 10;
            
            sprintf(sat_prn_str,"%d",sat_prn);


            if (g_hash_table_contains(sm, g_strdup(sat_prn_str)))
            {
                assigned_chan = atoi(g_hash_table_lookup(sm, g_strdup(sat_prn_str)));
                 
                if (bit == 1)
                    {g_queue_push_tail(queues[assigned_chan], "1");}
                else if (bit == 0)
                    {g_queue_push_tail(queues[assigned_chan], "-1");}
                else
                    {g_queue_push_tail(queues[assigned_chan], "0");}

            }
            
        }

        if (!tow_rx)
        {
            udprecv(s, &tow_correction, sizeof(double));
            printf("TOW Correction %f: ", tow_correction);
            tow_fix = true;
            tow_rx = true;

        }
    }
}

void dt_thread()
{
    printf("Listening for range corrections stream\n");

    int s = udpinit(7532);
    
    while (1)
    {
        udprecv(s, &dynamic_dt, sizeof(double));
    }
}


void locations_thread()
{
        printf("Listening for range corrections stream\n");

    int s = udpinit(7533);
    
    while (1)
    {
        udprecv(s, llhr, 3*sizeof(double));
        printf("Spoofing location: %f,%f,%f\n", llhr[0], llhr[1], llhr[2]);
    }
}
