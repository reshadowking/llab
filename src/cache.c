#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "cache.h"

// 哈希函数
static unsigned int hash(const char *key) {
    unsigned int hash = 0;
    while (*key) {
        hash = (hash << 5) + hash + *key++;
    }
    return hash % HASH_TABLE_SIZE;
}

// 创建新缓存项
static cache_item_t *create_item(const char *key, void *data, size_t size) {
    cache_item_t *item = malloc(sizeof(cache_item_t));
    if (!item) return NULL;
    
    item->key = strdup(key);
    item->data = malloc(size);
    if (!item->data || !item->key) {
        free(item->key);
        free(item->data);
        free(item);
        return NULL;
    }
    
    memcpy(item->data, data, size);
    item->size = size;
    item->timestamp = time(NULL);
    item->frequency = 1;
    item->prev = item->next = item->h_next = NULL;
    
    return item;
}

// 释放缓存项
static void free_item(cache_item_t *item) {
    if (!item) return;
    free(item->key);
    free(item->data);
    free(item);
}

// 从链表中移除项
static void remove_from_list(cache_t *cache, cache_item_t *item) {
    if (item->prev) item->prev->next = item->next;
    if (item->next) item->next->prev = item->prev;
    if (cache->head == item) cache->head = item->next;
    if (cache->tail == item) cache->tail = item->prev;
}

// 添加到链表头部(LRU)或根据频率排序(LFU)
static void add_to_list(cache_t *cache, cache_item_t *item) {
    if (cache->algorithm == LRU) {
        // LRU: 新项添加到头部
        item->next = cache->head;
        item->prev = NULL;
        if (cache->head) cache->head->prev = item;
        cache->head = item;
        if (!cache->tail) cache->tail = item;
    } else {
        // LFU: 按频率排序插入
        cache_item_t *curr = cache->head;
        cache_item_t *prev = NULL;
        
        while (curr && curr->frequency >= item->frequency) {
            prev = curr;
            curr = curr->next;
        }
        
        if (prev) {
            item->next = prev->next;
            item->prev = prev;
            prev->next = item;
            if (item->next) item->next->prev = item;
        } else {
            item->next = cache->head;
            item->prev = NULL;
            if (cache->head) cache->head->prev = item;
            cache->head = item;
        }
        
        if (!cache->tail || cache->tail == prev) cache->tail = item;
    }
}

// 淘汰缓存项
static void evict_item(cache_t *cache) {
    if (!cache->tail) return;
    
    cache_item_t *victim = cache->tail;
    
    // 从哈希表移除
    unsigned int idx = hash(victim->key);
    cache_item_t *curr = cache->table[idx];
    cache_item_t *prev = NULL;
    
    while (curr) {
        if (curr == victim) {
            if (prev) prev->h_next = curr->h_next;
            else cache->table[idx] = curr->h_next;
            break;
        }
        prev = curr;
        curr = curr->h_next;
    }
    
    // 从链表移除
    remove_from_list(cache, victim);
    
    // 更新统计
    cache->total_size -= victim->size;
    cache->count--;
    
    free_item(victim);
}

cache_t *cache_create(size_t max_size, cache_algorithm_t algorithm) {
    cache_t *cache = malloc(sizeof(cache_t));
    if (!cache) return NULL;
    
    memset(cache->table, 0, sizeof(cache->table));
    cache->head = cache->tail = NULL;
    cache->total_size = 0;
    cache->max_size = max_size;
    cache->count = 0;
    cache->algorithm = algorithm;
    pthread_mutex_init(&cache->lock, NULL);
    
    return cache;
}

void cache_destroy(cache_t *cache) {
    if (!cache) return;
    
    pthread_mutex_lock(&cache->lock);
    
    // 清理所有缓存项
    cache_item_t *item = cache->head;
    while (item) {
        cache_item_t *next = item->next;
        free_item(item);
        item = next;
    }
    
    pthread_mutex_unlock(&cache->lock);
    pthread_mutex_destroy(&cache->lock);
    free(cache);
}

int cache_put(cache_t *cache, const char *key, void *data, size_t size) {
    if (!cache || !key || !data || size == 0) return -1;
    
    pthread_mutex_lock(&cache->lock);
    
    // 检查是否已存在
    unsigned int idx = hash(key);
    cache_item_t *curr = cache->table[idx];
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            // 更新现有项
            free(curr->data);
            curr->data = malloc(size);
            if (!curr->data) {
                pthread_mutex_unlock(&cache->lock);
                return -1;
            }
            memcpy(curr->data, data, size);
            curr->size = size;
            curr->timestamp = time(NULL);
            curr->frequency++;
            
            // 更新链表位置
            remove_from_list(cache, curr);
            add_to_list(cache, curr);
            
            pthread_mutex_unlock(&cache->lock);
            return 0;
        }
        curr = curr->h_next;
    }
    
    // 创建新项
    cache_item_t *item = create_item(key, data, size);
    if (!item) {
        pthread_mutex_unlock(&cache->lock);
        return -1;
    }
    
    // 检查空间并淘汰
    while (cache->total_size + size > cache->max_size && cache->count > 0) {
        evict_item(cache);
    }
    
    // 添加到哈希表
    item->h_next = cache->table[idx];
    cache->table[idx] = item;
    
    // 添加到链表
    add_to_list(cache, item);
    
    cache->total_size += size;
    cache->count++;
    
    pthread_mutex_unlock(&cache->lock);
    return 0;
}

cache_item_t *cache_get(cache_t *cache, const char *key) {
    if (!cache || !key) return NULL;
    
    pthread_mutex_lock(&cache->lock);
    
    unsigned int idx = hash(key);
    cache_item_t *item = cache->table[idx];
    
    while (item) {
        if (strcmp(item->key, key) == 0) {
            // 更新访问信息
            item->timestamp = time(NULL);
            item->frequency++;
            
            // 更新链表位置
            remove_from_list(cache, item);
            add_to_list(cache, item);
            
            pthread_mutex_unlock(&cache->lock);
            return item;
        }
        item = item->h_next;
    }
    
    pthread_mutex_unlock(&cache->lock);
    return NULL;
}

void cache_remove(cache_t *cache, const char *key) {
    if (!cache || !key) return;
    
    pthread_mutex_lock(&cache->lock);
    
    unsigned int idx = hash(key);
    cache_item_t *curr = cache->table[idx];
    cache_item_t *prev = NULL;
    
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            // 从哈希表移除
            if (prev) prev->h_next = curr->h_next;
            else cache->table[idx] = curr->h_next;
            
            // 从链表移除
            remove_from_list(cache, curr);
            
            // 更新统计
            cache->total_size -= curr->size;
            cache->count--;
            
            free_item(curr);
            break;
        }
        prev = curr;
        curr = curr->h_next;
    }
    
    pthread_mutex_unlock(&cache->lock);
}

void cache_clear(cache_t *cache) {
    if (!cache) return;
    
    pthread_mutex_lock(&cache->lock);
    
    // 清理所有缓存项
    cache_item_t *item = cache->head;
    while (item) {
        cache_item_t *next = item->next;
        free_item(item);
        item = next;
    }
    
    // 重置缓存状态
    memset(cache->table, 0, sizeof(cache->table));
    cache->head = cache->tail = NULL;
    cache->total_size = 0;
    cache->count = 0;
    
    pthread_mutex_unlock(&cache->lock);
}

size_t cache_get_size(cache_t *cache) {
    if (!cache) return 0;
    return cache->total_size;
}

unsigned int cache_get_count(cache_t *cache) {
    if (!cache) return 0;
    return cache->count;
}

void cache_set_algorithm(cache_t *cache, cache_algorithm_t algorithm) {
    if (!cache) return;
    
    pthread_mutex_lock(&cache->lock);
    
    if (cache->algorithm != algorithm) {
        cache->algorithm = algorithm;
        
        // 重新排序所有项
        if (cache->head) {
            cache_item_t *curr = cache->head;
            cache->head = cache->tail = NULL;
            
            while (curr) {
                cache_item_t *next = curr->next;
                curr->prev = curr->next = NULL;
                add_to_list(cache, curr);
                curr = next;
            }
        }
    }
    
    pthread_mutex_unlock(&cache->lock);
}