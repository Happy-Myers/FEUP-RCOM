CC = gcc
CFLAGS = -Wall

SRC = src/
INCLUDE = include/
BIN = bin/

.PHONY: app
app: $(SRC)/download_app.c
	$(CC) $(CFLAGS) -o download_app $^

.PHONY: clean
clean:
	rm -rf download_app