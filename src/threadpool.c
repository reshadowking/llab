#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"
#include "config.h"

static void *worker_thread(void *arg) {
    threadpool_t *pool = (threadpool_t *)arg;
    
    while (1) {
        pthread_mutex_lock(&pool->lock);
        
        // 等待任务或关闭信号
        while (pool->queue_size == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->cond, &pool->lock);
        }
        
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }
        
        // 获取任务
        task_t *task = pool->queue_head;
        if (task) {
            pool->queue_head = task->next;
            if (!pool->queue_head) {
                pool->queue_tail = NULL;
            }
            pool->queue_size--;
        }
        
        pthread_mutex_unlock(&pool->lock);
        
        if (task) {
            // 执行任务
            task->function(task->arg);
            free(task);
        }
    }
    
    return NULL;
}

threadpool_t *threadpool_create(int thread_count) {
    if (thread_count <= 0 || thread_count > MAX_THREADS) {
        thread_count = 4;
    }
    
    threadpool_t *pool = malloc(sizeof(threadpool_t));
    if (!pool) return NULL;
    
    pool->threads = malloc(sizeof(pthread_t) * thread_count);
    if (!pool->threads) {
        free(pool);
        return NULL;
    }
    
    pool->queue_head = pool->queue_tail = NULL;
    pool->queue_size = 0;
    pool->shutdown = 0;
    pool->thread_count = thread_count;
    
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->cond, NULL);
    
    // 创建工作线程
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            // 创建失败，销毁线程池
            threadpool_destroy(pool);
            return NULL;
        }
    }
    
    printf("Thread pool created with %d threads\n", thread_count);
    return pool;
}

int threadpool_add_task(threadpool_t *pool, void (*function)(void *), void *arg) {
    if (!pool || !function) return -1;
    
    task_t *task = malloc(sizeof(task_t));
    if (!task) return -1;
    
    task->function = function;
    task->arg = arg;
    task->next = NULL;
    
    pthread_mutex_lock(&pool->lock);
    
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        free(task);
        return -1;
    }
    
    // 添加到队列尾部
    if (pool->queue_tail) {
        pool->queue_tail->next = task;
    } else {
        pool->queue_head = task;
    }
    pool->queue_tail = task;
    pool->queue_size++;
    
    // 通知工作线程
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->lock);
    
    return 0;
}

void threadpool_destroy(threadpool_t *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->lock);
    
    // 等待所有线程退出
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // 清理任务队列
    task_t *task = pool->queue_head;
    while (task) {
        task_t *next = task->next;
        free(task);
        task = next;
    }
    
    free(pool->threads);
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond);
    free(pool);
    
    printf("Thread pool destroyed\n");
}