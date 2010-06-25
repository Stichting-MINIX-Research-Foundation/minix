# Makefile for the boot monitor package.

# XXX: Can only be built with ACK currently
CC:=${CC:C/^gcc/cc/}
COMPILER_TYPE:=ack

PROGS=	bootblock cdbootblock boot masterboot \
	jumpboot installboot edparams dosboot mkfile

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

SRCS.boot=		boothead.s boot.c bootimage.c rawfs86.c
CPPFLAGS.boothead.s=	${I86CPPFLAGS}
CPPFLAGS.boot.c=	${I86CPPFLAGS}
CPPFLAGS.bootimage.c=	${I86CPPFLAGS}
CPPFLAGS.rawfs86.c=	${I86CPPFLAGS}
LDFLAGS.boot=	${I86LDFLAGS}
DPADD.boot=		${LIBSYS}
LDADD.boot=		-lsys
BINDIR.boot=	/usr/mdec
MAN.boot=

realall: .PHONY bootsize
bootsize: boot
	${INSTALL} -S 22kb boot

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

SRCS.dosboot=		doshead.s dosboot.o bootimage.o rawfs86.o
CPPFLAGS.dosboot.c=	-DDOS $(I86CPPFLAGS)
LDADD.dosboot=	${I86LDFLAGS} -lsys
BINDIR.dosboot=	/usr/mdec
CPPFLAGS.doshead.s= -mi386
MAN.dosboot=

SRCS.mkfile=		mkfhead.s mkfile.c
CPPFLAGS.mkfile.s=	${I86CPPFLAGS}
LDADD.mkfile=	${I86LDFLAGS} -lsys
BINDIR.mkfile=	/usr/mdec
MAN.mkfile=

rawfs86.c:	rawfs.c
	ln -f rawfs.c rawfs86.c

edparams.c:	boot.c
	ln -f boot.c edparams.c

dosboot.c: boot.c
	ln -f boot.c dosboot.c

cdbootblock.s:	bootblock.s
	ln -f bootblock.s cdbootblock.s

mkfile.com: mkfile

boot.com: dosboot
	./a.out2com dosboot boot.com

CPPFLAGS=	-I${MINIXSRCDIR}
AFLAGS=		-I${MINIXSRCDIR}
I86CPPFLAGS=	-mi86 -Was-ncc
I86LDFLAGS=	-mi86 -Was-ncc -.o -com

STRIPFLAG=	-s

CLEANFILES+=	rawfs86.c edparams.c cdbootblock.s dosboot.c

.include <bsd.prog.mk>
