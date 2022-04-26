# You can use clang if you prefer
CC = gcc

# Feel free to add other C flags
CFLAGS += -c -std=gnu99 -Wall -Werror -Wextra -O2 
# By default, we colorize the output, but this might be ugly in log files, so feel free to remove the following line.
# CFLAGS += -D_COLOR -g

# You may want to add something here
LDFLAGS += -lz 

# Adapt these as you want to fit with your project
SENDER_SOURCES = $(wildcard src/sender.c src/log.c src/packet_implem.c src/fec.c src/read-write/*.c src/linkedList/linkedList.c)
RECEIVER_SOURCES = $(wildcard src/receiver.c src/log.c src/packet_implem.c src/fec.c src/read-write/*.c src/linkedList/linkedList.c)

SENDER_OBJECTS = $(SENDER_SOURCES:.c=.o)
RECEIVER_OBJECTS = $(RECEIVER_SOURCES:.c=.o)

SENDER = sender
RECEIVER = receiver

all: $(SENDER) $(RECEIVER)

$(SENDER): $(SENDER_OBJECTS)
	$(CC) $(SENDER_OBJECTS) -o $@ $(LDFLAGS)

$(RECEIVER): $(RECEIVER_OBJECTS)
	$(CC) $(RECEIVER_OBJECTS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

.PHONY: clean mrproper

clean:
	rm -f $(SENDER_OBJECTS) $(RECEIVER_OBJECTS)


mrproper:
	rm -f $(SENDER) $(RECEIVER)

# Run all tests without printing log
tests-nolog: all
	make nolog
	./tests/run_tests.sh -nolog

tests: all
	make clean
	make
	./tests/run_tests.sh

valgrind: all
	./tests/valgrind.sh

# By default, debug are disabled. But you can enable them with the debug target.
debug: CFLAGS += -D_DEBUG
debug: clean all

# Make nolog desactivate logs for the sender and receiver
nolog: CFLAGS += -D_LOG_DISABLE
nolog: clean all

# Place the zip in the parent repository of the project
ZIP_NAME="../projet1_Hardy_Paquet.zip"

# A zip target, to help you have a proper zip file. You probably need to adapt this code.
zip:
	# Generate the log file stat now. Try to keep the repository clean.
	git log --stat > gitlog.stat
	zip -r $(ZIP_NAME) Makefile README.md src tests rapport.pdf gitlog.stat
	# We remove it now, but you can leave it if you want.
	rm gitlog.stat


cleanTests:
	rm -f tests/basic/*
	rm -f tests/image/*
	rm -f tests/imageSimlink/*
	rm -f tests/pdf/*
	rm -f tests/pdfSimlink/*
	rm -f tests/random/*
	rm -f tests/smallImage/*
	rm -f tests/smallSimlink/*
	rm -f tests/valgrind/*