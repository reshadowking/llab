#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "config.h"  // 包含配置头文件

// 使用config.h中的定义，不再重复定义
// #define MAX_CACHE_SIZE (100 * 1024 * 1024)  // 移动到config.h
// #define HASH_TABLE_SIZE 1024               // 移动到config.h

// 缓存项结构
typedef struct cache_item {
    char *key;                  // 资源路径
    void *data;                // 资源数据
    size_t size;               // 资源大小
    time_t timestamp;          // 最后访问时间
    unsigned int frequency;    // 访问频率(LFU使用)
    struct cache_item *prev;
    struct cache_item *next;
    struct cache_item *h_next; // 哈希表链表指针
} cache_item_t;

// 缓存结构
typedef struct cache {
    cache_item_t *table[HASH_TABLE_SIZE]; // 哈希表
    cache_item_t *head;        // 链表头(LRU/LFU顺序)
    cache_item_t *tail;        // 链表尾
    size_t total_size;         // 当前缓存总大小
    size_t max_size;           // 最大缓存大小
    unsigned int count;        // 缓存项数量
    cache_algorithm_t algorithm; // 缓存算法
    pthread_mutex_t lock;      // 线程安全锁
} cache_t;

// 函数声明
cache_t *cache_create(size_t max_size, cache_algorithm_t algorithm);
void cache_destroy(cache_t *cache);
int cache_put(cache_t *cache, const char *key, void *data, size_t size);
cache_item_t *cache_get(cache_t *cache, const char *key);
void cache_remove(cache_t *cache, const char *key);
void cache_clear(cache_t *cache);
size_t cache_get_size(cache_t *cache);
unsigned int cache_get_count(cache_t *cache);
void cache_set_algorithm(cache_t *cache, cache_algorithm_t algorithm);

#endif