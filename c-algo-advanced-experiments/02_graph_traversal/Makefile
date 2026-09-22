CC      = cc
STD     = -std=c17
WARN    = -Wall -Wextra -Wpedantic -Werror
DBG     = -O0 -g
SAN     = -fsanitize=address,undefined -fno-omit-frame-pointer
SRC     = graph.c algos.c

.PHONY: all run test san clean run-buggy

# buggy_cycle_demo 是"错误示例"程序，只依赖 graph.c（它自带一份错误的环检测实现）
all: demo tests buggy_cycle_demo

demo: $(SRC) demo.c graph.h algos.h
	$(CC) $(STD) $(WARN) $(DBG) $(SRC) demo.c -o demo

tests: $(SRC) tests.c graph.h algos.h
	$(CC) $(STD) $(WARN) $(DBG) $(SRC) tests.c -o tests

demo_san: $(SRC) demo.c graph.h algos.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) $(SRC) demo.c -o demo_san

tests_san: $(SRC) tests.c graph.h algos.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) $(SRC) tests.c -o tests_san

buggy_cycle_demo: graph.c buggy_cycle_demo.c graph.h
	$(CC) $(STD) $(WARN) $(DBG) graph.c buggy_cycle_demo.c -o buggy_cycle_demo

buggy_cycle_demo_san: graph.c buggy_cycle_demo.c graph.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) graph.c buggy_cycle_demo.c -o buggy_cycle_demo_san

run: demo
	./demo

test: tests
	./tests

run-buggy: buggy_cycle_demo
	./buggy_cycle_demo

san: demo_san tests_san buggy_cycle_demo_san
	./demo_san
	./tests_san
	./buggy_cycle_demo_san

clean:
	rm -f demo tests demo_san tests_san buggy_cycle_demo buggy_cycle_demo_san
	rm -rf *.dSYM
