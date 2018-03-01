#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>
#include <netdb.h>
#include<stdio.h> //printf
#include<string.h>    //strlen
#include<sys/socket.h>    //socket
#include<arpa/inet.h> //inet_addr

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

int main(int argc, char *argv[])
{
    if (argc != 4) {
        printf("usage: %s server_ip server_port send_rate\n", argv[0]);
        exit(1);
    }
    char *server_ip = argv[1];
    int server_port, send_rate;
    if ((server_port = atoi(argv[2])) <= 0) {
        printf("usage: %s server_ip server_port send_rate\n", argv[0]);
        exit(1);
    }
    if ((send_rate = atoi(argv[3])) <= 0) {
        printf("usage: %s server_ip server_port send_rate\n", argv[0]);
        exit(1);
    }
    if (send_rate > 10 * 1024 * 1024) {
        send_rate = 10 * 1024 * 1024;
        printf("send_rate reset to 10MB!!!!!\n");
    }
    printf("server %s:%d send_rate %d\n", server_ip, server_port, send_rate);
    int sock;
    struct sockaddr_in server_sockaddr;

    //create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sock) {
        printf("could not create socket\n");
        exit(1);
    }

    server_sockaddr.sin_addr.s_addr = inet_addr(server_ip);
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(server_port);

    //connect to remote server
    if (connect(sock, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr)) < 0)
    {
        perror("connect failed. error");
        exit(1);
    }

    char send_buf[10 * 1024 * 1024 + 10];
    for (int i = 0; i < 10 * 1024 * 1024; i++) {
        send_buf[i] = 'a';
    }

    int send_size;
    while(1) {
        uint64_t start_ts = get_current_msec();
        //send some data
        if((send_size = send(sock, send_buf, send_rate, 0)) < 0)
        {
            printf("send failed\n");
            exit(0);
        }
        printf("start ts %lu send size %d\n", start_ts, send_size);
        uint64_t end_ts = get_current_msec();
        uint64_t delta  = end_ts - start_ts;
        if (delta > 1000) {
            printf("delta %lu\n", delta);
            continue;
        }
        usleep((1000 - delta) * 1000);

    }

    close(sock);
    return 0;
}
