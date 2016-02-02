#	$NetBSD: Makefile,v 1.17 2015/07/01 15:38:56 christos Exp $

.include <bsd.own.mk>

LIB=		GLU
LIBISCXX=	yes

GLUDIR=		${X11SRCDIR.glu}/src

SRCS.libutil=	\
	error.c \
	glue.c \
	mipmap.c \
	project.c \
	quad.c \
	registry.c
.PATH:		${GLUDIR}/libutil

SRCS.libtess=	\
	dict.c \
	geom.c \
	memalloc.c \
	mesh.c \
	normal.c \
	priorityq.c \
	render.c \
	sweep.c \
	tess.c \
	tessmono.c 
.PATH:		${GLUDIR}/libtess

SRCS.interface=	\
	bezierEval.cc \
	bezierPatch.cc \
	bezierPatchMesh.cc \
	glcurveval.cc \
	glinterface.cc \
	glrenderer.cc \
	glsurfeval.cc \
	incurveeval.cc \
	insurfeval.cc
.PATH:		${GLUDIR}/libnurbs/interface

SRCS.internals=	\
	arc.cc \
	arcsorter.cc \
	arctess.cc \
	backend.cc \
	basiccrveval.cc \
	basicsurfeval.cc \
	bin.cc \
	bufpool.cc \
	cachingeval.cc \
	ccw.cc \
	coveandtiler.cc \
	curve.cc \
	curvelist.cc \
	curvesub.cc \
	dataTransform.cc \
	displaylist.cc \
	flist.cc \
	flistsorter.cc \
	hull.cc \
	intersect.cc \
	knotvector.cc \
	mapdesc.cc \
	mapdescv.cc \
	maplist.cc \
	mesher.cc \
	monoTriangulationBackend.cc \
	monotonizer.cc \
	mycode.cc \
	nurbsinterfac.cc \
	nurbstess.cc \
	patch.cc \
	patchlist.cc \
	quilt.cc \
	reader.cc \
	renderhints.cc \
	slicer.cc \
	sorter.cc \
	splitarcs.cc \
	subdivider.cc \
	tobezier.cc \
	trimline.cc \
	trimregion.cc \
	trimvertpool.cc \
	uarray.cc \
	varray.cc
.PATH:		${GLUDIR}/libnurbs/internals
.for fn in ${SRCS.internals}
CPPFLAGS.${fn}+= -D_EXTENSIONS_
.endfor

SRCS.nurbtess=	\
	directedLine.cc \
	gridWrap.cc \
	monoChain.cc \
	monoPolyPart.cc \
	monoTriangulation.cc \
	partitionX.cc \
	partitionY.cc \
	polyDBG.cc \
	polyUtil.cc \
	primitiveStream.cc \
	quicksort.cc \
	rectBlock.cc \
	sampleComp.cc \
	sampleCompBot.cc \
	sampleCompRight.cc \
	sampleCompTop.cc \
	sampleMonoPoly.cc \
	sampledLine.cc \
	searchTree.cc
.PATH:		${GLUDIR}/libnurbs/nurbtess

.if !defined(__MINIX)
CPPFLAGS+=	-DPTHREADS -DUSE_XSHM -DHZ=100 -DNDEBUG -DLIBRARYBUILD
.else
CPPFLAGS+=	-DUSE_XSHM -DHZ=100 -DNDEBUG -DLIBRARYBUILD
.endif # !defined(__MINIX)
CPPFLAGS+=	-I${GLUDIR}/include \
		-I${GLUDIR}/libnurbs/internals \
		-I${GLUDIR}/libnurbs/interface \
		-I${GLUDIR}/libnurbs/nurbtess

SRCS=		${SRCS.libutil} ${SRCS.libtess} \
		${SRCS.interface} ${SRCS.internals} ${SRCS.nurbtess}

.if !defined(__MINIX)
CFLAGS+=	-fno-strict-aliasing -fvisibility=hidden -pthread
.else
CFLAGS+=	-fno-strict-aliasing -fvisibility=hidden
.endif # !defined(__MINIX)

.PATH:		${X11SRCDIR.glu}/include/GL
INCS=	glu.h glu_mangle.h
INCSDIR=${X11INCDIR}/GL

LIBDPLIBS=	Xext	${.CURDIR}/../libXext \
		X11	${.CURDIR}/../libX11/dynamic \
		GL	${.CURDIR}/../libGL \
		Xxf86vm	${.CURDIR}/../libXxf86vm \
		Xfixes	${.CURDIR}/../libXfixes \
		Xdamage	${.CURDIR}/../libXdamage \
		drm	${.CURDIR}/../libdrm \
		m	${NETBSDSRCDIR}/lib/libm

PKGCONFIG=	glu
PKGDIST.glu=	${X11SRCDIR.glu}
.include "${.CURDIR}/../libGL/mesa-ver.mk"
PKGCONFIG_VERSION.glu=	${MESA_VER}
PKGCONFIG_SED_FLAGS= \
	-e "s,@GLU_REQUIRES@,gl,"

.include <bsd.x11.mk>
.include <bsd.lib.mk>

# XXX
COPTS.arcsorter.cc+=	-Wno-error
COPTS.sorter.cc+=	-Wno-error
COPTS.tobezier.cc+=	-Wno-error

.if ${MACHINE_ARCH} == "m68000" || ${MACHINE_ARCH} == "m68k"
COPTS.sampleCompTop.cc+=	-O0
.endif

CWARNFLAGS.clang+=	-Wno-parentheses -Wno-tautological-compare
