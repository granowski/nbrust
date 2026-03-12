CC=		cc
CFLAGS=		-O2 -Wall -Iinclude -Icargo
OBJDIR=		obj
PROG=		nbrust
CARGO_PROG=	nbcargo

OBJS=		${OBJDIR}/main.o \
		${OBJDIR}/lexer.o \
		${OBJDIR}/parser.o \
		${OBJDIR}/codegen.o \
		${OBJDIR}/codegen_arm64.o \
		${OBJDIR}/codegen_armv6.o \
		${OBJDIR}/types.o \
		${OBJDIR}/symbol_table.o \
		${OBJDIR}/type_checker.o \
		${OBJDIR}/macro_expand.o \
		${OBJDIR}/borrow_checker.o \
		${OBJDIR}/monomorphization.o

CARGO_OBJS=	${OBJDIR}/cargo_main.o \
		${OBJDIR}/toml.o

.PHONY: all clean

all: ${OBJDIR} ${PROG} ${CARGO_PROG}

${OBJDIR}:
	mkdir -p ${OBJDIR}

.SUFFIXES: .c .o

# Portable rules using $< and $@
${OBJDIR}/main.o: src/main.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/lexer.o: src/lexer.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/parser.o: src/parser.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/codegen.o: src/codegen.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/codegen_arm64.o: src/codegen_arm64.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/codegen_armv6.o: src/codegen_armv6.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/types.o: src/types.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/symbol_table.o: src/symbol_table.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/type_checker.o: src/type_checker.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/macro_expand.o: src/macro_expand.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/borrow_checker.o: src/borrow_checker.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/monomorphization.o: src/monomorphization.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/cargo_main.o: cargo/main.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${OBJDIR}/toml.o: cargo/toml.c | ${OBJDIR}
	${CC} ${CFLAGS} -c $< -o $@

${PROG}: ${OBJS}
	${CC} ${CFLAGS} -o ${PROG} ${OBJS}

${CARGO_PROG}: ${CARGO_OBJS}
	${CC} ${CFLAGS} -o ${CARGO_PROG} ${CARGO_OBJS}

clean:
	rm -rf ${OBJDIR} ${PROG} ${CARGO_PROG}
