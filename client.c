#include <stdio.h>
#include <ev.h>
#include <netinet/in.h> // for sockaddr_in
#include <fcntl.h> // for fcntl
#include <string.h> // for bzero

static void
setnonblock(int fd){
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

static void
setaddress(const char* ip, int port, struct sockaddr_in* addr) {
    bzero(addr, sizeof(*addr));
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

static void do_connected(struct ev_loop* reactor, ev_io* w, int events){
    char str[] = "hello, world!";
    send(w->fd, str, sizeof(str), 0);
    ev_break(reactor,EVBREAK_ALL);
}

int main(){
    struct ev_loop* reactor=ev_loop_new(EVFLAG_AUTO);
    int fd = new_tcp_client("127.0.0.1", 8888);
    ev_io io;
    ev_io_init(&io, &do_connected, fd, EV_WRITE);
    ev_io_start(reactor, &io);
    ev_run(reactor, 0);
    close(fd);
    ev_loop_destroy(reactor);
    return 0;
}
