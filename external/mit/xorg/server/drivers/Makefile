#	$NetBSD: Makefile,v 1.63 2012/07/19 19:23:52 macallan Exp $

# xf86-input drivers

SUBDIR= \
	xf86-input-keyboard \
	xf86-input-mouse \
	#MINIX: xf86-input-ws

.if ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "x86_64"
SUBDIR+= \
	xf86-input-vmmouse

.if ${MACHINE_ARCH} == "i386"
SUBDIR+= \
	xf86-input-elographics
.endif

.endif


# xf86-video drivers

.if ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "x86_64"
SUBDIR+= \
	xf86-video-apm \
	xf86-video-ark \
	xf86-video-ast \
	xf86-video-ati \
	xf86-video-chips \
	xf86-video-cirrus \
	xf86-video-cirrus_alpine \
	xf86-video-cirrus_laguna \
	xf86-video-glint \
	xf86-video-i128 \
	xf86-video-i740 \

# needs porting for xorg-server 1.6
#	xf86-video-imstt \

SUBDIR+= \
	xf86-video-intel \
	xf86-video-mach64 \
	xf86-video-mga \
	xf86-video-neomagic \
	xf86-video-nv \
	xf86-video-openchrome \
	xf86-video-r128 \
	xf86-video-radeon \
	xf86-video-radeonhd \
	xf86-video-s3 \
	xf86-video-s3virge \
	xf86-video-savage \
	xf86-video-siliconmotion \
	xf86-video-sis \
	xf86-video-tdfx \
	xf86-video-tga \
	xf86-video-trident \
	xf86-video-tseng \
	xf86-video-vmware \
	xf86-video-vesa \
	xf86-video-xgi
#	xf86-video-rendition

# does not compile yet on MINIX
#	xf86-video-wsfb \

.if ${MACHINE_ARCH} == "i386"
SUBDIR+= \
	xf86-video-geode
.endif	# ${MACHINE_ARCH} == "i386"

.endif	# ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"

.if ${MACHINE} == "alpha"
SUBDIR+= \
	xf86-video-apm \
	xf86-video-ark \
	xf86-video-ast \
	xf86-video-ati \
	xf86-video-chips \
	xf86-video-cirrus \
	xf86-video-cirrus_alpine \
	xf86-video-cirrus_laguna \
	xf86-video-glint \
	xf86-video-i128 \
	xf86-video-i740 \
	xf86-video-mach64 \
	xf86-video-mga \
	xf86-video-nv \
	xf86-video-r128 \
	xf86-video-radeon \
	xf86-video-radeonhd \
	xf86-video-s3 \
	xf86-video-s3virge \
	xf86-video-savage \
	xf86-video-siliconmotion \
	xf86-video-sis \
	xf86-video-tdfx \
	xf86-video-tga \
	xf86-video-trident \
	xf86-video-tseng \
	xf86-video-wsfb
#	xf86-video-imstt
.endif	# ${MACHINE} == "alpha"

.if ${MACHINE} == "bebox"
SUBDIR+= \
	xf86-video-cirrus \
	xf86-video-cirrus_alpine \
	xf86-video-mga \
	xf86-video-s3 \
	xf86-video-s3virge
.endif	# ${MACHINE} == "bebox"

.if ${MACHINE} == "cats"
SUBDIR+= \
	xf86-video-apm \
	xf86-video-ark \
	xf86-video-ast \
	xf86-video-ati \
	xf86-video-chips \
	xf86-video-cirrus \
	xf86-video-cirrus_alpine \
	xf86-video-cirrus_laguna \
	xf86-video-glint \
	xf86-video-i128 \
	xf86-video-i740 \
	xf86-video-mga \
	xf86-video-neomagic \
	xf86-video-nv \
	xf86-video-radeon \
	xf86-video-radeonhd \
	xf86-video-s3 \
	xf86-video-s3virge \
	xf86-video-savage \
	xf86-video-siliconmotion \
	xf86-video-tdfx \
	xf86-video-tga \
	xf86-video-trident \
	xf86-video-tseng \
	xf86-video-vesa \
	xf86-video-wsfb
.endif	# ${MACHINE} == "cats"

.if ${MACHINE} == "dreamcast"
SUBDIR+= \
	xf86-video-wsfb
.endif	# ${MACHINE} == "dreamcast"

.if ${MACHINE} == "evbarm"
SUBDIR+= \
	#xf86-video-wsfb
.endif # ${MACHINE} == "evbarm"

.if ${MACHINE} == "evbmips"
SUBDIR+= \
	xf86-video-siliconmotion \
	xf86-video-sis \
	xf86-video-wsfb
.endif	# ${MACHINE} == "evbmips"

.if ${MACHINE} == "ews4800mips"
SUBDIR+= \
	xf86-video-wsfb
.endif	# ${MACHINE} == "ews4800mips"

.if ${MACHINE} == "hp300"
SUBDIR+= \
	xf86-video-wsfb
.endif	# ${MACHINE} == "hp300"

.if ${MACHINE} == "hpcarm" || \
    ${MACHINE} == "hpcmips" || \
    ${MACHINE} == "hpcsh"
SUBDIR+= \
	xf86-video-wsfb
.endif # ${MACHINE} == "hpcarm" || ${MACHINE} == "hpcmips" || ${MACHINE} == "hpcsh"

.if ${MACHINE} == "luna68k"
SUBDIR+= \
	xf86-video-wsfb
.endif	# ${MACHINE} == "luna68k"

.if ${MACHINE} == "macppc"
SUBDIR+= \
	xf86-video-ati \
	xf86-video-chips \
	xf86-video-mach64 \
	xf86-video-mga \
	xf86-video-nv \
	xf86-video-radeon \
	xf86-video-r128 \
	xf86-video-tdfx \
	xf86-video-wsfb
#	xf86-video-imstt
.endif	# ${MACHINE} == "macppc"

.if ${MACHINE} == "netwinder"
SUBDIR+= \
	xf86-video-wsfb
#	xf86-video-igs
.endif	# ${MACHINE} == "netwinder"

.if ${MACHINE} == "newsmips"
SUBDIR+= \
	xf86-video-wsfb
.endif	# ${MACHINE} == "newsmips"

.if ${MACHINE} == "ofppc"
SUBDIR+= \
	xf86-video-ati \
	xf86-video-chips \
	xf86-video-mach64 \
	xf86-video-mga \
	xf86-video-nv \
	xf86-video-radeon \
	xf86-video-r128 \
	xf86-video-tdfx \
	xf86-video-wsfb
.endif	# ${MACHINE} == "ofppc"

.if ${MACHINE} == "prep"
SUBDIR+= \
	xf86-video-cirrus \
	xf86-video-cirrus_alpine \
	xf86-video-mga \
	xf86-video-s3
.endif	# ${MACHINE} == "prep"

.if ${MACHINE} == "sgimips"
SUBDIR+= \
	xf86-video-crime \
	xf86-video-newport \
	xf86-video-wsfb
.endif	# ${MACHINE} == "sgimips"

.if ${MACHINE} == "shark"
SUBDIR+= \
	xf86-video-chips \
        xf86-video-igs \
        xf86-video-wsfb
.endif	# ${MACHINE} == "shark"

.if ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64"
SUBDIR+= \
	xf86-video-ag10e \
	xf86-video-ati \
	xf86-video-glint \
	xf86-video-mach64 \
	xf86-video-mga \
	xf86-video-r128 \
	xf86-video-radeon \
	xf86-video-suncg6 \
	xf86-video-sunffb \
	xf86-video-sunleo \
	xf86-video-wsfb
.if ${MACHINE} == "sparc"
SUBDIR+= \
	xf86-video-pnozz \
	xf86-video-suncg14 \
	xf86-video-suntcx
.endif	# ${MACHINE} == "sparc"
.endif	# ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"

.if ${MACHINE} == "vax"
SUBDIR+= \
	xf86-video-wsfb
.endif	# ${MACHINE} == "vax"

.if ${MACHINE} == "zaurus"
SUBDIR+= \
	xf86-video-wsfb
.endif # ${MACHINE} == "zaurus"

# broken with xorg-server 1.6
# x86
#	xf86-video-nsc \
#	xf86-video-vga \
# alpha
#	xf86-video-vga \
# bebox
#	xf86-video-vga
# prep
#	xf86-video-vga

.include <bsd.subdir.mk>
