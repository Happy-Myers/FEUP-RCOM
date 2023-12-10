COMPILER_TYPE = gnu
CC = gcc

PROG = download
SRC = src/download_app.c

CFLAGS= -Wall -g

$(PROG): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(PROG)

clean:
	rm -rf download