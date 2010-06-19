CFLAGS= -Wall

all:
	gcc $(CFLAGS) magicfifo.c -o magicfifo
	
clean:
	rm -f magicfifo
