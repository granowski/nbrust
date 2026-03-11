CC=	cc
CFLAGS=	-O2 -Wall -Iinclude -Icargo
SRCS=	src/main.c src/lexer.c src/parser.c src/codegen.c src/codegen_arm64.c src/codegen_armv6.c \
	src/types.c src/symbol_table.c src/type_checker.c src/macro_expand.c src/borrow_checker.c
CARGO_SRCS=	cargo/main.c cargo/toml.c
OBJDIR=	obj
OBJS=	${SRCS:src/%.c=${OBJDIR}/%.o}
CARGO_OBJS=	${OBJDIR}/cargo_main.o ${OBJDIR}/toml.o
PROG=	nbrust
CARGO_PROG=	nbcargo

.PHONY: all clean

all: ${OBJDIR} ${PROG} ${CARGO_PROG}

${OBJDIR}:
	mkdir -p ${OBJDIR}

${OBJDIR}/%.o: src/%.c
	${CC} ${CFLAGS} -c -o $@ $<

${OBJDIR}/cargo_main.o: cargo/main.c
	${CC} ${CFLAGS} -c -o $@ $<

${OBJDIR}/toml.o: cargo/toml.c
	${CC} ${CFLAGS} -c -o $@ $<

${PROG}: ${OBJS}
	${CC} ${CFLAGS} -o ${PROG} ${OBJS}

${CARGO_PROG}: ${CARGO_OBJS}
	${CC} ${CFLAGS} -o ${CARGO_PROG} ${CARGO_OBJS}

clean:
	rm -rf ${OBJDIR} ${PROG} ${CARGO_PROG}
