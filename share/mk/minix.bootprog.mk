# MINIX-specific boot program options
.include <bsd.own.mk>

LDFLAGS+= -Wl,--section-start=.init=0x0

.include <minix.service.mk>
