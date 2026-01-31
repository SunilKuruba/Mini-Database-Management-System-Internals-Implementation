CC      := gcc
CFLAGS  := -g -Wall
LDFLAGS :=
LDLIBS  := -lm

TARGET  := test1
OBJS    := test_assign1_1.o storage_mgr.o dberror.o

# Default
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Compile any .c -> .o (no -lm here; that’s for linking)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean run test
clean:
	$(RM) $(TARGET) *.o *~

run: $(TARGET)
	./$(TARGET)

test: run
