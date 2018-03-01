#include<stdio.h>
#include<string.h>    //strlen
#include<sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <stdlib.h>
 
int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("usage: %s bind_ip bind_port\n", argv[0]);
    }
    char *bind_ip = argv[1];
    int bind_port;
    if ((bind_port = atoi(argv[2])) <= 0) {
        printf("usage: %s bind_ip bind_port\n", argv[0]);
    }

    int listen_fd, client_sock, c, read_size;
    struct sockaddr_in server, client;
    char client_message[10 * 1024 * 1024 + 10];
    //Create socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(bind_ip);
    server.sin_port = htons(bind_port);
    //Bind
    if( bind(listen_fd,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        exit(1);
    }
    //Listen
    listen(listen_fd, 30);
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    while(1) {
        //accept connection from an incoming client
        client_sock = accept(listen_fd, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0)
        {
            perror("accept failed");
            return 1;
        }
        puts("Connection accepted");
        //Receive a message from client
        //int recv_cnt = 0;
        while((read_size = recv(client_sock, client_message, sizeof(10 * 1024 * 1024), 0)) > 0) {
             //printf("recv cnt %d recv size %d\n", recv_cnt++, read_size);
             continue;
        }

        if (0 == read_size) {
            printf("Client disconnected\n");
        } else {
            perror("recv failed");
        }
    }
    return 0;
}
