CC=	cc
CFLAGS=	-O2 -Wall -Iinclude -Icargo
SRCS=	src/main.c src/lexer.c src/parser.c src/codegen.c src/codegen_arm64.c src/codegen_armv6.c \
	src/types.c src/symbol_table.c src/type_checker.c src/macro_expand.c src/borrow_checker.c src/monomorphization.c
CARGO_SRCS=	cargo/main.c cargo/toml.c
OBJDIR=	obj
OBJS=	${OBJDIR}/main.o ${OBJDIR}/lexer.o ${OBJDIR}/parser.o ${OBJDIR}/codegen.o \
	${OBJDIR}/codegen_arm64.o ${OBJDIR}/codegen_armv6.o ${OBJDIR}/types.o \
	${OBJDIR}/symbol_table.o ${OBJDIR}/type_checker.o ${OBJDIR}/macro_expand.o \
	${OBJDIR}/borrow_checker.o ${OBJDIR}/monomorphization.o
CARGO_OBJS=	${OBJDIR}/cargo_main.o ${OBJDIR}/toml.o
PROG=	nbrust
CARGO_PROG=	nbcargo

.PHONY: all clean

all: ${OBJDIR} ${PROG} ${CARGO_PROG}

${OBJDIR}:
	mkdir -p ${OBJDIR}

.SUFFIXES: .c .o

${OBJDIR}/main.o: src/main.c
	${CC} ${CFLAGS} -c -o $@ src/main.c

${OBJDIR}/lexer.o: src/lexer.c
	${CC} ${CFLAGS} -c -o $@ src/lexer.c

${OBJDIR}/parser.o: src/parser.c
	${CC} ${CFLAGS} -c -o $@ src/parser.c

${OBJDIR}/codegen.o: src/codegen.c
	${CC} ${CFLAGS} -c -o $@ src/codegen.c

${OBJDIR}/codegen_arm64.o: src/codegen_arm64.c
	${CC} ${CFLAGS} -c -o $@ src/codegen_arm64.c

${OBJDIR}/codegen_armv6.o: src/codegen_armv6.c
	${CC} ${CFLAGS} -c -o $@ src/codegen_armv6.c

${OBJDIR}/types.o: src/types.c
	${CC} ${CFLAGS} -c -o $@ src/types.c

${OBJDIR}/symbol_table.o: src/symbol_table.c
	${CC} ${CFLAGS} -c -o $@ src/symbol_table.c

${OBJDIR}/type_checker.o: src/type_checker.c
	${CC} ${CFLAGS} -c -o $@ src/type_checker.c

${OBJDIR}/macro_expand.o: src/macro_expand.c
	${CC} ${CFLAGS} -c -o $@ src/macro_expand.c

${OBJDIR}/borrow_checker.o: src/borrow_checker.c
	${CC} ${CFLAGS} -c -o $@ src/borrow_checker.c

${OBJDIR}/monomorphization.o: src/monomorphization.c
	${CC} ${CFLAGS} -c -o $@ src/monomorphization.c

${OBJDIR}/cargo_main.o: cargo/main.c
	${CC} ${CFLAGS} -c -o $@ cargo/main.c

${OBJDIR}/toml.o: cargo/toml.c
	${CC} ${CFLAGS} -c -o $@ cargo/toml.c

${PROG}: ${OBJS}
	${CC} ${CFLAGS} -o ${PROG} ${OBJS}

${CARGO_PROG}: ${CARGO_OBJS}
	${CC} ${CFLAGS} -o ${CARGO_PROG} ${CARGO_OBJS}

clean:
	rm -rf ${OBJDIR} ${PROG} ${CARGO_PROG}
