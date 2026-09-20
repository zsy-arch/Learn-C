# Makefile —— algolib 构建脚本
#
#   make          构建 demo
#   make run      构建并运行测试套件
#   make asan     ASan + UBSan 版本
#   make leaks    macOS 泄漏检测
#   make bench    性能对比测试
#   make O2       用 -O2 构建并运行（观察优化对性能测试的影响）
#   make clean    清理

CC      ?= cc
STD     := -std=c17
WARN    := -Wall -Wextra -Wpedantic -Werror \
           -Wshadow -Wconversion -Wsign-conversion \
           -Wstrict-prototypes -Wmissing-prototypes \
           -Wpointer-arith -Wcast-qual -Wwrite-strings
DEBUG   := -O0 -g
CFLAGS  := $(STD) $(WARN) $(DEBUG) -MMD -MP
SAN     := -fsanitize=address,undefined -fno-omit-frame-pointer

SRCS    := sort.c container.c
TESTSRC := test_main.c
BENCHSRC:= bench_main.c

BUILD   := build
OBJS    := $(SRCS:%.c=$(BUILD)/%.o)
TESTOBJ := $(BUILD)/test_main.o
DEPS    := $(OBJS:.o=.d) $(TESTOBJ:.o=.d)

TEST    := $(BUILD)/algotest
TESTASAN:= $(BUILD)/algotest_asan
BENCH   := $(BUILD)/algobench

.PHONY: all run asan leaks bench O2 clean

all: $(TEST)

$(TEST): $(OBJS) $(TESTOBJ)
	$(CC) $^ $(LDFLAGS) -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

run: $(TEST)
	./$(TEST)

# sanitizer 版本：不能复用普通 .o（编译选项不同）
asan: | $(BUILD)
	$(CC) $(STD) $(WARN) $(DEBUG) $(SAN) $(SRCS) $(TESTSRC) -o $(TESTASAN)
	./$(TESTASAN)

# 泄漏检测：leaks 不能和 ASan 同用，所以用普通二进制
leaks: $(TEST)
	@MallocStackLogging=1 leaks --atExit -- ./$(TEST) 2>&1 \
		| grep -E 'leaks for|nodes malloced' || true

bench: | $(BUILD)
	$(CC) $(STD) $(WARN) $(DEBUG) $(SRCS) $(BENCHSRC) -o $(BENCH)
	./$(BENCH)

O2: | $(BUILD)
	$(CC) $(STD) $(WARN) -O2 -g $(SRCS) $(TESTSRC) -o $(BUILD)/algotest_O2
	./$(BUILD)/algotest_O2

clean:
	rm -rf $(BUILD)

-include $(DEPS)
