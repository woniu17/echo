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
#define MAX_UDP_CNT (100)

#define UDP_LEN_SIZE (4)
#define UDP_SN_SIZE (4)
#define UDP_TS_SIZE (8)
#define UDP_HEADER_SIZE (UDP_LEN_SIZE + UDP_SN_SIZE + UDP_TS_SIZE)

#define UDP_LEN_OFFSET (0)
#define UDP_SN_OFFSET (UDP_LEN_SIZE)
#define UDP_TS_OFFSET (UDP_LEN_SIZE + UDP_SN_SIZE)
#define UDP_DATA_OFFSET (UDP_HEADER_SIZE)

#define MIN_UDP_LEN (UDP_HEADER_SIZE)
#define MAX_UDP_LEN (1400)

#define MAX_RECV_TIME (4000)

#define max(val1, val2) ((val1 < val2) ? (val2) : (val1))
#define min(val1, val2) ((val1 > val2) ? (val2) : (val1))

struct sockaddr_in addr;
int sock;

char *host;
char *port;

uint32_t min_udp_len = MIN_UDP_LEN;
uint32_t udp_len_delta = 0;
uint32_t udp_cnt = MAX_UDP_CNT;

uint32_t send_packet_cnt = 0;
uint32_t recv_packet_cnt = 0;

int send_flag = 1;

pthread_mutex_t print_mutex;

uint64_t
get_current_msec()
{
    struct timeval tv;
    gettimeofday(&tv, 0);

    time_t sec = tv.tv_sec;
    unsigned int msec = tv.tv_usec / 1000;

    uint64_t current_msec = (uint64_t) sec * 1000 + msec;
    return current_msec;

}

void
get_now_time(char cur[])
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);  //获取相对于1970到现在的秒数
    struct tm t;
    localtime_r(&time.tv_sec, &t);
    sprintf(cur, "%04d/%02d/%02d %02d:%02d:%02d", t.tm_year + 1900, t.tm_mon+1,
      t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
}

uint32_t
get_udp_len()
{
    if (udp_len_delta > 0) {
        return min(min_udp_len + udp_len_delta * send_packet_cnt, MAX_UDP_LEN);
    }

    return min_udp_len;
}

void *
send_thread()
{
    uint32_t i, j;
    for (i = 0; i < udp_cnt; i++) {
        while(0 == send_flag) {
        }
        unsigned char send_buff[BUF_LEN];
        uint32_t send_len = get_udp_len();
        uint32_t send_sn = i;
        uint64_t current_mesec = get_current_msec();

        *(uint32_t *) (send_buff + UDP_LEN_OFFSET) = send_len;
        *(uint32_t *) (send_buff + UDP_SN_OFFSET) = send_sn;
        *(uint64_t *) (send_buff + UDP_TS_OFFSET) = current_mesec;
        for (j = UDP_DATA_OFFSET; j < send_len; j += 4) {
            *(uint32_t *) (send_buff + j) = send_sn;
        }

        send_packet_cnt++;
        int n = sendto(sock, send_buff, send_len, 0, (struct sockaddr *)&addr, sizeof(addr));
        static char current[100];
        get_now_time(current);
        pthread_mutex_lock(&print_mutex);
        printf("%s %s:%s send_sn: %d send_len: %u total_send: %u\n",
               current, host, port, send_sn, send_len, send_packet_cnt);fflush(stdout);
        pthread_mutex_unlock(&print_mutex);
        if (n < 0)
        {
            perror("sendto");
            close(sock);
            exit(1);
        }
    }
    pthread_exit(NULL);
}

void
print_recv_info(uint32_t recv_sn, uint32_t recv_len, uint64_t recv_time)
{

    static uint32_t recv_time_cnt[MAX_RECV_TIME] = {0};
    static uint64_t all_recv_time = 0;
    static uint64_t avg_recv_time = 0;
    static uint64_t max_recv_time = 0;

    uint64_t mid_recv_time = 0;

    static char current[100];
    get_now_time(current);

    all_recv_time += recv_time;
    avg_recv_time = all_recv_time / recv_packet_cnt;

    uint64_t index = min(recv_time, MAX_RECV_TIME - 1);
    recv_time_cnt[index]++;
    uint32_t i;
    uint32_t cnt = 0;
    for (i = 0; i < MAX_RECV_TIME; i++) {
        cnt += recv_time_cnt[i];
        if (cnt * 2 >= recv_packet_cnt) {
            break;
        }
    }
    mid_recv_time = i;

    max_recv_time = max(max_recv_time, recv_time);

    uint32_t unrecv_cnt = send_packet_cnt - recv_packet_cnt;
    uint32_t unrecv_per = unrecv_cnt * 10000 / send_packet_cnt;
    pthread_mutex_lock(&print_mutex);
    printf("%s %s:%s recv_sn: %d recv_len: %d total_recv: %u\n",
           current, host, port, recv_sn, recv_len, recv_packet_cnt);
    pthread_mutex_unlock(&print_mutex);fflush(stdout);
}

void
check_recv_buff(uint8_t recv_flag[], unsigned char *recv_buff, int n)
{
    if (n < MIN_UDP_LEN || n > MAX_UDP_LEN) {
        printf("wrong_udp_packet_error n = %d max_udp_len range [%u, %u]!!!!\n",
            n, MIN_UDP_LEN, MAX_UDP_LEN);fflush(stdout);
        exit(1);
    }

    uint32_t recv_len = *(uint32_t *) (recv_buff + UDP_LEN_OFFSET);
    uint32_t recv_sn = *(uint32_t *) (recv_buff + UDP_SN_OFFSET);

    if (recv_len != (uint32_t)n) {
        printf("wrong_udp_packet_error n = %d recv_len %u!!!!\n", n, recv_len);fflush(stdout);
        exit(1);
    }

    if (recv_sn >= MAX_UDP_CNT) {
        printf("wrong_udp_packet_error recv_sn %u max_udp_sn %u!!!!\n",
            recv_sn, MAX_UDP_CNT);fflush(stdout);
        exit(1);
    }
    if (recv_flag[recv_sn] > 0) {
        printf("wrong_udp_packet_error duplicate udp packet recv_sn %u!!!!\n", recv_sn);fflush(stdout);
        exit(1);
    }
    uint32_t i;
    int content_error = 0;
    for (i = UDP_DATA_OFFSET; i < recv_len; i += 4) {
        uint32_t content = *(uint32_t *) (recv_buff + i);
        if (content != recv_sn){
            content_error = 1;
            printf("wrong_udp_packet_error len %u recv_sn %u content %u i %u!!!!\n",
                recv_len, recv_sn, content, i);fflush(stdout);
        }
    }
    if (content_error > 0) {
        exit(1);
    }

    recv_flag[recv_sn] = 1;
}


void *
recv_thread()
{
    //printf("start recv\n");
    uint8_t *recv_flag = calloc(udp_cnt, sizeof(uint8_t));
    if (NULL == recv_flag) {
        printf("udp_cnt %u 内存分配失败！！！\n", udp_cnt);fflush(stdout);
        exit(1);
    }
    uint32_t i;
    while(1) {
        int n;
        unsigned char recv_buff[BUF_LEN] = {0};
        unsigned int len = sizeof(addr);
        n = recvfrom(sock, recv_buff, BUF_LEN - 1, 0, (struct sockaddr *)&addr, &len);
        if (n < 0) {
            perror("recv error!!");fflush(stdout);
            exit(1);
        }
        if (0 == n) {
            printf("n == 0!!!!\n");fflush(stdout);
            continue;
        }

        check_recv_buff(recv_flag, recv_buff, n);
        uint32_t recv_sn = *(uint32_t *) (recv_buff + UDP_SN_OFFSET);
        uint32_t recv_len = *(uint32_t *) (recv_buff + UDP_LEN_OFFSET);
        uint64_t start_ts = *(uint64_t *) (recv_buff + UDP_TS_OFFSET);
        uint64_t end_ts = get_current_msec();

        if (end_ts < start_ts) {
            printf("start timestamp error!!!!\n");fflush(stdout);
            exit(1);
        }

        recv_packet_cnt++;

        uint64_t recv_time = end_ts - start_ts;
        print_recv_info(recv_sn, recv_len, recv_time);

        if (recv_packet_cnt == udp_cnt) {
            break;
        }
    }

    pthread_exit(NULL);
}


int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: %s ip port [udp_len] [udp_len_delta] "
                "[udp_cnt]""\n", argv[0]);
        printf("该程序会向目标(ip:port)发送的UDP报文进行探测，探测的参数如下:\n");
        printf("[udp_len]UDP报文字节数, 默认值%u, 范围为[%u, %u]，"
                "且必须是4的倍数\n",
                MIN_UDP_LEN, MIN_UDP_LEN, MAX_UDP_LEN);
        printf("[udp_len_delta] 默认值为0，为0时，发送定长报文，"
                "否则以udp_len为基数，每次发送的UDP报文长度"
                "增加udp_len_delta，udp_len_delta必须为4的倍数\n");
        printf("[udp_cnt] 发送的报文数\n");
        fflush(stdout);
        exit(1);
    }
    printf("This is a UDP client\n");fflush(stdout);

    if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }
    host = argv[1];
    port = argv[2];
    if (argc >= 4) min_udp_len = atoi(argv[3]);
    if (argc >= 5) udp_len_delta = atoi(argv[4]);
    if (argc >= 6) udp_cnt = atoi(argv[5]);

    printf("UDP服务器：%s:%s, 报文大小：[%u字节, udp_len_delta %u,"
            " 报文数：%u\n",
           host, port, min_udp_len, udp_len_delta, udp_cnt);fflush(stdout);

    if (0 == udp_cnt || udp_cnt > MAX_UDP_CNT) {
        printf("发送的报文数%u 必须大于0，小于%u!!\n", udp_cnt, MAX_UDP_CNT);
        fflush(stdout);
        exit(1);
    }
    if (min_udp_len < MIN_UDP_LEN || 0 != min_udp_len % 4) {
        printf("[udp_len] UDP报文字节数范围为[%u, %u]，"
            "且必须是4的倍数\n", MIN_UDP_LEN, MAX_UDP_LEN);fflush(stdout);
        exit(1);
    }
    if (0 != udp_len_delta % 4) {
        printf("[udp_len_delta] %u 必须是4的倍数\n", udp_len_delta);
        fflush(stdout);
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    addr.sin_port = htons(atoi(port));
    if (INADDR_NONE == addr.sin_addr.s_addr)
    {
        printf("非法IP %s!\n", host);fflush(stdout);
        close(sock);
        exit(1);
    }

    pthread_mutex_init(&print_mutex,NULL);
    pthread_t thread[2];
    int ret;
    memset(&thread, 0, sizeof(thread));
    ret = pthread_create(&thread[0], NULL, send_thread, NULL);
    if (ret != 0) {
       printf("线程1创建失败!\n");fflush(stdout);
       exit(1);
    }
    ret = pthread_create(&thread[1], NULL, recv_thread, NULL);
    if (ret != 0) {
       printf("线程2创建失败!\n");fflush(stdout);
       exit(1);
    }
    char c;
    while(c = getchar()) {
        if ('\n' == c) {
            send_flag = 1 - send_flag;
            if (0 == send_flag) {
                printf("暂停发送，按回车键继续...\n");
            }
        }
    }
    if(thread[0] != 0)
    {
        pthread_join(thread[0],NULL);
        printf("发送线程已经结束/n");fflush(stdout);
    }
    if(thread[1] != 0)
    {
       pthread_join(thread[1],NULL);
        printf("接收线程已经结束/n");fflush(stdout);
    }

    return 0;
}
