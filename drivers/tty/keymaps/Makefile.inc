FILESDIR= /usr/lib/keymaps

.SUFFIXES:      .src .map

.src.map:
	${_MKTARGET_CREATE}
	$(HOST_CC) $(LDFLAGS) -DKEYSRC=\"$<\" -o $<_genmap ${.CURDIR}/genmap.c
	./$<_genmap > $@
	@rm -f $<_genmap

realall: ${FILES}

CLEANFILES+= ${FILES}
