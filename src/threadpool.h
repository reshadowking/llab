#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"  // 包含配置头文件

// 使用config.h中的定义，不再重复定义
// #define MAX_THREADS 16  // 移动到config.h
// #define MAX_QUEUE 256   // 移动到config.h

// 任务结构
typedef struct task {
    void (*function)(void *arg);
    void *arg;
    struct task *next;
} task_t;

// 线程池结构
typedef struct threadpool {
    pthread_t *threads;        // 线程数组
    task_t *queue_head;        // 任务队列头
    task_t *queue_tail;        // 任务队列尾
    pthread_mutex_t lock;       // 互斥锁
    pthread_cond_t cond;        // 条件变量
    int shutdown;               // 关闭标志
    int thread_count;           // 线程数量
    int queue_size;             // 队列大小
} threadpool_t;

// 函数声明
threadpool_t *threadpool_create(int thread_count);
int threadpool_add_task(threadpool_t *pool, void (*function)(void *), void *arg);
void threadpool_destroy(threadpool_t *pool);

#endif