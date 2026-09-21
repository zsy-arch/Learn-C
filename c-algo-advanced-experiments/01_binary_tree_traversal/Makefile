CC = cc
STD = -std=c17
WARN = -Wall -Wextra -Wpedantic -Werror
DBG = -O0 -g
SAN = -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all run test san clean

all: demo tests

demo: demo.c
	$(CC) $(STD) $(WARN) $(DBG) demo.c -o demo

tests: tests.c
	$(CC) $(STD) $(WARN) $(DBG) tests.c -o tests

run: demo
	./demo

test: tests
	./tests

demo_san: demo.c
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) demo.c -o demo_san

tests_san: tests.c
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) tests.c -o tests_san

san: demo_san tests_san
	./demo_san
	./tests_san

clean:
	rm -f demo tests demo_san tests_san *.o *.txt
