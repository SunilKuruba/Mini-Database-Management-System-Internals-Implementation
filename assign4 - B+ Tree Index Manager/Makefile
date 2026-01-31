CC      := gcc
CFLAGS  := -g -Wall -std=c99
LDFLAGS :=
LDLIBS  := -lm

SRCS = storage_mgr.c buffer_mgr.c buffer_mgr_stat.c dberror.c record_mgr.c rm_serializer.c expr.c btree_mgr.c
OBJS = $(SRCS:.c=.o)

all: test_assign4_1 test_expr test_assign4_2

# Build test_assign4_2 (new test suite)
test_assign4_2: test_assign4_2.o $(OBJS)
	$(CC) $(CFLAGS) -o test_assign4_2 test_assign4_2.o $(OBJS) $(LDLIBS)
test_assign4_2.o: test_assign4_2.c dberror.h btree_mgr.h tables.h test_helper.h
	$(CC) $(CFLAGS) -c test_assign4_2.c

# Build test_assign4_1 (main test suite)
test_assign4_1: test_assign4_1.o $(OBJS)
	$(CC) $(CFLAGS) -o test_assign4_1 test_assign4_1.o $(OBJS) $(LDLIBS)

# Build test_expr (expression evaluation tests)
test_expr: test_expr.o $(OBJS)
	$(CC) $(CFLAGS) -o test_expr test_expr.o $(OBJS) $(LDLIBS)

# Generic rule to compile object files (handles ../ paths too)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test_assign4_1.o: test_assign4_1.c dberror.h expr.h btree_mgr.h tables.h test_helper.h
	$(CC) $(CFLAGS) -c test_assign4_1.c

test_expr.o: test_expr.c dberror.h expr.h tables.h test_helper.h
	$(CC) $(CFLAGS) -c test_expr.c

# Run tests
test1: test_assign4_1
	./test_assign4_1

test2: test_expr
	./test_expr

test3: test_assign4_2
	./test_assign4_2

valgrind_test:
	valgrind --leak-check=full --show-leak-kinds=all \
	--track-origins=yes ./test_assign4_1

# Clean build artifacts
clean:
	rm -f *.o test_assign4_1 test_expr *.bin ../assign3/*.o

.PHONY: all clean test1 test2 test3 valgrind_test
