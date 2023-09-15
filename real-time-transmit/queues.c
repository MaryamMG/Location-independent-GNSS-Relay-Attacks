#include <glib.h>
#include <stdio.h>

void main()
{
    GQueue* q[10];
    
    for (int i=0;i<10;i++)
    {
        q[i] = g_queue_new();
        g_queue_push_tail(q[i], "0");
    } 
    double b = 1;

    if (b == 1)
    {
        printf("Hello %f\n", b);
    }
}