#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<netdb.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<errno.h>

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


int main(int argc,char *argv[])
{
    int sockfd;
    const int BUF_LEN = 2048;
    const int DATA_LEN = 2048;
    char sendbuffer[BUF_LEN];
    char recvbuffer[BUF_LEN];
    struct sockaddr_in server_addr;
    struct hostent *host;
    int portnumber,nbytes;
    if (argc!=3)
    {
        fprintf(stderr,"Usage :%s hostname portnumber\a\n",argv[0]);
        exit(1);
    }
    if ((host=gethostbyname(argv[1]))==NULL)
    {
        herror("Get host name error\n");
        exit(1);
    }
    if ((portnumber=atoi(argv[2]))<0)
    {
        fprintf(stderr,"Usage:%s hostname portnumber\a\n",argv[0]);
        exit(1);
    }
    if ((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1)
    {
        fprintf(stderr,"Socket Error:%s\a\n",strerror(errno));
        exit(1);
    }
    bzero(&server_addr,sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(portnumber);
    server_addr.sin_addr=*((struct in_addr *)host->h_addr);
    if (connect(sockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr))==-1)
    {
       fprintf(stderr,"Connect error:%s\n",strerror(errno));
       exit(1);
    }
    struct timeval timeout={10, 0};
    int ret = setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
    ret = setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));
    struct timeval start, end;
    int packet_cnt = 0;
    int loss_cnt = 0;
    int long_time_cnt = 0;
    while (1)
    {
        int i;
        for (i = 1; i < DATA_LEN; i++) { sendbuffer[i] = 'a'; }
        gettimeofday(&start, 0);

        int send_cnt = send(sockfd, sendbuffer, DATA_LEN, 0);
        int recv_cnt = recv(sockfd, recvbuffer, DATA_LEN, 0);

        gettimeofday(&end, 0);
        int ms = time_subtract(&start, &end);
        packet_cnt++;
        if (send_cnt < DATA_LEN || recv_cnt < DATA_LEN) {
            loss_cnt++;
            ms = -1;
        } else {
            if (ms >= 200) {
                long_time_cnt++;
            }
        }
        printf("%4d ms, %6d total packets, %6d loss packets, %6d >200ms packets\n", ms, packet_cnt, loss_cnt, long_time_cnt);
    }
    close(sockfd);
    exit(0);
}
