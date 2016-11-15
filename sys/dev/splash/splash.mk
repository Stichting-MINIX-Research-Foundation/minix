# $NetBSD: splash.mk,v 1.6 2015/09/15 02:22:43 uebayasi Exp $

.if defined(SPLASHSCREEN_IMAGE)

# Makefile for embedding splash image into kernel.
.include <bsd.endian.mk>

.if (${OBJECT_FMTS:Melf64})
BFD_ELFTARGET=elf64
.else
BFD_ELFTARGET=elf32
.endif

BFD_ENDIANNESS=${TARGET_ENDIANNESS:S/1234/little/C/4321/big/}
BFD_CPU=${MACHINE_CPU:S/_/-/}

.if (${BFD_CPU:Maarch64} || ${BFD_CPU:Marm} || ${BFD_CPU:Mmips} || ${BFD_CPU:Mscore})
BFD_TARGET=${BFD_ELFTARGET}-${BFD_ENDIANNESS}${BFD_CPU}
.else
BFD_TARGET=${BFD_ELFTARGET}-${BFD_CPU}
.endif

splash_image.o:	${SPLASHSCREEN_IMAGE}
	${_MKTARGET_CREATE}
	cp ${SPLASHSCREEN_IMAGE} splash.image
	${OBJCOPY} -I binary -B ${MACHINE_CPU:C/x86_64/i386/} \
		-O ${BFD_TARGET} splash.image splash_image.o
	rm splash.image
.else

# SPLASHSCREEN_IMAGE is not defined; build empty splash_image.o.
splash_image.c:
	${_MKTARGET_CREATE}
	echo > $@

.endif
