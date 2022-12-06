CC = gcc
CFLAGS = -g -std=gnu11 -Werror -Wall -Wextra -Wpedantic -Wmissing-declarations -Wmissing-prototypes -Wold-style-definition
DEPS = parser.h
OBJ = mmake.o parser.o

%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

mmake: $(OBJ)
		$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean
clean:
		-rm *.o mmake
