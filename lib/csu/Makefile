.include <bsd.own.mk>

.if ${OBJECT_FMT} == "a.out"
SUBDIR=${ARCH}-aout
.elif ${OBJECT_FMT} == "ELF"
SUBDIR=${ARCH}-elf
.endif

.include <bsd.subdir.mk>
