
CFLAGS += -Iinclude

# OS detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),NetBSD)
    CFLAGS += -DNETBSD
    CC = cc
endif
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -DAPPLE
    CC = clang
endif
ifeq ($(UNAME_S),Linux)
    CFLAGS += -DLINUX
    CC = gcc
endif

# Common build settings
OBJDIR = obj
PROGS = nbrust nbcargo

# Source files
SRCS_NBRUST = \
	src/main.c \
	src/lexer.c \
	src/parser.c \
	src/codegen.c \
	src/codegen_arm64.c \
	src/codegen_armv6.c \
	src/types.c \
	src/symbol_table.c \
	src/type_checker.c \
	src/macro_expand.c \
	src/borrow_checker.c \
	src/monomorphization.c

SRCS_NBCARGO = \
	cargo/main.c \
	cargo/toml.c

# Build rules
all: ${PROGS}

nbrust: ${SRCS_NBRUST}
	${CC} ${CFLAGS} -o nbrust ${SRCS_NBRUST}

nbcargo: ${SRCS_NBCARGO}
	${CC} ${CFLAGS} -o nbcargo ${SRCS_NBCARGO}

# Object file generation
${OBJDIR}/%.o: %.c
	mkdir -p ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

# Clean target
clean:
	rm -rf ${OBJDIR} ${PROGS}
	rm -f src/*.o cargo/*.o

# Test target (optional)
test:
	@echo "Testing nbrust..."
	./nbrust --help
	@echo "Testing nbcargo..."
	./nbcargo --help
