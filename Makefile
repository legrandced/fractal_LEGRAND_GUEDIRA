CC=gcc
CFLAGS=-g -Wall -W -I/usr/include/SDL
LDFLAGS=-lpthread -lSDL
SRC=main.c
SRC2=libfractal/*.c
OBJ=$(SRC:.c=.o)
LIBFILE=-Ilibfractal

all: $(OBJ)
	$(CC) $(CFLAGS) $(SRC) $(SRC2) $(LIBFILE) -o main $(LDFLAGS)

lib:
	cd libfractal && $(MAKE)

tests:
	make -s -C ./tests

clean:
	rm main *.o **/*.o **/*.a
	cd tests && $(MAKE) clean

.PHONY: tests all run
