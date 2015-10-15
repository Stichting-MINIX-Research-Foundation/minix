.include <bsd.own.mk>

.if defined(__MINIX)
CWARNFLAGS.gcc+=	-Wno-deprecated-declarations
.endif # defined(__MINIX)
