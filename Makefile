CC = clang++
CFLAGS = -std=c++11 -O2 -Wall

all: main

main: main.cpp Parsers.hpp
	$(CC) $(CFLAGS) -o main main.cpp

run:
	./main

test:
	./main < example.input

clean:
	rm -f main *.o
