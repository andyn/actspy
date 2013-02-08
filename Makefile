CC = gcc
CFLAGS = -std=c99 -pedantic -Wall -Wextra -g

all: actspy

ttyspy: actspy.c
	$(CC) $(CFLAGS) -o actspy actspy.c
	
clean:
	-rm actspy
	
.PHONY: all clean
