# You can use clang if you prefer
CC = gcc-11

# Feel free to add other C flags
CFLAGS += -c -std=gnu99 -Wall -Werror -Wextra -O2 -lz
# By default, we colorize the output, but this might be ugly in log files, so feel free to remove the following line.
CFLAGS += -D_COLOR

# For CUnit (on MacOs)
CFLAGS += -lcunit -I/usr/local/include -L/usr/local/CUnit/lib -c

# Adapt these as you want to fit with your project
SOURCES = $(wildcard ../packet_implem.c linkedList.c test_linkedList.c)

OBJECTS = $(SOURCES:.c=.o)

TEST = test

all: $(TEST)

$(TEST): $(OBJECTS)
	$(CC) $(OBJECTS) -o test $(CFLAGS)
	./test

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

.PHONY: clean 

clean:
	rm -f $(OBJECTS)



# It is likely that you will need to update this
tests: all
	make
	./test

# By default, logs are disabled. But you can enable them with the debug target.
debug: CFLAGS += -D_DEBUG
debug: clean all
