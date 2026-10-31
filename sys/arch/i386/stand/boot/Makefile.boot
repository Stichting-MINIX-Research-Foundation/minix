# $NetBSD: Makefile.boot,v 1.72.4.1 2019/09/17 19:32:00 martin Exp $

S=	${.CURDIR}/../../../../..

NOMAN=
NOLIBCSANITIZER=
NOSANITIZER=
NOPIE=
PROG?= boot
NEWVERSWHAT?= "BIOS Boot"

AFLAGS.biosboot.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}

SOURCES?= biosboot.S boot2.c conf.c devopen.c exec.c \
	  exec_multiboot1.c exec_multiboot2.c
SRCS= ${SOURCES}

.include <bsd.init.mk>

STRIPFLAG=	# nothing

LIBCRT0=	# nothing
LIBCRTI=	# nothing
LIBCRTBEGIN=	# nothing
LIBCRTEND=	# nothing
LIBC=		# nothing

BINDIR=/usr/mdec
BINMODE=444

.PATH:	${.CURDIR}/.. ${.CURDIR}/../../lib

LDFLAGS+= -nostdlib -Wl,-N -Wl,-e,boot_start
CPPFLAGS+= -I ${.CURDIR}/..  -I ${.CURDIR}/../../lib -I ${S}/lib/libsa
CPPFLAGS+= -I ${.OBJDIR}
# Make sure we override any optimization options specified by the user
COPTS=  -Os

.if ${MACHINE_ARCH} == "x86_64"
LDFLAGS+=  -Wl,-m,elf_i386
# MINIX's clang ignores -m32 and warns that it is unused.
AFLAGS+=   ${${ACTIVE_CC} == "clang":?:-m32}
CPUFLAGS=  ${${ACTIVE_CC} == "clang":?:-m32}
LIBKERN_ARCH=i386
KERNMISCMAKEFLAGS="LIBKERN_ARCH=i386"
.else
CPUFLAGS=  -march=i386 -mtune=i386
.endif

CFLAGS+=   -mno-sse -mno-sse2 -mno-sse3

COPTS+=    -ffreestanding
CFLAGS+= -Wall -Wmissing-prototypes -Wstrict-prototypes
CPPFLAGS+= -nostdinc -D_STANDALONE
CPPFLAGS+= -I$S

CPPFLAGS+= -DSUPPORT_PS2
CPPFLAGS+= -DDIRECT_SERIAL
CPPFLAGS+= -DSUPPORT_SERIAL=boot_params.bp_consdev

CPPFLAGS+= -DCONSPEED=boot_params.bp_conspeed
CPPFLAGS+= -DCONSADDR=boot_params.bp_consaddr
CPPFLAGS+= -DCONSOLE_KEYMAP=boot_params.bp_keymap

CPPFLAGS+= -DSUPPORT_CD9660
CPPFLAGS+= -DSUPPORT_USTARFS
CPPFLAGS+= -DSUPPORT_DOSFS
CPPFLAGS+= -DSUPPORT_EXT2FS
CPPFLAGS+= -DSUPPORT_MINIXFS3
CPPFLAGS+= -DPASS_BIOSGEOM
CPPFLAGS+= -DPASS_MEMMAP
#CPPFLAGS+= -DBOOTPASSWD
CPPFLAGS+= -DEPIA_HACK
#CPPFLAGS+= -DDEBUG_MEMSIZE
#CPPFLAGS+= -DBOOT_MSG_COM0
CPPFLAGS+= -DLIBSA_ENABLE_LS_OP
.if defined(__MINIX)
CPPFLAGS+= -DLIBSA_ENABLE_LOAD_MODS_OP
.endif # defined(__MINIX)

# The biosboot code is linked to 'virtual' address of zero and is
# loaded at physical address 0x10000.
# XXX The heap values should be determined from _end.
SAMISCCPPFLAGS+= -DHEAP_START=0x40000 -DHEAP_LIMIT=0x70000
SAMISCCPPFLAGS+= -DLIBSA_PRINTF_LONGLONG_SUPPORT
SAMISCMAKEFLAGS+= SA_USE_CREAD=yes	# Read compressed kernels
SAMISCMAKEFLAGS+= SA_INCLUDE_NET=no	# Netboot via TFTP, NFS

CPPFLAGS+=	-Wno-pointer-sign

# CPPFLAGS+= -DBOOTXX_RAID1_SUPPORT

I386_STAND_DIR?= $S/arch/i386/stand

### find out what to use for libi386
I386DIR= ${I386_STAND_DIR}/lib
.include "${I386DIR}/Makefile.inc"
LIBI386= ${I386LIB}

### find out what to use for libsa
SA_AS= library
SAMISCMAKEFLAGS+="SA_USE_LOADFILE=yes"
SAMISCMAKEFLAGS+="SA_ENABLE_LS_OP=yes"
.include "${S}/lib/libsa/Makefile.inc"
LIBSA= ${SALIB}

### find out what to use for libkern
KERN_AS= library
.include "${S}/lib/libkern/Makefile.inc"
LIBKERN= ${KERNLIB}

### find out what to use for libz
Z_AS= library
.include "${S}/lib/libz/Makefile.inc"
LIBZ= ${ZLIB}

LDSCRIPT ?= $S/arch/i386/conf/stand.ldscript

cleandir distclean: .WAIT cleanlibdir

cleanlibdir:
	-rm -rf lib

LIBLIST= ${LIBI386} ${LIBSA} ${LIBZ} ${LIBKERN} ${LIBI386} ${LIBSA}
# LIBLIST= ${LIBSA} ${LIBKERN} ${LIBI386} ${LIBSA} ${LIBZ} ${LIBKERN}

CLEANFILES+= ${PROG}.tmp ${PROG}.map ${PROG}.sym

VERSIONMACHINE=x86
.include "${S}/conf/newvers_stand.mk"

.if defined(__MINIX)
# BJG
# -Wl,-Ttext,0 changed to --section-start=.text=0 twice below of a gold problem.
# did not leave both versions in because of the huge continued line.
.endif # defined(__MINIX)

# Anything that calls 'real_to_prot' must have a %pc < 0x10000.
# We link the program, find the callers (all in libi386), then
# explicitly pull in the required objects before any other library code.
${PROG}: ${OBJS} ${LIBLIST} ${LDSCRIPT} ${.CURDIR}/../Makefile.boot
	${_MKTARGET_LINK}
	bb="$$( ${CC} -o ${PROG}.sym ${LDFLAGS} -Wl,--section-start=.text=0 -Wl,-cref \
	    ${OBJS} ${LIBLIST} | ( \
		while read symbol file; do \
			[ -z "$$file" ] && continue; \
			[ "$$symbol" = real_to_prot ] && break; \
		done; \
		while \
			oifs="$$IFS"; \
			IFS='()'; \
			set -- $$file; \
			IFS="$$oifs"; \
			[ -n "$$2" ] && echo "${I386DST}/$$2"; \
			read file rest && [ -z "$$rest" ]; \
		do :; \
		done; \
	) )"; \
	${CC} -o ${PROG}.sym ${LDFLAGS} -Wl,--section-start=.text=0 -T ${LDSCRIPT} \
		-Wl,-Map,${PROG}.map -Wl,-cref ${OBJS} $$bb ${LIBLIST}
	${OBJCOPY} -O binary ${PROG}.sym ${PROG}

.include <bsd.prog.mk>
KLINK_MACHINE=	i386
.include <bsd.klinks.mk>
