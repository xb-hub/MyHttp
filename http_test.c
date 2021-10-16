#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv)
{
    if(argc < 2)
    {
        printf("Usage: parameter port\n");
        return -1;
    }
    int clientfd;
    struct sockaddr_in addr;
    char buf[1024];
    char response[1024];
    u_short port = (u_short)atoi(argv[1]);
    if((clientfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("socket creat fail!");
        return -1;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(connect(clientfd, (struct scokaddr *)&addr, sizeof(addr)) == -1)
    {
        printf("connect fail!");
        return -1;
    }
    sprintf(buf, "GET / HTTP/1.1\r\n");
    send(clientfd, buf, strlen(buf), 0);
    sprintf(buf, "Host: 127.0.0.1:4000");
    send(clientfd, buf, strlen(buf), 0);
    recv(clientfd, response, 1024, 0);
    printf("%s\n", response);
}