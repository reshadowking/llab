#ifndef CONFIG_H
#define CONFIG_H

// 缓存配置
#define MAX_CACHE_SIZE (100 * 1024 * 1024)  // 100MB最大缓存大小
#define HASH_TABLE_SIZE 1024                 // 哈希表大小
#define MAX_CACHE_ITEM_SIZE (10 * 1024 * 1024) // 单个缓存项最大大小(10MB)

// 网络配置
#define MAX_EVENTS 1024                      // epoll最大事件数
#define BUFFER_SIZE 8196                     // 缓冲区大小
#define MAX_CONNECTIONS 1024                  // 最大连接数
#define BACKLOG_SIZE 128                     // 监听队列大小

// 线程池配置
#define MAX_THREADS 16                       // 最大线程数
#define MAX_QUEUE 256                        // 任务队列最大长度

// 服务器配置
#define DEFAULT_PORT 8080                    // 默认端口
#define DEFAULT_DOCUMENT_ROOT "./www"        // 默认文档根目录
#define DEFAULT_CACHE_ALGORITHM LRU         // 默认缓存算法

// 缓存算法枚举
typedef enum {
    LRU,
    LFU
} cache_algorithm_t;

// 性能监控配置
#define STATS_UPDATE_INTERVAL 5              // 统计信息更新间隔(秒)

#endif