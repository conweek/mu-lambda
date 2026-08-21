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

test: test-tokeniser

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean test test-tokeniser
