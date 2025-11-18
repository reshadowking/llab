#ifndef EPOLL_HANDLER_H
#define EPOLL_HANDLER_H

#include <sys/epoll.h>
#include "cache.h"
#include "threadpool.h"
#include "config.h"

#define MAX_EVENTS 1024

typedef struct {
    int epoll_fd;
    int server_fd;
    cache_t *cache;
    char *document_root;
    threadpool_t *thread_pool;
    struct epoll_event *events;
} epoll_handler_t;

// 添加这些函数声明
epoll_handler_t *epoll_handler_create(int server_fd, cache_t *cache, 
                                     const char *document_root, threadpool_t *pool);
void epoll_handler_destroy(epoll_handler_t *handler);
void epoll_handler_loop(epoll_handler_t *handler);

#endif