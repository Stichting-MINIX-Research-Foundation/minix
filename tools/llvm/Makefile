#	$NetBSD: Makefile,v 1.20 2015/03/05 10:22:25 joerg Exp $

.include <bsd.init.mk>

#
# Do *not* set HOSTPROG or HOSTLIB at this point otherwise
# it will create a loop trying to extract the object directory.
#
.include "Makefile.inc"
.include "${LLVM_TOPLEVEL}/lib/Makefile.inc"

realdepends realall: config/config.status need-dl need-terminfo

config/config.status: ${LLVM_SRCDIR}/configure
	mkdir -p config
	printf '#!/bin/sh\necho 2.7.3' > config/python
	chmod 755 config/python
	cd config && ${HOST_SH} ${LLVM_SRCDIR}/configure ${LLVM_CONFIGURE_ARGS} \
	    --enable-optimized CC=${HOST_CC:Q} CXX=${HOST_CXX:Q} \
	    --with-python=${.OBJDIR}/config/python
# --disable-assertions

need-dl:
	printf '#include <dlfcn.h>\nint main(void){void *p; return dladdr(p, p);}' > need-dl.c
	if ${HOST_CC} -o need-dl.out -D_GNU_SOURCE need-dl.c > /dev/null 2>&1; then \
		echo > ${.TARGET}; \
	elif ${HOST_CC} -o need-dl.out -D_GNU_SOURCE need-dl.c -ldl > /dev/null 2>&1; then \
		echo -ldl > ${.TARGET}; \
	else \
		echo > ${.TARGET}; \
	fi

need-terminfo:
	printf '#include <term.h>\nint main(void){return setupterm(0, 0, 0);}' > need-terminfo.c
	for lib in tinfo terminfo ncurses curses; do \
		if ${HOST_CC} -o need-terminfo.out need-terminfo.c -l$$lib > /dev/null 2>&1; then \
			echo -l$$lib > ${.TARGET}; \
			break; \
		fi; \
	done

CLEANFILES+=	need-dl need-dl.c need-dl.out need-terminfo need-terminfo.c need-terminfo.out

cleandir: cleandir-llvm

cleandir-llvm: .PHONY
	test ! -d config || rm -r config
	rm -f need-dl need-dl.tmp

.include <bsd.hostprog.mk>
