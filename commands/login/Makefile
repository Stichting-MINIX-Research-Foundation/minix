PROG=	login
MAN=
.if defined(NBSD_LIBC) && (${NBSD_LIBC} != "no")
LDADD+=	-lcrypt
.endif

.include <bsd.prog.mk>
