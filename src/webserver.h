#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "cache.h"
#include "threadpool.h"
#include "config.h"  // 包含配置头文件

// 使用config.h中的定义，不再重复定义
// #define BUFFER_SIZE 8196  // 移动到config.h

typedef struct {
    int client_fd;
    char *document_root;
    cache_t *cache;
} client_context_t;

void send_error_response(int client_fd, int code, const char *message);
void send_file_response(int client_fd, const char *filename, void *data, size_t size);
void handle_client_request(void *arg);
int create_server_socket(int port);
void start_server(int port, const char *document_root, cache_algorithm_t algorithm);

#endif