#include <sys/types.h>
#include <sys/socket.h>
#include<pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

int time_subtract(struct timeval *x, struct timeval *y)
{
    int ms;
    if ( x->tv_sec > y->tv_sec )
        return -1;
    if ((x->tv_sec==y->tv_sec) && (x->tv_usec > y->tv_usec))
        return -1;

    int sec = y->tv_sec-x->tv_sec;
    int us = y->tv_usec-x->tv_usec;

    if (us < 0)
    {
        sec--;
        us += 1000000;
    }
    ms = sec * 1000;
    ms += us / 1000;

    return ms;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: %s ip port", argv[0]);
        exit(1);
    }
    printf("This is a UDP client\n");
    struct sockaddr_in addr;
    int sock;

    if ( (sock=socket(AF_INET, SOCK_DGRAM, 0)) <0)
    {
        perror("socket");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    addr.sin_addr.s_addr = inet_addr(argv[1]);
    if (addr.sin_addr.s_addr == INADDR_NONE)
    {
        printf("Incorrect ip address!");
        close(sock);
        exit(1);
    }

    struct timeval tv;  
    tv.tv_sec = 2;  
    tv.tv_usec = 0;  
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {  
         printf("socket option  SO_RCVTIMEO not support\n");  
         return;  
    }

    const int BUF_LEN = 2048;
    const int SEND_LEN = 1400;
    int i;
    char buff[BUF_LEN];
    int len = sizeof(addr);
    struct timeval start, end;
    int packet_cnt = 0;
    int loss_cnt = 0;
    int long_time_cnt = 0;
    while (1)
    {
        for (i = 0; i < SEND_LEN; i++) { buff[i] = 'a'; }
        buff[i] = 0;
        //printf("buff %s\n", buff);
        //exit(0);
        int n;
        gettimeofday(&start, 0);
        n = sendto(sock, buff, SEND_LEN, 0, (struct sockaddr *)&addr, sizeof(addr));
        //printf("sent %d bytes\n", n);
        if (n < 0)
        {
            perror("sendto");
            close(sock);
            break;
        }
        n = recvfrom(sock, buff, BUF_LEN - 1, 0, (struct sockaddr *)&addr, &len);
        if (n>0)
        {
            buff[n] = 0;
            //printf("received %d bytes\n", n);
            //puts(buff);
        }
        else if (n == 0)
        {
            //printf("server closed\n");
            //close(sock);
            //break;
        }
        else if (n == -1)
        {
            //perror("recvfrom");
            //close(sock);
            //break;
        }
        //printf("res %d\n", n);
        //usleep(4500);
        gettimeofday(&end, 0);
        int ms = time_subtract(&start, &end);
        packet_cnt++;
        if (n < SEND_LEN) {
            loss_cnt++;
            ms = -1;
        } else {
            if (ms >= 200) {
                long_time_cnt++;
            }
        }
        printf("%4d ms, %6d total packets, %6d loss packets, %6d >200ms packets\n", ms, packet_cnt, loss_cnt, long_time_cnt);
        sleep(1);
    }
    
    return 0;
}
