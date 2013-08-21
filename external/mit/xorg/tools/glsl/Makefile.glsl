#	$NetBSD: Makefile.glsl,v 1.1 2010/07/19 05:34:28 mrg Exp $

GLSLDIR!=	cd ${NETBSDSRCDIR}/external/mit/xorg/tools/glsl && ${PRINTOBJDIR}
GLSL=	${GLSLDIR}/glsl-compile

${GLSL}:
	(cd ${NETBSDSRCDIR}/external/mit/xorg/tools/glsl && ${MAKE})
