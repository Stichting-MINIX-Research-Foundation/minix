# Makefile for the boot monitor package.

.include <bsd.own.mk>

# 16-bit code; various boot blocks
SUBDIR= masterboot bootblock cdbootblock

# Supplementary MBR code (alternate to masterboot)
SUBDIR+= jumpboot

# 16-bit code; needs the i86 ACK library
SUBDIR+= boot

# Userland MINIX code
SUBDIR+= installboot edparams

.include <bsd.subdir.mk>
