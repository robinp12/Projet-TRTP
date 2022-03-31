# You can use clang if you prefer
CC = gcc-11

# Feel free to add other C flags
CFLAGS += -std=gnu99 -Wall -Werror -Wextra -O2 -lz
# By default, we colorize the output, but this might be ugly in log files, so feel free to remove the following line.
CFLAGS += -D_COLOR

# For CUnit (on MacOs)
CFLAGS += -I/usr/local/include -L/usr/local/lib -lcunit -g

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@ $(LDFLAGS)

test: ../packet_implem.o linkedList.o
	$(CC) test_linkedList.c -o test $(CFLAGS) ../packet_implem.o linkedList.o

testValgrind:
	make test
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all ./test

.PHONY: test

clean:
	rm -f test ../packet_implem.o linkedList.o test_linkedList.o



# It is likely that you will need to update this
tests: all
	make
	./test

# By default, logs are disabled. But you can enable them with the debug target.
debug: CFLAGS += -D_DEBUG
debug: clean all
