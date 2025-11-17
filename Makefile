CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE -g -pthread -I. -I./src
LDFLAGS = -lpthread

SRCDIR = src
OBJDIR = obj
BINDIR = bin
WWWDIR = www

# 获取所有源文件
SOURCES = $(wildcard $(SRCDIR)/*.c)
# 服务器需要的源文件（排除client.c）
SERVER_SOURCES = $(filter-out $(SRCDIR)/client.c, $(SOURCES))
# 客户端只需要client.c
CLIENT_SOURCES = $(SRCDIR)/client.c

# 对象文件
SERVER_OBJECTS = $(SERVER_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
CLIENT_OBJECTS = $(CLIENT_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

SERVER_TARGET = $(BINDIR)/webserver
CLIENT_TARGET = $(BINDIR)/client

.PHONY: all clean install test run debug

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_OBJECTS) | $(BINDIR)
	$(CC) $(SERVER_OBJECTS) -o $@ $(LDFLAGS)

$(CLIENT_TARGET): $(CLIENT_OBJECTS) | $(BINDIR)
	$(CC) $(CLIENT_OBJECTS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

install: $(SERVER_TARGET)
	cp $(SERVER_TARGET) /usr/local/bin/

test: $(CLIENT_TARGET)
	@echo "Testing server connection..."
	./$(CLIENT_TARGET)

# 设置测试环境
setup:
	mkdir -p $(WWWDIR)
	@echo "<html><body><h1>Welcome to Web Server</h1><p>Test page</p></body></html>" > $(WWWDIR)/index.html
	@echo "Test favicon" > $(WWWDIR)/favicon.ico
	@echo "Test image" > $(WWWDIR)/example.png
	@echo "Test files created in $(WWWDIR)/"

# 运行服务器
run: $(SERVER_TARGET)
	./$(SERVER_TARGET) -p 8080 -d ./www -a lru

# 调试目标：显示变量值
debug:
	@echo "SERVER_SOURCES: $(SERVER_SOURCES)"
	@echo "SERVER_OBJECTS: $(SERVER_OBJECTS)"
	@echo "CLIENT_SOURCES: $(CLIENT_SOURCES)"
	@echo "CLIENT_OBJECTS: $(CLIENT_OBJECTS)"
	@echo "Files in src/:"
	@ls -la $(SRCDIR)/

# 检查头文件依赖
check-headers:
	@echo "Checking header dependencies..."
	@for header in cache.h threadpool.h webserver.h epoll_handler.h config.h; do \
		if [ -f "$(SRCDIR)/$$header" ]; then \
			echo "✅ $(SRCDIR)/$$header exists"; \
		else \
			echo "❌ $(SRCDIR)/$$header missing"; \
		fi \
	done

# 性能测试
benchmark: $(SERVER_TARGET)
	@echo "Starting performance benchmark..."
	@./$(SERVER_TARGET) -p 8081 -d ./www -a lru &
	@sleep 2
	@echo "Running benchmark with ab..."
	@ab -n 1000 -c 100 http://localhost:8081/ || true
	@pkill webserver

# 内存检查
memcheck: $(SERVER_TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(SERVER_TARGET) -p 8082 -d ./www -a lru