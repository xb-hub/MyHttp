#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

void accept_request(void *arg);
void not_found(int client);
void unimplemented(int client);
void bad_request(int client);
void cat(int client, FILE *resource);
void headers(int client);
void server_file(int client, const char *path);
void execute_cgi(int client, const char *path, char *method, char *query_string);
int my_getline(int sock, char *buf, int size);
int startup(u_short port);

// 处理http请求线程
void accept_request(void *arg)
{
    int client = (intptr_t)arg;
    int cgi = 0;

    char method[1024];
    char url[1024];
    char buf[1024];
    char path[1024];
    char *query_string = NULL;

    size_t len;
    size_t i, j;

    struct stat st;

    len = my_getline(client, buf, sizeof(buf));
    printf("%s", buf);
    i = 0;
    while (i < len && !isspace(buf[i])) // 提取method
    {
        method[i] = buf[i];
        i++;
    }
    j = i;
    method[i] = '\0';

    if(strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        return;
    }
    i = 0;
    while (isspace(buf[j]) && j < len)
    {
        j++;
    }
    while (!isspace(buf[j]) && j < sizeof(url)) // 提取url
    {
        url[i++] = buf[j++];
    }
    url[i] = '\0';
    
    if(strcasecmp(method, "GET") == 0)  // 判断是否需要动态解析
    {
        query_string = url;
        while (*query_string != '?' && *query_string != '\0')
        {
            query_string++;
        }
        if(*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "doc%s", url);
    if(path[strlen(path) - 1] == '/')   // 如果没有具体路径显示主页
    {
        strcat(path, "index.html");
    }
    printf("%s\n", path);
    if(stat(path, &st) == -1)   // 找不到资源文件
    {
        while (len > 0 && strcmp(buf, "\n"))    // 从套接字中读取剩下的http报文，并丢弃
        {
            len = my_getline(client, buf, sizeof(buf));
        }
        not_found(client);  // 返回404
    }
    else
    {
        if(S_ISDIR(st.st_mode)) strcat(path, "/index.html");    // 如果路径所指是目录文件
        if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))  cgi = 1;    // 如果文件具有可执行权限，进行动态解析。
        if(!cgi)    server_file(client, path);
        else    execute_cgi(client, path, method, query_string);
    }
    close(client);
}

void not_found(int client)
{
    printf("not found%d", client);
}

void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);  // send():发送数据的字节数。
}

void bad_request(int client)
{
    printf("bad request%d", client);
}

// 复制文件内容并send
void cat(int client, FILE *resource)
{
    char buf[1024];

    while (fgets(buf, sizeof(buf), resource) != NULL)
    {
        printf("%s", buf);
        send(client, buf, strlen(buf), 0);
    }
}

// 响应报文头
void headers(int client)
{
    char buf[1024];

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

// 通过socket传输文件
void server_file(int client,const char *path)
{
    FILE *resource;
    size_t len = 1;
    char buf[1024];

    while (len > 0 && strcmp(buf, "\n"))    // 从套接字中读取剩下的http报文，并丢弃
    {
        len = my_getline(client, buf, sizeof(buf));
        printf("%s", buf);
    }

    if((resource = fopen(path, "r")) == NULL)
    {
        not_found(client);
    }
    else
    {
        headers(client);    // 添加报文头
        cat(client, resource);  // 传输文件内容
    }
    fclose(resource);
}

void execute_cgi(int client, const char *path, char *method, char *query_string)
{
    printf("%d %s %s %s", client, path, method, query_string);
}

void error_die(const char *str)
{
    perror(str);
    exit(1);
}

int my_getline(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while (i < size - 1 && c != '\n')
    {
        n = recv(sock, &c, 1, 0);
        if(n > 0)
        {
            if(c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                if(n > 0 && c == '\n')  // windows中换行符为\r\n
                {
                    n = recv(sock, &c, 1, 0);
                }
                else
                {
                    c = '\n';
                }
            }
            buf[i++] = c;
        }
        else
        {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

// 创建套接字
int startup(u_short port)
{
    int socketfd = 0;
    int on = 1;
    struct sockaddr_in addr;

    if((socketfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        error_die("socket");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        error_die("setsockopt failed");
    }
    if(bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        error_die("bind");
    }
    if(listen(socketfd, 5) < 0)
    {
        error_die("listen");
    }
    return socketfd;
}

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Usage: parameter port\n");
        return -1;
    }
    int server_sock;
    int client_sock;
    u_short port = (u_short)atoi(argv[1]);
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);
    pthread_t new_thread;

    server_sock = startup(port);
    printf("my http running on port %d\n", port);

    while (1)
    {
        if((client_sock = accept(server_sock, (struct sockaddr *)(&client_name), &client_name_len)) == -1)
        {
            error_die("accept");
        }
        if(pthread_create(&new_thread, NULL, (void *)accept_request, (void *)(intptr_t)client_sock) != 0)
        {
            perror("thread");
        }
    }
    close(server_sock);
    return 0;
}