#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>      // 添加这个头文件
#include <sys/socket.h>      // 添加这个头文件
#include <netinet/in.h>      // 添加这个头文件

#include <unistd.h>
#include "epoll_handler.h"
#include "webserver.h"

epoll_handler_t *epoll_handler_create(int server_fd, cache_t *cache, 
                                     const char *document_root, threadpool_t *pool) {
    epoll_handler_t *handler = malloc(sizeof(epoll_handler_t));
    if (!handler) return NULL;
    
    handler->epoll_fd = epoll_create1(0);
    if (handler->epoll_fd == -1) {
        perror("epoll_create1");
        free(handler);
        return NULL;
    }
    
    handler->server_fd = server_fd;
    handler->cache = cache;
    handler->document_root = strdup(document_root);
    handler->thread_pool = pool;
    handler->events = malloc(sizeof(struct epoll_event) * MAX_EVENTS);
    
    if (!handler->events || !handler->document_root) {
        free(handler->events);
        free(handler->document_root);
        close(handler->epoll_fd);
        free(handler);
        return NULL;
    }
    
    // 添加服务器socket到epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(handler->epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl: server_fd");
        free(handler->events);
        free(handler->document_root);
        close(handler->epoll_fd);
        free(handler);
        return NULL;
    }
    
    return handler;
}

void epoll_handler_destroy(epoll_handler_t *handler) {
    if (!handler) return;
    
    close(handler->epoll_fd);
    free(handler->events);
    free(handler->document_root);
    free(handler);
}

int epoll_handler_add_client(epoll_handler_t *handler, int client_fd) {
    // 设置非阻塞模式
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // 边缘触发模式
    ev.data.fd = client_fd;
    
    if (epoll_ctl(handler->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
        perror("epoll_ctl: client_fd");
        return -1;
    }
    
    return 0;
}

void epoll_handler_loop(epoll_handler_t *handler) {
    printf("Epoll event loop started...\n");
    
    while (1) {
        int nfds = epoll_wait(handler->epoll_fd, handler->events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            if (handler->events[i].data.fd == handler->server_fd) {
                // 新的客户端连接
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int client_fd = accept(handler->server_fd, 
                                     (struct sockaddr*)&client_addr, &addr_len);
                
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }
                
                printf("New connection from %s:%d\n", 
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                
                if (epoll_handler_add_client(handler, client_fd) == -1) {
                    close(client_fd);
                }
            } else {
                // 客户端数据可读
                int client_fd = handler->events[i].data.fd;
                
                // 创建客户端上下文
                client_context_t *ctx = malloc(sizeof(client_context_t));
                if (!ctx) {
                    close(client_fd);
                    continue;
                }
                
                ctx->client_fd = client_fd;
                ctx->document_root = handler->document_root;
                ctx->cache = handler->cache;
                
                // 添加到线程池处理
                if (threadpool_add_task(handler->thread_pool, handle_client_request, ctx) != 0) {
                    fprintf(stderr, "Failed to add task to thread pool\n");
                    close(client_fd);
                    free(ctx);
                }
            }
        }
    }
}