FILESDIR= /usr/lib/keymaps

.SUFFIXES:      .src .map

.src.map:
	${_MKTARGET_CREATE}
	${HOST_CC} ${HOST_LDFLAGS}  -DKEYSRC=\"$<\" -o ${.OBJDIR}/${.TARGET}_genmap ${.CURDIR}/genmap.c
	${.OBJDIR}/${.TARGET}_genmap > ${.OBJDIR}/${.TARGET}
	rm -f ${.OBJDIR}/${.TARGET}_genmap

realall: ${FILES}

CLEANFILES+= ${FILES}
