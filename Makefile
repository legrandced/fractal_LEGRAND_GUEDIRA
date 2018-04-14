CC=gcc
CFLAGS=-g -Wall -W -I/usr/include/SDL
LDFLAGS=-lpthread -lSDL
SRC=main.c
SRC2=libfractal/*.c
OBJ=$(SRC:.c=.o)
LIBFILE=-Ilibfractal

all: $(OBJ)
	$(CC) $(CFLAGS) $(SRC) $(SRC2) $(LIBFILE) -o main $(LDFLAGS)
