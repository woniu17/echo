#include <sys/types.h>
#include <sys/socket.h>
#include<pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <time.h>

void get_now_time(char cur[])
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);  //获取相对于1970到现在的秒数
    struct tm t;
    localtime_r(&time.tv_sec, &t);
    sprintf(cur, "%04d/%02d/%02d %02d:%02d:%02d", t.tm_year + 1900, t.tm_mon+1, t.tm_mday,
      t.tm_hour, t.tm_min, t.tm_sec);
}

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
    if (argc < 3)
    {
        printf("Usage: %s ip port [data_len] [ping_interval]", argv[0]);
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
    char *host = argv[1];
    char *port = argv[2];
    char *data_len = "1400";
    char *interval = "0";
    if (argc >= 4) data_len = argv[3];
    if (argc >= 5) interval = argv[4];

    const int BUF_LEN = 2048;
    int SEND_LEN = 1400;
    int ping_interval = 0;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    addr.sin_port = htons(atoi(port));
    SEND_LEN = atoi(data_len);
    ping_interval = atoi(interval) * 1000;
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
         return 0;
    }

    int i;
    char current[100];
    char send_buff[BUF_LEN];
    char recv_buff[BUF_LEN];
    unsigned int len = sizeof(addr);
    struct timeval start, end;
    int packet_cnt = 0;
    int loss_cnt = 0;
    int long_time_cnt = 0;
    int d = port[0];
    d = 'a';
    printf("UDP包数据内容：%d*%d, 探测间隔：%s 毫秒\n", d, SEND_LEN, interval);
    while (1)
    {
        for (i = 0; i < SEND_LEN; i++) { send_buff[i] = d + i; }
        send_buff[i] = 0;
        int n;
        get_now_time(current);
        gettimeofday(&start, 0);
        n = sendto(sock, send_buff, SEND_LEN, 0, (struct sockaddr *)&addr, sizeof(addr));
        //printf("sent %d bytes\n", n);
        if (n < 0)
        {
            perror("sendto");
            close(sock);
            break;
        }
        n = recvfrom(sock, recv_buff, BUF_LEN - 1, 0, (struct sockaddr *)&addr, &len);
        gettimeofday(&end, 0);
        int ms = time_subtract(&start, &end);
        packet_cnt++;
        if (n < SEND_LEN) {
            loss_cnt++;
            ms = -1;
        } else {
            if (memcmp(recv_buff, send_buff, n)) {
                printf("error!!!!\n");
            }
            if (ms >= 200) {
                long_time_cnt++;
            }
        }
        printf("%s UDP服务器：%s:%s, 数据大小：%d 字节, 本次延迟：%3d 毫秒, 累计发包数: %4d, 累计丢包数：%4d, 高延迟包数(>200ms)：%4d\n",
               current, host, port, SEND_LEN, ms, packet_cnt, loss_cnt, long_time_cnt);
        usleep(ping_interval);
    }
    return 0;
}
