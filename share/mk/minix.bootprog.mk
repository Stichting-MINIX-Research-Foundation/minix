# MINIX-specific boot program options
.include <bsd.own.mk>

.if ${OBJECT_FMT} == "ELF"
LDFLAGS+= -Wl,--section-start=.init=0x0
.endif

.include <minix.service.mk>
