CC=gcc
CFLAGS=-g -Wall -W
LDFLAGS=-lcunit -lm
OBJ=$(SRC:.c=.o)

all: $(OBJ) main lib
	$(CC) $(CFLAGS) test1.c -o test1 $(LDFLAGS)
	$(CC) $(CFLAGS) test2.c -o test2 $(LDFLAGS)
	$(CC) $(CFLAGS) test3.c -o test3 $(LDFLAGS)
	$(CC) $(CFLAGS) test4.c -o test4 $(LDFLAGS)
	$(CC) $(CFLAGS) test5.c -o test5 $(LDFLAGS)
	./test1
	./test2
	./test3
	./test4
	./test5

main:
	cd .. && $(MAKE)

lib:
	cd .. && cd libfractal && $(MAKE)

clean:
	rm test1 test2 test3 test4 test5 *.txt *.bmp
