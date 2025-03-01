# Croitoru Constantin-Bogdan 334CA
CC = g++
CCFLAGS = -lpthread -Werror -Wall

.PHONY: build clean

build: tema1

tema1: main.cpp
	$(CC) -o $@ $^ $(CCFLAGS)

clean:
	rm -f tema1
