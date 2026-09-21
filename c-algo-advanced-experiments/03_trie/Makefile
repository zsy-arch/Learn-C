CC = cc
STD = -std=c17
WARN = -Wall -Wextra -Wpedantic -Werror
DBG = -O0 -g
SAN = -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all run test san clean leaks

all: demo tests

demo: demo.c trie.h
	$(CC) $(STD) $(WARN) $(DBG) demo.c -o demo

tests: tests.c trie.h
	$(CC) $(STD) $(WARN) $(DBG) tests.c -o tests

run: demo
	./demo

test: tests
	./tests

demo_san: demo.c trie.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) demo.c -o demo_san

tests_san: tests.c trie.h
	$(CC) $(STD) $(WARN) $(DBG) $(SAN) tests.c -o tests_san

san: demo_san tests_san
	ASAN_OPTIONS=detect_leaks=0 ./demo_san
	./tests_san

leaks: tests
	MallocStackLogging=1 leaks --atExit -- ./tests

clean:
	rm -f demo tests demo_san tests_san *.o
	rm -rf *.dSYM
