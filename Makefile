all: clean proj2

.PHONY: clean

proj2:
	gcc -Wall -std=c99 proj2.c -pthread -pedantic -o proj2

clean:
	rm -f proj2

