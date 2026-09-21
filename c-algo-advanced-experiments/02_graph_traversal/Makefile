CC      = cc
STD     = -std=c17
WARN    = -Wall -Wextra -Wpedantic -Werror
DBG     = -O0 -g
SAN     = -fsanitize=address,undefined -fno-omit-frame-pointer
SRC     = graph.c algos.c

.PHONY: all run test san clean

all: demo tests

demo: $(SRC) demo.c graph.h algos.h
	$(CC) $(STD) $(WARN) $(DBG) $(SRC) demo.c -o demo

tests: $(SRC) tests.c graph.h algos.h
	$(CC) $(STD) $(WARN) $(DBG) $(SRC) tests.c -o tests

demo_san: $(SRC) demo.c graph.h algos.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) $(SRC) demo.c -o demo_san

tests_san: $(SRC) tests.c graph.h algos.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) $(SRC) tests.c -o tests_san

run: demo
	./demo

test: tests
	./tests

san: demo_san tests_san
	./demo_san
	./tests_san

clean:
	rm -f demo tests demo_san tests_san
