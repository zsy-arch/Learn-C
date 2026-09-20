# Makefile —— 一个「够用又不复杂」的 C 项目构建脚本
#
#   make          构建 demo
#   make run      构建并运行
#   make asan     用 AddressSanitizer + UBSan 构建并运行
#   make clean    清理
#
# 关键点：
#   1. 用 -MMD -MP 自动生成头文件依赖，改了 .h 也会重新编译
#   2. 所有中间产物放进 build/，不污染源码目录
#   3. 警告即错误（-Werror），在 CI 里挡住低级问题

CC      ?= cc
STD     := -std=c17
WARN    := -Wall -Wextra -Wpedantic -Werror \
           -Wshadow -Wconversion -Wsign-conversion \
           -Wstrict-prototypes -Wmissing-prototypes \
           -Wpointer-arith -Wcast-qual -Wwrite-strings
DEBUG   := -O0 -g
CFLAGS  := $(STD) $(WARN) $(DEBUG) -MMD -MP
LDFLAGS :=

SAN     := -fsanitize=address,undefined -fno-omit-frame-pointer

SRCS    := vec.c slist.c sstr.c main.c
BUILD   := build
OBJS    := $(SRCS:%.c=$(BUILD)/%.o)
DEPS    := $(OBJS:.o=.d)
TARGET  := $(BUILD)/demo
TARGET_ASAN := $(BUILD)/demo_asan

.PHONY: all run asan clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

run: $(TARGET)
	./$(TARGET)

# sanitizer 版本：一步到位，不复用 .o（编译选项不同）
asan: | $(BUILD)
	$(CC) $(STD) $(WARN) $(DEBUG) $(SAN) $(SRCS) -o $(TARGET_ASAN)
	./$(TARGET_ASAN)

clean:
	rm -rf $(BUILD)

-include $(DEPS)
