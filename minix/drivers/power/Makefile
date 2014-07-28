.include <bsd.own.mk>

.if ${MACHINE_ARCH} == "i386"
SUBDIR+=	acpi
.endif # ${MACHINE_ARCH} == "i386"

.if ${MACHINE_ARCH} == "earm"
SUBDIR+=	tps65217
SUBDIR+=	tps65950
.endif # ${MACHINE_ARCH} == "earm"

.include <bsd.subdir.mk>
