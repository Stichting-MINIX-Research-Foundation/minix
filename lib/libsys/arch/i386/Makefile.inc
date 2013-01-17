# Makefile for arch-dependent libsys code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:  ${HERE}

SRCS+=  \
        get_randomness.c \
        getidle.c \
        profile.c \
        profile_extern.c \
        ser_putc.c \
        spin.c \
        sys_eniop.c \
        sys_in.c \
        sys_int86.c \
        sys_out.c \
        sys_readbios.c \
        sys_sdevio.c \
        sys_umap_remote.c \
        sys_vinb.c \
        sys_vinl.c \
        sys_vinw.c \
        sys_voutb.c \
        sys_voutl.c \
        sys_voutw.c \
        timing.c \
        tsc_util.c \
        vbox.c

CPPFLAGS+=        -I${HERE}/../../


