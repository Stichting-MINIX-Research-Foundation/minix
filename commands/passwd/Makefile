PROG=	passwd
BINMODE= 4755
MAN=
.if defined(NBSD_LIBC) && (${NBSD_LIBC} != "no")
LDADD+=        -lcrypt
.endif

LINKS+=      ${BINDIR}/passwd ${BINDIR}/chsh
LINKS+=      ${BINDIR}/passwd ${BINDIR}/chfn

.include <bsd.prog.mk>
