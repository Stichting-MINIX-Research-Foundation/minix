# Makefile for the boot monitor package.

# XXX: Can only be built with ACK currently
CC:=${CC:C/^gcc/cc/}
COMPILER_TYPE:=ack

PROGS=	bootblock cdbootblock bootexec boot masterboot \
	jumpboot installboot edparams

SRCS.bootblock=		bootblock.s
CPPFLAGS.bootblock.s=	${I86CPPFLAGS}
LDFLAGS.bootblock=	${I86LDFLAGS}
BINDIR.bootblock=	/usr/mdec
MAN.bootblock=

SRCS.cdbootblock=	cdbootblock.s
CPPFLAGS.cdbootblock.s=	${I86CPPFLAGS} -DCDBOOT
LDFLAGS.cdbootblock=	${I86LDFLAGS}
BINDIR.cdbootblock=	/usr/mdec
MAN.cdbootblock=

SRCS.bootexec=		boothead.s boot.c bootimage.c rawfs86.c
CPPFLAGS.boothead.s=	${I86CPPFLAGS}
CPPFLAGS.boot.c=	${I86CPPFLAGS}
CPPFLAGS.bootimage.c=	${I86CPPFLAGS}
CPPFLAGS.rawfs86.c=	${I86CPPFLAGS}
LDFLAGS.bootexec=	${I86LDFLAGS}
DPADD.bootexec=		${LIBSYS}
LDADD.bootexec=		-lsys
BINDIR.bootexec=	/usr/mdec
MAN.bootexec=

BINDIR.boot=		/usr/mdec
MAN.boot=

SRCS.masterboot=	masterboot.s
CPPFLAGS.masterboot.s=	${I86CPPFLAGS}
LDFLAGS.masterboot=	${I86LDFLAGS}
BINDIR.masterboot=	/usr/mdec
MAN.masterboot=

SRCS.jumpboot=		jumpboot.s
CPPFLAGS.jumpboot.s=	${I86CPPFLAGS}
LDFLAGS.jumpboot=	${I86LDFLAGS}
BINDIR.jumpboot=	/usr/mdec
MAN.jumpboot=

SRCS.installboot=	installboot.c rawfs.c
BINDIR.installboot=	/usr/bin
MAN.installboot=

SRCS.edparams=		edparams.c rawfs.c
CPPFLAGS.edparams.c=	-DUNIX
BINDIR.edparams=	/usr/bin
MAN.edparams=

rawfs86.c:	rawfs.c
	ln -f rawfs.c rawfs86.c

edparams.c:	boot.c
	ln -f boot.c edparams.c

cdbootblock.s:	bootblock.s
	ln -f bootblock.s cdbootblock.s

boot: bootexec
	install -S 22kb bootexec
	cp bootexec boot

CPPFLAGS=	-I${MINIXSRCDIR}
AFLAGS=		-I${MINIXSRCDIR}
I86CPPFLAGS=	-mi86 -Was-ncc
I86LDFLAGS=	-mi86 -Was-ncc -.o -com

STRIPFLAG=	-s

CLEANFILES+=	rawfs86.c edparams.c cdbootblock.s

.include <minix.prog.mk>
