FILESDIR= ${DESTDIR}/usr/lib/keymaps

.SUFFIXES:      .src .map

.src.map:
	${_MKTARGET_CREATE}
	$(CC) -DKEYSRC=\"$<\" genmap.c
	./a.out > $@
	@rm -f a.out

realall: ${FILES}

clean:
	rm -f ${FILES}
