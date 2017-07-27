#include <stdio.h>
#include <unistd.h>
#include <ev.h>
#include<arpa/inet.h> // for sockaddr_in
#include <fcntl.h> // for fcntl
#include <string.h> // for bzero
#include <errno.h>
#include<pthread.h>
#define unused(x) if (x){}

static void
setnonblock(int fd){
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

static void
setaddress(const char* ip, int port, struct sockaddr_in* addr) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(addr->sin_addr));
    addr->sin_port = htons(port);
}

static int
new_tcp_client(const char* ip, int port){
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setnonblock(fd);
    struct sockaddr_in addr;
    setaddress(ip, port, &addr);
    connect(fd, (struct sockaddr*)(&addr), sizeof(addr));
    return fd;
}

static void
write_data(struct ev_loop* reactor, ev_io* w, int events){
    unused(events);
    char str[] = "hello, world!";
    send(w->fd, str, sizeof(str), 0);
    printf("send %s size %zu\n", str, sizeof(str));
    ev_io_stop(reactor, w);
}

static void
read_data(struct ev_loop* reactor, ev_io* io_r, int events){
    unused(events);
    ssize_t rcv_len = 0;
    char buf[1024];
    rcv_len = recv(io_r->fd, buf, sizeof(buf) - 1, 0);
    if (0 == rcv_len) {
        printf("fd %d server close!!\n", io_r->fd);
    } else if (rcv_len < 0) {
        printf("fd %d read error %s!!!\n", io_r->fd, strerror(errno));
    } else {
        buf[rcv_len] = '\0';
        printf("fd %d read %s\n", io_r->fd, buf);
    }
    ev_break(reactor, EVBREAK_ALL);
}

void *
echo(void *arg)
{
    unused(arg);
    struct ev_loop* reactor=ev_loop_new(EVFLAG_AUTO);
    int fd = new_tcp_client("127.0.0.1", 22222);
    ev_io io_w;
    ev_io_init(&io_w, &write_data, fd, EV_WRITE);
    ev_io_start(reactor, &io_w);

    ev_io io_r;
    ev_io_init(&io_r, &read_data, fd, EV_READ);
    ev_io_start(reactor, &io_r);

    ev_run(reactor, 0);
    close(fd);
    ev_loop_destroy(reactor);
    return NULL;
}

int
main(){
    pthread_t threads[100];
    for (int i = 0; i < 100; i++) {
        pthread_create(&threads[i], NULL, echo, NULL);
        if (0 == (i+1) % 10) {
            sleep(3);
        }
    }
    sleep(10);
    return 0;
}
