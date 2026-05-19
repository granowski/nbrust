# OS detection
UNAME_S := $(shell uname -s)

CFLAGS += -Iinclude -Icargo -Wall -O2
CC = cc

ifeq ($(UNAME_S),NetBSD)
    CFLAGS += -DNETBSD
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -DAPPLE
    CC = clang
endif
ifeq ($(UNAME_S),Linux)
    CFLAGS += -DLINUX
    CC = gcc
endif

OBJDIR = obj
PROGS = nbrust nbcargo

# Source files
SRCS_NBRUST = $(wildcard src/*.c)
OBJS_NBRUST = $(patsubst %.c, $(OBJDIR)/%.o, $(SRCS_NBRUST))

SRCS_NBCARGO = $(wildcard cargo/*.c)
OBJS_NBCARGO = $(patsubst %.c, $(OBJDIR)/%.o, $(SRCS_NBCARGO))

# Build rules
all: $(PROGS)

nbrust: $(OBJS_NBRUST)
	$(CC) $(CFLAGS) -o $@ $^

nbcargo: $(OBJS_NBCARGO)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(PROGS)
	rm -rf tmp/

test: all
	@echo "Testing nbrust..."
	./nbrust --help
	@echo "Testing nbcargo..."
	./nbcargo --help

.PHONY: all clean test
