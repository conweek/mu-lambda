CC       = gcc
CFLAGS   = -Wall -Wextra -g -I$(SRCDIR)
DEFINES  =

SRCDIR   = src
TESTDIR  = tests/src
BUILDDIR = build

SRCS     = $(wildcard $(SRCDIR)/*.c)
OBJS     = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))
# Everything except main.o, linked into tests
LIB_OBJS = $(filter-out $(BUILDDIR)/main.o,$(OBJS))

TARGET   = $(BUILDDIR)/mu-lambda

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEFINES) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Tests — one rule per test, add new ones following the pattern
test-tokeniser: $(BUILDDIR)/tokeniser-test
	./$(BUILDDIR)/tokeniser-test

$(BUILDDIR)/tokeniser-test: $(TESTDIR)/tokeniser-test.c $(LIB_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEFINES) -o $@ $< $(LIB_OBJS)

test-arena: $(BUILDDIR)/test_arena
	./$(BUILDDIR)/test_arena

$(BUILDDIR)/test_arena: $(TESTDIR)/test_arena.c $(LIB_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEFINES) -o $@ $< $(LIB_OBJS)

test-parser: $(BUILDDIR)/parser-test
	./$(BUILDDIR)/parser-test

$(BUILDDIR)/parser-test: $(TESTDIR)/parser-test.c $(LIB_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEFINES) -o $@ $< $(LIB_OBJS)

test-interpreter: $(BUILDDIR)/interpreter-test
	./$(BUILDDIR)/interpreter-test

$(BUILDDIR)/interpreter-test: $(TESTDIR)/interpreter-test.c $(LIB_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEFINES) -o $@ $< $(LIB_OBJS)

test-parser-suite: $(BUILDDIR)/parser-suite
	./$(BUILDDIR)/parser-suite

$(BUILDDIR)/parser-suite: $(TESTDIR)/parser-suite.c $(LIB_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEFINES) -o $@ $< $(LIB_OBJS)

test-interpreter-suite: $(BUILDDIR)/interpreter-suite
	./$(BUILDDIR)/interpreter-suite

$(BUILDDIR)/interpreter-suite: $(TESTDIR)/interpreter-suite.c $(LIB_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(DEFINES) -o $@ $< $(LIB_OBJS)

test: test-tokeniser test-arena test-parser test-parser-suite test-interpreter-suite

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean test test-tokeniser test-arena test-parser test-parser-suite test-interpreter test-interpreter-suite
