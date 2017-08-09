CC = gcc
CFLAGS = -Wall -g -O2
MODFLAGS = -shared -fPIC


all: bloom.so

bloom.so:	bloom.c
	$(CC) $(CFLAGS) $(MODFLAGS) -o bloom.so bloom.c
