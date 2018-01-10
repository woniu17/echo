#include <sys/types.h>
#include <sys/socket.h>
#include<pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: %s ip port\n", argv[0]);
        exit(1);
    }
    char *host = argv[1];
    char *port = argv[2];
    printf("Welcome! This is a UDP server, I can only received message from client and reply with same message\n");

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    //addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(port));

    int sock;
    if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }
    const int BUF_LEN = 2048;
    char buff[BUF_LEN];
    struct sockaddr_in clientAddr;
    int n;
    unsigned int len = sizeof(clientAddr);
    int count = 0;
    while (1)
    {
        n = recvfrom(sock, buff, BUF_LEN-1, 0, (struct sockaddr*)&clientAddr, &len);
        if (n>0)
        {
            buff[n] = 0;
            printf("累计收包数：%4d， 数据包大小：%4d 字节\n", ++count, n);
            //printf("buff %s\n", buff);
            n = sendto(sock, buff, n, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
            if (n < 0)
            {
                perror("sendto");
                break;
            }
        }
        else
        {
            perror("recv");
            break;
        }
    }
    return 0;
}
