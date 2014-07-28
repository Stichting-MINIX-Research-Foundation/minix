# Generate binary keymaps.

.include <bsd.own.mk>

FILES=	dvorak.map french.map german.map italian.map japanese.map \
	latin-america.map norwegian.map polish.map russian-cp866.map \
	russian.map scandinavian.map spanish.map uk.map \
	us-std.map us-swap.map russian-cp1251.map \
	ukraine-koi8-u.map portuguese.map abnt2.map

FILESDIR= /usr/lib/keymaps

.SUFFIXES:      .src .map

HOST_CPPFLAGS+= -I${NETBSDSRCDIR}/minix

.src.map:
	${_MKTARGET_CREATE}
	${HOST_CC} ${HOST_CFLAGS} ${HOST_CPPFLAGS} ${HOST_LDFLAGS} -DKEYSRC=\"$<\" -o ${.OBJDIR}/${.TARGET}_genmap ${.CURDIR}/genmap.c
	${.OBJDIR}/${.TARGET}_genmap > ${.OBJDIR}/${.TARGET}
	rm -f ${.OBJDIR}/${.TARGET}_genmap

realall: ${FILES}

CLEANFILES+= ${FILES}

.include <bsd.prog.mk>
