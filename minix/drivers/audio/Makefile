.include <bsd.own.mk>

# LP64-audited for x86_64.  als4000/cmi8738/cs4281/trident are MMIO drivers
# (BAR widening, see docs/drivers-x86_64.md); es1370/es1371 are PCI port-I/O
# and sb16 is ISA port-I/O + 8237 DMA -- no MMIO, 32-bit/ISA DMA inherent.
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
SUBDIR+=	als4000
SUBDIR+=	cmi8738
SUBDIR+=	cs4281
SUBDIR+=	es1370
SUBDIR+=	es1371
SUBDIR+=	sb16
SUBDIR+=	trident
.endif # ${MACHINE_ARCH} == "i386" || x86_64

.include <bsd.subdir.mk>
