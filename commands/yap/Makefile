# yap Makefile

# XXX: Can only be built with ACK currently
CC:=${CC:C/^gcc/cc/}
COMPILER_TYPE:=ack

PROG=	yap
SRCS=	assert.c commands.c display.c getcomm.c getline.c help.c \
	keys.c machine.c main.c options.c output.c pattern.c \
	process.c prompt.c term.c
CPPFLAGS+= -wa
LINKS+=	${BINDIR}/yap ${BINDIR}/more
MAN=

.include <minix.prog.mk>
