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
#include <pthread.h>

#define BUF_LEN (2048)
#define MAX_UDP_CNT (100 * 1000 * 1000) //1亿 100MB

typedef struct system_time_s {
	time_t sec;
	uint32_t msec;
} system_time_t;

uint64_t
get_current_msec()
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    time_t sec = tv.tv_sec;
    unsigned int msec = tv.tv_usec / 1000;

    uint64_t current_msec = (uint64_t) sec * 1000 + msec;
    //printf("sec %lu usec %u current_msec %lu\n", (uint64_t)sec, tv.tv_usec, current_msec);
    return current_msec;

}


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

struct sockaddr_in addr;
int sock;

char *host;
char *port;

uint32_t udp_len = 1400;
uint32_t min_ping_interval = 10; // 毫秒
uint32_t max_ping_interval = 100; // 毫秒
uint32_t udp_cnt = MAX_UDP_CNT;

struct timeval start, end;
int send_packet_cnt = 0;

void *
send_thread()
{
    //printf("start send\n");
    uint32_t i, j;
    char current[100];
    for (i = 0; i < udp_cnt; i++) {
        unsigned char send_buff[BUF_LEN];
        uint32_t send_sn = i;
        for (j = 0; j < udp_len - 8; j += 4) {
            *(uint32_t *) (send_buff + j) = send_sn;
        }
        uint64_t current_mesec = get_current_msec();
        memcpy(send_buff + udp_len - 8, &current_mesec, sizeof(current_mesec));
        send_packet_cnt++;
        int n = sendto(sock, send_buff, udp_len, 0, (struct sockaddr *)&addr, sizeof(addr));
        //printf("send_sn %u start_ts %ld\n", send_sn, current_mesec);
        if (n < 0)
        {
            perror("sendto");
            close(sock);
            exit(1);
        }
        uint32_t ping_interval = (min_ping_interval + max_ping_interval) / 2;
        usleep(ping_interval * 1000);
    }
    pthread_exit(NULL);
}

void *
recv_thread()
{
    //printf("start recv\n");
    char current[100];
    int recv_packet_cnt = 0;
    int long_time_cnt = 0;
    uint8_t *recv_flag = calloc(udp_cnt, sizeof(uint8_t));
    if (NULL == recv_flag) {
        printf("udp_cnt %u 内存分配失败！！！\n");
        exit(1);
    }
    uint32_t i;
    while(1) {
        int n;
        get_now_time(current);
        unsigned char recv_buff[BUF_LEN] = {0};
        unsigned int len = sizeof(addr);
        n = recvfrom(sock, recv_buff, BUF_LEN - 1, 0, (struct sockaddr *)&addr, &len);
        if (n < 0) {
            perror("recv error!!");
            exit(1);
        }
        if (0 == n) {
            printf("n == 0!!!!\n");
            continue;
        }
        if (n != (int)udp_len) {
            printf("get wrong udp packet n = %d udp_len %u!!!!\n", n, udp_len);
            exit(1);
        }
        uint32_t recv_sn = *(uint32_t *) (recv_buff + 0);
        uint32_t no_send_sn = send_packet_cnt;
        if (recv_sn >= no_send_sn) {
            printf("get wrong udp packet recv_sn %u no_send_sn %u!!!!\n",
                recv_sn, no_send_sn);
            exit(1);
        }
        if (recv_flag[recv_sn] > 0) {
            printf("get duplicate udp packet recv_sn %u!!!!\n", recv_sn);
            exit(1);
        }

        for (i = 0; i < udp_len - 8; i += 4) {
            uint32_t content = *(uint32_t *) (recv_buff + i);
            if (content != recv_sn){
                printf("get wrong udp packet recv_sn %u content %u i %u!!!!\n",
                    recv_sn, content, i);
                exit(1);
            }
        }
        uint64_t start_ts = *(uint64_t *) (recv_buff + udp_len - 8);
        uint64_t end_ts = get_current_msec();
        uint64_t ms = end_ts - start_ts;
        //printf("recv_sn %u start_ts %ld end_ts %ld\n", recv_sn, start_ts, end_ts);
        if (end_ts < start_ts) {
            printf("start timestamp error!!!!\n");
            exit(1);
        }
        recv_flag[recv_sn] = 1;
        recv_packet_cnt++;
        if (ms >= 200) {
            long_time_cnt++;
        }
        printf("%s 服务器：%s:%s, 报文序号：%7d, 大小：%d 字节, 延迟：%4ld 毫秒, "
                "已发数: %7d, 已收数: %7d, 未收数：%4d, 高延迟包数(>200ms)：%4d\n",
               current, host, port, recv_sn, udp_len, ms, send_packet_cnt, recv_packet_cnt,
               send_packet_cnt - recv_packet_cnt, long_time_cnt);
        if (recv_packet_cnt == udp_cnt) {
            printf("接收完成！！！\n");
            exit(0);
        }
    }

    pthread_exit(NULL);
}


int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: %s ip port [udp_len] [udp_cnt] [min_ping_interval] [max_ping_interval]\n", argv[0]);
        printf("该程序会向目标(ip:port)发送定长的UDP报文进行探测，探测的参数如下:\n");
        printf("[udp_len] 每个UDP报文的字节数，最小值为12，最大值为1400，必须是4的倍数\n");
        printf("[udp_cnt] 发送的报文数\n");
        printf("[min_ping_interval] 最小探测时间间隔，单位毫秒\n");
        printf("[max_ping_interval] 最大探测时间间隔，单位毫秒\n");
        exit(1);
    }
    printf("This is a UDP client\n");

    if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }
    host = argv[1];
    port = argv[2];
    if (argc >= 4) udp_len = atoi(argv[3]);
    if (argc >= 5) udp_cnt = atoi(argv[4]);
    if (argc >= 6) min_ping_interval = atoi(argv[5]);
    if (argc >= 7) max_ping_interval = atoi(argv[6]);

    if (0 == udp_cnt || udp_cnt > MAX_UDP_CNT) {
		printf("发送的报文数%u 必须大于0，小于%u!!\n", udp_cnt, MAX_UDP_CNT);
		exit(1);
    }
	if (min_ping_interval > max_ping_interval) {
		printf("最小探测时间间隔 %u > 最大时间间隔 %u\n", min_ping_interval, max_ping_interval);
		exit(1);
	}
	if (udp_len < 12 || udp_len > 1400 || 0 != udp_len % 4) {
		printf("字节数 %u，最小值为12，最大值为1400，必须是4的倍数\n", udp_len);
		exit(1);
	}

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    addr.sin_port = htons(atoi(port));
    if (INADDR_NONE == addr.sin_addr.s_addr)
    {
        printf("非法IP %s!\n", host);
        close(sock);
        exit(1);
    }
    /*
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
         printf("socket option SO_RCVTIMEO not support\n");
         exit(1);
    }
    */
    printf("UDP服务器：%s:%s, 报文大小：%u 字节, 报文数：%u, "
            "探测时间间隔：[%u, %u] 毫秒\n",
           host, port, udp_len, udp_cnt, min_ping_interval, max_ping_interval);
    pthread_t thread[2];
    int ret;
    memset(&thread, 0, sizeof(thread));
    ret = pthread_create(&thread[0], NULL, send_thread, NULL);
    if (ret != 0) {
       printf("线程1创建失败!\n");
       exit(1);
    }
    ret = pthread_create(&thread[1], NULL, recv_thread, NULL);
    if (ret != 0) {
       printf("线程2创建失败!\n");
       exit(1);
    }
    if(thread[0] != 0)
    {
        pthread_join(thread[0],NULL);
        printf("发送线程已经结束/n");
    }
    if(thread[1] != 0)
    {
       pthread_join(thread[1],NULL);
        printf("接收线程已经结束/n");
    }

    return 0;
}
