.include <bsd.own.mk>
.PATH: ${MINIXSRCDIR}/common/include/arch/i386

INCS+=	archtypes.h bios.h cmos.h cpu.h diskparm.h fpu.h int86.h \
	interrupt.h memory.h multiboot.h partition.h \
	pci.h pci_amd.h pci_intel.h pci_sis.h pci_via.h \
	ports.h stackframe.h vm.h elf.h

