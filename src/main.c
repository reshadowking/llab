#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include "webserver.h"
#include "config.h"

void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -p, --port PORT      Server port (default: %d)\n", DEFAULT_PORT);
    printf("  -d, --dir DIR        Document root directory (default: %s)\n", DEFAULT_DOCUMENT_ROOT);
    printf("  -a, --algorithm ALG  Cache algorithm: lru or lfu (default: %s)\n", 
           DEFAULT_CACHE_ALGORITHM == LRU ? "lru" : "lfu");
    printf("  -h, --help           Show this help message\n");
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    char *document_root = DEFAULT_DOCUMENT_ROOT;
    cache_algorithm_t algorithm = DEFAULT_CACHE_ALGORITHM;
    
    // 解析命令行参数
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"dir", required_argument, 0, 'd'},
        {"algorithm", required_argument, 0, 'a'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "p:d:a:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    return 1;
                }
                break;
            case 'd':
                document_root = optarg;
                break;
            case 'a':
                if (strcasecmp(optarg, "lru") == 0) {
                    algorithm = LRU;
                } else if (strcasecmp(optarg, "lfu") == 0) {
                    algorithm = LFU;
                } else {
                    fprintf(stderr, "Invalid algorithm: %s (use lru or lfu)\n", optarg);
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // 检查文档根目录是否存在
    if (access(document_root, F_OK) != 0) {
        fprintf(stderr, "Document root directory does not exist: %s\n", document_root);
        return 1;
    }
    
    printf("Starting web server...\n");
    printf("Port: %d\n", port);
    printf("Document root: %s\n", document_root);
    printf("Cache algorithm: %s\n", algorithm == LRU ? "LRU" : "LFU");
    printf("Cache size: %d MB\n", MAX_CACHE_SIZE / (1024 * 1024));
    
    start_server(port, document_root, algorithm);
    
    return 0;
}