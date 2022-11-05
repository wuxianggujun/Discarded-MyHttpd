//
// Created by MI on 2022/11/2.
//
#include <cstdio>
#include <iostream>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>

//网络通信需要包含的头文件
#include <winsock2.h>
//带参数的宏
#define PRINTF(str) cout<<"["<<__func__<<" - "<<__LINE__<<"]"<<str<<endl;


//手动加载的库文件
void error_die(const char *string);

#pragma comment(lib, "ws2_32.lib")

using namespace std;

/*
实现网络的初始化
返回值:套接字 socket
参数 port：表示端口,如果*port的值是0.那么就自动分配端口，这样子写是为了向tinyhttpd服务器致敬
 端口在网络开发规范里面是一个无符号的短整型 unsigned short
 */
int startup(unsigned short *port) {
    //网络通信的初始化
    WSADATA data;
    int ret = WSAStartup(MAKEWORD(1, 1), &data);//1.1版本的协议
    if (ret) {//ret != 0
        error_die("WSAStartup");
    }
    //套接字的类型, 数据流,tcp协议
    int server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == SOCKET_ERROR) {
        //打印错误提示，并结束
        error_die("套接字失败了");
    }
    //设置端口复用
    int opt = 1;
    int res = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &opt, sizeof(opt));
    if (res == -1) {
        error_die("setsockopt");

    }

    //配置服务器网络地址
    struct sockaddr_in server_addr{};
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(*port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //绑定套接字
    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        error_die("bind");
    }
    //动态分配端口
    int nameLen = sizeof(server_addr);
    if (*port == 0) {
        if (getsockname(server_socket, (struct sockaddr *) &server_addr, &nameLen) < 0) {
            error_die("getsockname");
        }
        *port = server_addr.sin_port;
    }

    //创建监听队列
    if (listen(server_socket, 5) < 0) {
        error_die("listen");
    }

    return server_socket;

}

void error_die(const char *string) {
    perror(string);
    exit(1);
}

//从指定的客户端套接字读取一行数据，保存到buff中,返回实际读取到的字节数
int get_line(int sock, char *buff, int size) {

    char c = 0;// '\0'
    int i = 0;
    while (i < size - 1 && c != '\n') {
        int n = recv(sock, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = recv(sock, &c, 1, MSG_PEEK);
                if (n > 0 && c == '\n') {
                    recv(sock, &c, 1, 0);
                } else {
                    c = '\n';
                }
            }
            buff[i++] = c;
        } else {
            c = '\n';
        }
    }


    return 0;
}

void unimplement(int client) {
    //向指定的套字节，发送一个提示还没有实现的错误页面


}

void not_found(int client) {
    //网页不存在

}

void headers(int client) {
    //发送响应头信息
    char buff[1024];
    strcpy(buff, "HTTP/1.0 200 OK\r\n");
    send(client, buff, strlen(buff), 0);


    strcpy(buff, "Server: RockHttpd/0.1\r\n");
    send(client, buff, strlen(buff), 0);

    strcpy(buff, "Content-type:text/html\n");
    send(client, buff, strlen(buff), 0);

    strcpy(buff, "\r\n");
    send(client, buff, strlen(buff), 0);
}

void cat(int client, FILE *resource) {
    //一次读一个字节，然后再把字节发个服务器
    char buff[4096];

    int count = 0;

    while (1) {
        int ret = fread(buff, sizeof(char), sizeof(buff), resource);
        if (ret <= 0) {
            break;
        }
        send(client, buff, ret, 0);
        count +=ret;
    }
    cout<<"总共发送 "<<count<<"字节给浏览器"<< endl;

}

void server_file(int client, const char *fileName) {
    char numChars = 1;
    char buff[1024];

    //把请求数据包的剩余数据行读完
    while (numChars > 0 && strcmp(buff, "\n")) {
        numChars = get_line(client, buff, sizeof(buff));
        PRINTF(buff);
    }
    FILE *resource = fopen(fileName, "r");
    if (resource == NULL) {
        not_found(client);
    } else {
        //正式发送资源给服务器
        headers(client);
        //发送请求资源信息
        cat(client, resource);
        cout << "资源发送完毕！" << endl;
    }
    fclose(resource);
}

//处理用户请求的线程函数
DWORD WINAPI accept_request(LPVOID arg) {
    char buff[1024];//1K
    int client = (SOCKET) arg;//客户端套接字

    //读取一行数据
    int numChars = get_line(client, buff, sizeof(buff));

    PRINTF(buff);//

    char method[255];
    int j = 0, i = 0;
    while (!isspace(buff[j]) && i < sizeof(method) - 1) {
        method[i++] = buff[j++];
    }
    method[i] = 0;//'\0'
    PRINTF(method);

    //检查请求方法服务器是否支持
    if (stricmp(method, "GET") && stricmp(method, "POST")) {
        unimplement(client);
        return 0;
    }
    // 解析资源文件路径
    char url[255];//存放请求的资源路径
    i = 0;
    while (isspace(buff[j]) && j < sizeof(buff)) {
        j++;
    }

    while (!isspace(buff[j]) && i < sizeof(url) - 1 && j < sizeof(buff)) {
        url[i++] = buff[j++];
    }
    url[i] = 0;
    PRINTF(url);

    //127.0.0.1
    //url /
    //htdocs/
    char path[512] = "";
    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/') {
        strcat(path, "index.html");
    }
    PRINTF(path);

    //
    struct stat status{};
    if (stat(path, &status) == -1) {
        //请求包的剩余数据读取完毕
        while (numChars > 0 && strcmp(buff, "\n")) {
            numChars = get_line(client, buff, sizeof(buff));
        }
        not_found(client);

    } else {
        if ((status.st_mode & S_IFMT) == S_IFDIR) {
            strcat(path, "/index.html");
        }
        server_file(client, path);
    }

    closesocket(client);

    return 0;
}


int main() {
    unsigned short port = 80;

    int server_sock = startup(&port);

    cout << "Http server start,listener port:" << server_sock << endl;

    //
    struct sockaddr_in client_addr{};

    int client_addr_len = sizeof(client_addr);

    while (true) {
        //阻塞式等待用户通过浏览器发起访问
        int client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_sock == -1) {
            error_die("accept");
        }

        //使用client_sock对用户进行访问
        //创建一个新的线程
        DWORD threadId = 0;
        CreateThread(nullptr, 0, accept_request, (void *) client_sock, 0, &threadId);
    }
    // "/"网站服务器资源目录下的index.html


    closesocket(server_sock);
}

