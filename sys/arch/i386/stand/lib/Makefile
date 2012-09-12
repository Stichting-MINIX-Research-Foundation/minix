#	$NetBSD: Makefile,v 1.36 2012/08/10 12:18:15 joerg Exp $

S?=	${.CURDIR}/../../../..

LIB= i386
NOPIC=# defined
NOPROFILE=# defined

I386_INCLUDE_DISK?= yes
I386_INCLUDE_DOS?= no
I386_INCLUDE_BUS?= no
I386_INCLUDE_PS2?= yes

AFLAGS.biosdelay.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.biosgetrtc.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.biosgetsystime.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.biosmca.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.biosmemps2.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.biosmem.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.biosmemx.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.biosreboot.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.biosvbe.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.biosvideomode.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.bios_disk.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.bios_pci.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.comio.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.conio.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.dos_file.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.dump_eax.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.message.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.message32.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.pvcopy.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.putstr.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.putstr32.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}
AFLAGS.realprot.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}

CPPFLAGS= -I$S/lib/libsa ${I386CPPFLAGS} ${I386MISCCPPFLAGS}
#CPPFLAGS+= -DDISK_DEBUG
#CPPFLAGS+= -DNO_DISKLABEL
#CPPFLAGS+= -DNO_GPT
#CPPFLAGS+= -DSAVE_MEMORY

SRCS= pcio.c conio.S comio.S comio_direct.c biosvideomode.S
SRCS+= getsecs.c biosgetrtc.S biosdelay.S biosreboot.S gatea20.c
SRCS+= biosmem.S getextmemx.c biosmemx.S printmemlist.c
SRCS+= pread.c menuutils.c parseutils.c
SRCS+= bootinfo.c bootinfo_biosgeom.c bootinfo_memmap.c
SRCS+= startprog.S multiboot.S
SRCS+= biosgetsystime.S cpufunc.S bootmenu.c
SRCS+= realprot.S message.S message32.S dump_eax.S pvcopy.S putstr.S putstr32.S
SRCS+= rasops.c vbe.c biosvbe.S
.if (${I386_INCLUDE_DISK} == "yes")
SRCS+= biosdisk.c biosdisk_ll.c bios_disk.S
.endif
.if (${I386_INCLUDE_DOS} == "yes")
SRCS+= dosfile.c dos_file.S
.endif
.if (${I386_INCLUDE_DISK} == "yes") || (${I386_INCLUDE_DOS} == "yes")
SRCS+= diskbuf.c
.endif
.if (${I386_INCLUDE_BUS} == "yes")
SRCS+= biospci.c bios_pci.S isapnp.c isadma.c
.endif
.if (${I386_INCLUDE_PS2} == "yes")
SRCS+= biosmca.S biosmemps2.S
.endif

.include <bsd.own.mk>
.undef DESTDIR
.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `lorder ${OBJS} | tsort`

# XXX
.if ${HAVE_GCC:U} == 45
COPTS.biosdisk.c+=	-fno-strict-aliasing
.endif
