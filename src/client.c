#include <arpa/inet.h>   // 提供IP地址转换函数
#include <netinet/in.h>  // 提供套接字地址结构定义
#include <stdio.h>       // 标准输入输出
#include <stdlib.h>      // 标准库函数，如exit()
#include <string.h>      // 字符串操作函数
#include <sys/socket.h>  // 套接字相关函数
#include <sys/types.h>   // 数据类型定义
#include <unistd.h>      // POSIX API，如read()和write()

#define PORT 8181              /* 目标服务器的端口号 */
#define IP_ADDRESS "127.0.0.1" /* 目标服务器的IP地址 */
#define BUFSIZE 8196           /* 缓冲区大小 */

char *command = "GET /index.html HTTP/1.0 \r\n\r\n"; /* HTTP GET 请求命令 */

// 错误处理函数，打印错误信息并退出程序
void pexit(char *msg) {
    perror(msg);  // 打印错误信息
    exit(1);      // 退出程序
}

int main() {
    int i, sockfd;                 // sockfd是套接字文件描述符
    char buffer[BUFSIZE];          // 用于存储从服务器接收的数据
    struct sockaddr_in serv_addr;  // 定义服务器地址结构

    // 打印尝试连接服务器的信息
    printf("客户端尝试连接到 %s 和端口 %d\n", IP_ADDRESS, PORT);

    // 创建套接字，使用IPv4和TCP协议
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) pexit("socket() 创建失败");

    // 配置服务器地址
    serv_addr.sin_family = AF_INET;                     // 地址族为IPv4
    serv_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);  // 设置服务器IP地址
    serv_addr.sin_port = htons(PORT);                   // 设置服务器端口号

    // 尝试连接到服务器
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        pexit("connect() 连接失败");

    // 发送HTTP GET请求到服务器
    printf("发送字节数=%ld %s\n", strlen(command), command);
    if (write(sockfd, command, strlen(command)) < 0) pexit("write() 发送请求失败");

    // 循环读取服务器返回的数据，并输出到标准输出
    while ((i = read(sockfd, buffer, BUFSIZE)) > 0) {
        if (write(1, buffer, i) < 0)  // 1表示标准输出
            pexit("write() 输出到标准输出失败");
    }

    // 关闭套接字，释放资源
    close(sockfd);
    return 0;  // 程序正常退出
}
