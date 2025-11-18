#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "epoll_handler.h"
#include "webserver.h"
#include "threadpool.h"
#include "cache.h"
#include "logging.h"

static void handle_new_connection(epoll_handler_t *handler);
static void handle_client_data(epoll_handler_t *handler, int client_fd);

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
    
    log_message(LOG_INFO, "Epoll处理器创建成功，最大事件数: %d", MAX_EVENTS);
    return handler;
}

void epoll_handler_loop(epoll_handler_t *handler) {
    log_message(LOG_INFO, "Epoll事件循环开始");
    
    while (1) {
        int nfds = epoll_wait(handler->epoll_fd, handler->events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                log_message(LOG_DEBUG, "epoll_wait被信号中断，继续循环");
                continue;
            }
            perror("epoll_wait");
            log_message(LOG_ERROR, "epoll_wait错误: %s", strerror(errno));
            break;
        }
        
        log_message(LOG_DEBUG, "epoll_wait返回 %d 个就绪事件", nfds);
        
        for (int i = 0; i < nfds; i++) {
            if (handler->events[i].data.fd == handler->server_fd) {
                // 处理新连接
                handle_new_connection(handler);
            } else {
                // 处理客户端数据
                handle_client_data(handler, handler->events[i].data.fd);
            }
        }
    }
    
    log_message(LOG_INFO, "Epoll事件循环结束");
}

static void handle_new_connection(epoll_handler_t *handler) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_fd = accept(handler->server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd == -1) {
        perror("accept");
        log_message(LOG_ERROR, "接受新连接失败");
        return;
    }
    
    // 设置非阻塞模式
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    // 添加客户端到epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; // 边缘触发模式
    ev.data.fd = client_fd;
    if (epoll_ctl(handler->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
        perror("epoll_ctl: client_fd");
        close(client_fd);
        log_message(LOG_ERROR, "添加客户端到epoll失败");
        return;
    }
    
    log_message(LOG_INFO, "新连接: %s:%d", 
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
}

static void handle_client_data(epoll_handler_t *handler, int client_fd) {

    // 创建客户端上下文
    client_context_t *ctx = malloc(sizeof(client_context_t));
    if (!ctx) {
        log_message(LOG_ERROR, "分配客户端上下文内存失败");
        close(client_fd);
        return;
    }
    
    ctx->client_fd = client_fd;
    ctx->document_root = handler->document_root;
    ctx->cache = handler->cache;
    
    // 从epoll中移除，交给线程池处理
    epoll_ctl(handler->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    
    // 添加到线程池
    if (threadpool_add_task(handler->thread_pool, handle_client_request, ctx) != 0) {
        log_message(LOG_ERROR, "添加任务到线程池失败");
        close(client_fd);
        free(ctx);
    } else {
        log_message(LOG_DEBUG, "客户端任务已添加到线程池");
    }
}
void epoll_handler_destroy(epoll_handler_t *handler) {
    if (!handler) return;
    
    if (handler->events) {
        free(handler->events);
        handler->events = NULL;
    }
    
    if (handler->document_root) {
        free(handler->document_root);
        handler->document_root = NULL;
    }
    
    if (handler->epoll_fd >= 0) {
        close(handler->epoll_fd);
        handler->epoll_fd = -1;
    }
    
    free(handler);
    
    log_message(LOG_INFO, "Epoll处理器已销毁");
}
