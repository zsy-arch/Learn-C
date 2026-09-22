CC      = cc
STD     = -std=c17
WARN    = -Wall -Wextra -Wpedantic -Werror
DBG     = -O0 -g
SAN     = -fsanitize=address,undefined -fno-omit-frame-pointer
SRC     = btree.c

.PHONY: all run test guard san bench clean

all: demo tests guard_check

demo: $(SRC) demo.c btree.h
	$(CC) $(STD) $(WARN) $(DBG) $(SRC) demo.c -o demo

tests: $(SRC) tests.c btree.h
	$(CC) $(STD) $(WARN) $(DBG) $(SRC) tests.c -o tests

demo_san: $(SRC) demo.c btree.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) $(SRC) demo.c -o demo_san

tests_san: $(SRC) tests.c btree.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) $(SRC) tests.c -o tests_san

guard_check: $(SRC) guard_check.c btree.h
	$(CC) $(STD) $(WARN) $(DBG) $(SRC) guard_check.c -o guard_check

perf: $(SRC) perf.c btree.h
	$(CC) $(STD) $(WARN) -O2 -g $(SRC) perf.c -o perf

run: demo
	./demo

test: tests guard
	./tests

# btree_create 的 t < 2 守卫：断言刻意反着写——guard_check 必须**失败**。
# 只认 exit(1) + 那行特定诊断：node_create 里 malloc 失败也是 exit(1)，
# 光看退出码的话，守卫被删掉后 t<=0 引发的巨量 malloc 失败会冒充成守卫生效。
guard: guard_check
	@for t in 1 0 -1; do \
		out=`./guard_check $$t 2>&1`; rc=$$?; \
		case "$$rc:$$out" in \
		1:*"t must be >= 2"*) \
			echo "  guard: btree_create($$t) 被正确拒绝" ;; \
		*) \
			echo "  guard: FAIL —— btree_create($$t) 期望 exit(1) 且打印 't must be >= 2'，实际 exit($$rc): $$out"; \
			exit 1 ;; \
		esac; \
	done

san: demo_san tests_san
	./demo_san
	./tests_san

bench: perf
	./perf

clean:
	rm -f demo tests demo_san tests_san guard_check perf perf_san
	rm -rf *.dSYM
