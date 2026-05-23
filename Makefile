CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wpedantic -std=c2x
LDLIBS = -lbsd

SRC_FILES = main.c $(wildcard commands/*.c)

.PHONY: all clean

all: db

db: $(SRC_FILES)
	$(CC) $(CFLAGS) $(SRC_FILES) -o db $(LDLIBS)

clean:
	$(RM) db
