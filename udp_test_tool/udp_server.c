#include <sys/types.h>
#include <sys/socket.h>
#include<pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#define MAX_UDP_CNT (100)
#define RECV_INFO_SIZE (1000)

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

typedef struct recv_info_s {
    struct sockaddr_in client_addr;
    uint8_t *flag;
    uint32_t recv_cnt;
} recv_info_t;

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

recv_info_t *
get_recv_info(struct sockaddr_in client_addr)
{
    static recv_info_t recv_info_array[RECV_INFO_SIZE];
    static int is_init = 0;
    if (!is_init) {
        memset(recv_info_array, RECV_INFO_SIZE, sizeof(recv_info_t));
        is_init = 1;
    }
    int i;
    for (i = 0; i < RECV_INFO_SIZE; i++) {
        recv_info_t *recv_info = &recv_info_array[i];
        if (NULL == recv_info->flag) {
            recv_info->flag = calloc(MAX_UDP_CNT, sizeof(uint8_t));
            recv_info->client_addr = client_addr;
            return recv_info;
        }
        if (
            client_addr.sin_addr.s_addr == recv_info->client_addr.sin_addr.s_addr
            &&
            client_addr.sin_port == recv_info->client_addr.sin_port
           ) {
            return recv_info;
        }
    }
    printf("recv_info_array is full!!!!!!!!!\n");fflush(stdout);
    return NULL;
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
        printf("wrong_udp_packet_error n = %d max_udp_len %u!!!!\n", n, recv_len);fflush(stdout);
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

void
check_recv_buff_for_client(struct sockaddr_in client_addr, unsigned char recv_buff[], int n)
{
    recv_info_t *recv_info = get_recv_info(client_addr);
    if (NULL == recv_info) {
        return;
    }
    check_recv_buff(recv_info->flag, recv_buff, n);
    recv_info->recv_cnt++;
    uint32_t recv_sn = *(uint32_t *) (recv_buff + UDP_SN_OFFSET);

    char *ip = inet_ntoa(client_addr.sin_addr);
    static char current[100];
    get_now_time(current);
    printf("%s %s:%u recv_sn: %u recv_len: %d total_recv: %u\n",
        current,
        ip,
        client_addr.sin_port,
        recv_sn,
        n,
        recv_info->recv_cnt
        );fflush(stdout);
}

int
main(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: %s ip port\n", argv[0]);fflush(stdout);
        exit(1);
    }
    char *host = argv[1];
    char *port = argv[2];
    int check = 0;
    if (4 == argc) {
        if (0 == strcmp(argv[3], "check")) {
            check = 1;
        }
    }
    check = 1;
    printf("Welcome! This is a UDP server, I can only received message"
           " from client and reply with same message\n");fflush(stdout);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    //addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(port));

    int sock;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    const int BUF_LEN = 10240;
    unsigned char buff[BUF_LEN];
    struct sockaddr_in client_addr;
    int n;
    unsigned int len = sizeof(client_addr);
    uint32_t count = 0;
    while (1) {
        n = recvfrom(sock, buff, BUF_LEN - 1, 0, (struct sockaddr*)&client_addr, &len);
        count++;
        if (n <= 0) {
            perror("recv");
            break;
        }
        buff[n] = 0;
        if (check) {
            check_recv_buff_for_client(client_addr, buff, n);
        } else {
            char *ip = inet_ntoa(client_addr.sin_addr);
            printf("从客户端 %s:%5u 接收报文 长度 %4d 累计接收总数 %u\n",
                ip,
                client_addr.sin_port,
                n,
                count
                );fflush(stdout);
        }

        n = sendto(sock, buff, n, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
        if (n < 0) {
            perror("sendto");
            break;
        }
        recv_info_t *recv_info = get_recv_info(client_addr);
        if (NULL == recv_info) {
            continue;
        }

        static char current[100];
        get_now_time(current);
        uint32_t recv_sn = *(uint32_t *) (buff + UDP_SN_OFFSET);

        char *ip = inet_ntoa(client_addr.sin_addr);
        printf("%s %s:%u send_sn: %u send_len: %d total_send: %u\n",
            current,
            ip,
            client_addr.sin_port,
            recv_sn,
            n,
            recv_info->recv_cnt
            );fflush(stdout);
        }
    return 0;
}
