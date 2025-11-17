#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

#include "webserver.h"
#include "cache.h"
#include "epoll_handler.h"
#include "threadpool.h"

// 全局统计变量
static unsigned long cache_hits = 0;
static unsigned long total_requests = 0;

// 修复：添加缺失的函数声明
int create_server_socket(int port);

void send_error_response(int client_fd, int code, const char *message) {
    char response[1024];
    int len = snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><body><h1>%d %s</h1></body></html>",
        code, message, code, message);
    write(client_fd, response, len);
}

void send_file_response(int client_fd, const char *filename, void *data, size_t size) {
    char header[1024];
    const char *content_type = "text/plain";
    
    // 根据文件扩展名设置Content-Type
    if (strstr(filename, ".html")) content_type = "text/html";
    else if (strstr(filename, ".css")) content_type = "text/css";
    else if (strstr(filename, ".js")) content_type = "application/javascript";
    else if (strstr(filename, ".png")) content_type = "image/png";
    else if (strstr(filename, ".jpg") || strstr(filename, ".jpeg")) content_type = "image/jpeg";
    else if (strstr(filename, ".gif")) content_type = "image/gif";
    else if (strstr(filename, ".ico")) content_type = "image/x-icon";
    
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    char date[64];
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", tm);
    
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: keep-alive\r\n"
        "Date: %s\r\n"
        "Server: MyWebServer/1.0\r\n"
        "\r\n",
        content_type, size, date);
    
    write(client_fd, header, header_len);
    write(client_fd, data, size);
}

void handle_client_request(void *arg) {
    client_context_t *ctx = (client_context_t *)arg;
    char buffer[BUFFER_SIZE];
    
    // 读取HTTP请求
    ssize_t bytes_read = read(ctx->client_fd, buffer, sizeof(buffer)-1);
    if (bytes_read <= 0) {
        close(ctx->client_fd);
        free(ctx);
        return;
    }
    buffer[bytes_read] = '\0';
    
    total_requests++;
    
    // 解析请求行
    char method[16], path[256], protocol[16];
    if (sscanf(buffer, "%15s %255s %15s", method, path, protocol) != 3) {
        send_error_response(ctx->client_fd, 400, "Bad Request");
        close(ctx->client_fd);
        free(ctx);
        return;
    }
    
    // 只处理GET请求
    if (strcasecmp(method, "GET") != 0) {
        send_error_response(ctx->client_fd, 501, "Not Implemented");
        close(ctx->client_fd);
        free(ctx);
        return;
    }
    
    // 安全检查路径
    if (strstr(path, "..") != NULL) {
        send_error_response(ctx->client_fd, 403, "Forbidden");
        close(ctx->client_fd);
        free(ctx);
        return;
    }
    
    // 构建文件路径
    char filepath[512];
    if (strcmp(path, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", ctx->document_root);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", ctx->document_root, path);
    }
    
    // 检查缓存
    cache_item_t *cached = cache_get(ctx->cache, filepath);
    if (cached) {
        cache_hits++;
        printf("Cache HIT: %s (Hit rate: %.2f%%)\n", filepath, 
               (float)cache_hits / total_requests * 100);
        send_file_response(ctx->client_fd, filepath, cached->data, cached->size);
    } else {
        printf("Cache MISS: %s\n", filepath);
        
        // 缓存未命中，读取文件
        int file_fd = open(filepath, O_RDONLY);
        if (file_fd < 0) {
            send_error_response(ctx->client_fd, 404, "Not Found");
        } else {
            struct stat file_stat;
            if (fstat(file_fd, &file_stat) < 0) {
                send_error_response(ctx->client_fd, 500, "Internal Server Error");
                close(file_fd);
                close(ctx->client_fd);
                free(ctx);
                return;
            }
            
            // 只缓存小文件（小于10MB）
            if (file_stat.st_size < 10 * 1024 * 1024) {
                // 读取文件到内存并缓存
                void *file_data = malloc(file_stat.st_size);
                if (file_data) {
                    if (read(file_fd, file_data, file_stat.st_size) == file_stat.st_size) {
                        cache_put(ctx->cache, filepath, file_data, file_stat.st_size);
                    }
                    send_file_response(ctx->client_fd, filepath, file_data, file_stat.st_size);
                    free(file_data);
                } else {
                    // 内存分配失败，回退到普通发送
                    send_error_response(ctx->client_fd, 500, "Internal Server Error");
                }
            } else {
                // 大文件直接发送
                send_error_response(ctx->client_fd, 413, "File Too Large");
            }
            close(file_fd);
        }
    }
    
    close(ctx->client_fd);
    free(ctx);
}

int create_server_socket(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    // 创建socket文件描述符
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // 设置socket选项 - 使用条件编译处理SO_REUSEPORT
    #ifdef SO_REUSEPORT
        // 如果系统支持SO_REUSEPORT，同时设置SO_REUSEADDR和SO_REUSEPORT
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    #else
        // 如果不支持，只设置SO_REUSEADDR
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    #endif
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // 绑定socket到端口
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // 开始监听
    if (listen(server_fd, 128) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Server socket created on port %d\n", port);
    return server_fd;
}

void start_server(int port, const char *document_root, cache_algorithm_t algorithm) {
    int server_fd = create_server_socket(port);
    
    // 创建缓存
    cache_t *cache = cache_create(MAX_CACHE_SIZE, algorithm);
    if (!cache) {
        fprintf(stderr, "Failed to create cache\n");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // 创建线程池
    threadpool_t *pool = threadpool_create(8);
    if (!pool) {
        fprintf(stderr, "Failed to create thread pool\n");
        cache_destroy(cache);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // 创建epoll处理器
    epoll_handler_t *epoll_handler = epoll_handler_create(server_fd, cache, document_root, pool);
    if (!epoll_handler) {
        fprintf(stderr, "Failed to create epoll handler\n");
        threadpool_destroy(pool);
        cache_destroy(cache);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Web server started successfully!\n");
    printf("Port: %d\n", port);
    printf("Document root: %s\n", document_root);
    printf("Cache algorithm: %s\n", algorithm == LRU ? "LRU" : "LFU");
    printf("Cache size: %d MB\n", MAX_CACHE_SIZE / (1024 * 1024));
    
    // 进入事件循环
    epoll_handler_loop(epoll_handler);
    
    // 清理资源（通常不会执行到这里）
    epoll_handler_destroy(epoll_handler);
    threadpool_destroy(pool);
    cache_destroy(cache);
    close(server_fd);
}