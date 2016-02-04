# Makefile for arch-dependent i2c code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:  ${HERE}

SRCS += pci_i2c.c pci_i2c.h pci_i2c_register.h
