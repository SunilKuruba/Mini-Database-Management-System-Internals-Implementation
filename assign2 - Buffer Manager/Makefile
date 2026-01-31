CC      := gcc
CFLAGS  := -g -Wall
LDFLAGS :=
LDLIBS  := -lm

OBJS = storage_mgr.o buffer_mgr.o buffer_mgr_stat.o dberror.o

all: test_assign2_1 

# Build test_assign2_1 (main test suite - FIFO, LRU)
test_assign2_1: test_assign2_1.o $(OBJS)
	$(CC) $(CFLAGS) -o test_assign2_1 test_assign2_1.o $(OBJS)

test_assign2_1.o: test_assign2_1.c buffer_mgr.h storage_mgr.h test_helper.h
	$(CC) $(CFLAGS) -c test_assign2_1.c

# Run tests
test1: test_assign2_1
	./test_assign2_1

valgrind_test: test_assign2_1
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test_assign2_1

# Clean build artifacts
clean:
	rm -f *.o test_assign2_1

.PHONY: all clean test1 run_test1 valgrind_test
