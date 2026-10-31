#	$NetBSD: Makefile.inc,v 1.5 2018/05/27 01:14:50 christos Exp $
#
#	Configuration variables (default values are below):
#
#	S	must be set to the top of the 'sys' tree.
#	GNUEFIDST	may be set to the location of the directory where library
#		objects are to be built.  Defaults to ${.OBJDIR}/lib/gnuefi.
#	GNUEFI_AS	may be set to 'obj' to build a object from the library's
#		object files.  (Otherwise, a library will be built.)
#		Defaults to 'library'.
#	GNUEFIMISCCPPFLAGS
#		Miscellaneous cpp flags to be passed to the library's Makefile
#		when building.
#	GNUEFIMISCMAKEFLAGS
#		Miscellaneous flags to be passed to the library's Makefile when
#		building.  See library's Makefile for more details about
#		supported flags and their default values.

# Default values:
GNUEFIDST?=		${.OBJDIR}/lib/gnuefi
GNUEFI_AS?=		library
GNUEFIDOTDIR?= ../../.

CWARNFLAGS.clang+=	-Wno-format-extra-args

GNUEFIDIR=		${S:S@^.@${GNUEFIDOTDIR}@:q}/lib/libgnuefi
.if (${GNUEFI_AS} == "obj")
GNUEFILIB=		${GNUEFIDST}/libgnuefi.o
GNUEFILIB_PROF=	${GNUEFIDST}/libgnuefi.po
.else
GNUEFILIB=		${GNUEFIDST}/libgnuefi.a
GNUEFILIB_PROF=	${GNUEFIDST}/libgnuefi_p.a
.endif
ZLIBSRCDIR:=	${.PARSEDIR}/../../../common/dist/zlib
GNUEFIMISCCPPFLAGS+=-I${ZLIBSRCDIR}

GNUEFIMAKE= \
	cd ${GNUEFIDST} && ${MAKE} -f ${GNUEFIDIR:q}/Makefile \
	    GNUEFIDIR=${GNUEFIDIR:q} \
	    CC=${CC:q} CFLAGS=${CFLAGS:q} CPUFLAGS= \
	    AS=${AS:q} AFLAGS=${AFLAGS:q} \
	    LORDER=${LORDER:q} \
	    TSORT=${TSORT:q} \
	    LD=${LD:q} STRIP=${STRIP:q} \
	    AR=${AR:q} NM=${NM:q} \
	    RANLIB=${RANLIB:q} SIZE=${SIZE:q} \
	    MACHINE=${MACHINE} MACHINE_ARCH=${MACHINE_ARCH:q} \
	    GNUEFICPPFLAGS=${CPPFLAGS:S@^-I.@-I${GNUEFIDOTDIR}@g:q} \
	    GNUEFIMISCCPPFLAGS=${GNUEFIMISCCPPFLAGS:q} \
	    LIBGNUEFI_ARCH=${LIBGNUEFI_ARCH:q} \
	    ${GNUEFIMISCMAKEFLAGS}

${GNUEFILIB}:		.NOTMAIN .MAKE __always_make_gnuefilib
	@echo making sure the gnuefi library is up to date...
.if (${GNUEFI_AS} == "library")
	@${GNUEFIMAKE} libgnuefi.a
.else
	@${GNUEFIMAKE} libgnuefi.o
.endif

${GNUEFILIB_PROF}:		.NOTMAIN .MAKE __always_make_gnuefilib
	@echo making sure the profiled gnuefi library is up to date...
.if (${GNUEFI_AS} == "library")
	@${GNUEFIMAKE} libgnuefi_p.a
.else
	@${GNUEFIMAKE} libgnuefi.po
.endif

clean:			.NOTMAIN cleangnuefilib
cleangnuefilib:		.NOTMAIN
	@echo cleaning the gnuefi library objects
	@if [ -d "${GNUEFIDST}" ]; then ${GNUEFIMAKE} clean; fi

cleandir distclean:	.NOTMAIN cleandirgnuefilib
cleandirgnuefilib:		.NOTMAIN
	@echo cleandiring the gnuefi library objects
	@if [ -d "${GNUEFIDST}" ]; then ${GNUEFIMAKE} cleandir; fi

dependall depend:	.NOTMAIN dependgnuefilib
dependgnuefilib:		.NOTMAIN .MAKE __always_make_gnuefilib
	@echo depending the gnuefi library objects
	@${GNUEFIMAKE} depend

__always_make_gnuefilib:	.NOTMAIN
	@mkdir -p ${GNUEFIDST}

.PHONY: __always_make_gnuefilib
.PHONY: cleangnuefilib cleandirgnuefilib dependgnuefilib
