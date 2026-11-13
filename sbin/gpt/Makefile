# $NetBSD: Makefile,v 1.19 2019/06/25 04:53:40 jnemeth Exp $
# $FreeBSD: src/sbin/gpt/Makefile,v 1.7 2005/09/01 02:49:20 marcel Exp $

.include <bsd.own.mk>

WARNS=6
PROG=	gpt
SRCS=	add.c biosboot.c create.c destroy.c gpt.c header.c label.c map.c \
	main.c migrate.c recover.c remove.c resize.c resizedisk.c \
	set.c show.c type.c unset.c gpt_uuid.c uuid.c
MAN=	gpt.8

#LINKS=  ${BINDIR}/gpt ${BINDIR}/gptlabel
#MLINKS= gpt.8 gptlabel.8

.if (${HOSTPROG:U} == "")
SRCS+= backup.c restore.c
LDADD+=	-lprop -lutil
DPADD+= ${LIBPROP} ${LIBUTIL}
.endif

.include <bsd.prog.mk>
