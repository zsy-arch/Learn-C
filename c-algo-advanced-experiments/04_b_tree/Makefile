CC      = cc
STD     = -std=c17
WARN    = -Wall -Wextra -Wpedantic -Werror
DBG     = -O0 -g
SAN     = -fsanitize=address,undefined -fno-omit-frame-pointer
SRC     = btree.c

.PHONY: all run test san clean

all: demo tests

demo: $(SRC) demo.c btree.h
	$(CC) $(STD) $(WARN) $(DBG) $(SRC) demo.c -o demo

tests: $(SRC) tests.c btree.h
	$(CC) $(STD) $(WARN) $(DBG) $(SRC) tests.c -o tests

demo_san: $(SRC) demo.c btree.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) $(SRC) demo.c -o demo_san

tests_san: $(SRC) tests.c btree.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) $(SRC) tests.c -o tests_san

perf: $(SRC) perf.c btree.h
	$(CC) $(STD) $(WARN) -O2 -g $(SRC) perf.c -o perf

run: demo
	./demo

test: tests
	./tests

san: demo_san tests_san
	./demo_san
	./tests_san

bench: perf
	./perf

clean:
	rm -f demo tests demo_san tests_san perf perf_san
	rm -rf *.dSYM
