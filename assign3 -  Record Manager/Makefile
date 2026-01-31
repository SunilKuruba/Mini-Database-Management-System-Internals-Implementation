CC      := gcc
CFLAGS  := -g -Wall
LDFLAGS :=
LDLIBS  := -lm

OBJS = storage_mgr.o buffer_mgr.o buffer_mgr_stat.o dberror.o record_mgr.o rm_serializer.o expr.o

all: test_assign3_1 test_expr

# Build test_assign3_1 (main test suite)
test_assign3_1: test_assign3_1.o $(OBJS)
	$(CC) $(CFLAGS) -o test_assign3_1 test_assign3_1.o $(OBJS) $(LDLIBS)

# Build test_expr (expression evaluation tests)
test_expr: test_expr.o $(OBJS)
	$(CC) $(CFLAGS) -o test_expr test_expr.o $(OBJS) $(LDLIBS)

# Object file compilation
storage_mgr.o: storage_mgr.c storage_mgr.h dberror.h
	$(CC) $(CFLAGS) -c storage_mgr.c

buffer_mgr.o: buffer_mgr.c buffer_mgr.h storage_mgr.h dberror.h dt.h
	$(CC) $(CFLAGS) -c buffer_mgr.c

buffer_mgr_stat.o: buffer_mgr_stat.c buffer_mgr_stat.h buffer_mgr.h dberror.h
	$(CC) $(CFLAGS) -c buffer_mgr_stat.c

dberror.o: dberror.c dberror.h
	$(CC) $(CFLAGS) -c dberror.c

record_mgr.o: record_mgr.c record_mgr.h buffer_mgr.h storage_mgr.h dberror.h tables.h dt.h
	$(CC) $(CFLAGS) -c record_mgr.c

rm_serializer.o: rm_serializer.c dberror.h tables.h dt.h
	$(CC) $(CFLAGS) -c rm_serializer.c

expr.o: expr.c expr.h dberror.h tables.h dt.h
	$(CC) $(CFLAGS) -c expr.c

test_assign3_1.o: test_assign3_1.c dberror.h expr.h record_mgr.h tables.h test_helper.h
	$(CC) $(CFLAGS) -c test_assign3_1.c

test_expr.o: test_expr.c dberror.h expr.h tables.h test_helper.h
	$(CC) $(CFLAGS) -c test_expr.c

# Run tests
test1: test_assign3_1
	./test_assign3_1

test2: test_expr
	./test_expr

# Clean build artifacts
clean:
	rm -f *.o test_assign3_1 test_expr *.bin

.PHONY: all clean test1 test2