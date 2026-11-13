#	$NetBSD: Makefile.inc,v 1.19 2018/06/13 16:03:10 christos Exp $
#
#	Configuration variables (default values are below):
#
#	S	must be set to the top of the 'sys' tree.
#	I386DST	may be set to the location of the directory where library
#		objects are to be built.  Defaults to ${.OBJDIR}/lib/i386.
#	I386MISCCPPFLAGS
#		Miscellaneous cpp flags to be passed to the library's Makefile
#		when building.
#	I386MISCMAKEFLAGS
#		Miscellaneous flags to be passed to the library's Makefile when
#		building.  See library's Makefile for more details about
#		supported flags and their default values.

.if defined(__MINIX)
USE_BITCODE=no
.endif # defined(__MINIX)

# Default values:
I386DST?=		${.OBJDIR}/lib/i386

#I386DIR=		$S/arch/i386/stand/lib
I386LIB=		${I386DST}/libi386.a

CWARNFLAGS.clang+=	-Wno-tautological-compare

I386MAKE= \
	cd ${I386DIR} && MAKEOBJDIRPREFIX= && unset MAKEOBJDIRPREFIX && \
	    MAKEOBJDIR=${I386DST} ${MAKE} \
	    CC=${CC:q} CFLAGS=${CFLAGS:q} \
	    AS=${AS:q} AFLAGS=${AFLAGS:q} \
	    LD=${LD:q} STRIP=${STRIP:q} \
	    MACHINE=${MACHINE} MACHINE_ARCH=${MACHINE_ARCH:q} \
	    I386CPPFLAGS=${CPPFLAGS:S@^-I.@-I../../.@g:q} \
	    I386MISCCPPFLAGS=${I386MISCCPPFLAGS:q} \
	    ${I386MISCMAKEFLAGS}

${I386LIB}:		.NOTMAIN __always_make_i386lib
	@echo making sure the i386 library is up to date...
	@${I386MAKE} libi386.a
	@echo done

clean:			.NOTMAIN cleani386lib
cleani386lib:		.NOTMAIN
	@echo cleaning the i386 library objects
	@if [ -d "${I386DST}" ]; then ${I386MAKE} clean; fi
	@echo done

cleandir distclean:	.NOTMAIN cleandiri386lib
cleandiri386lib:	.NOTMAIN
	@echo cleandiring the i386 library objects
	@if [ -d "${I386DST}" ]; then ${I386MAKE} cleandir; fi
	@echo done

dependall depend:	.NOTMAIN dependi386lib
dependi386lib:		.NOTMAIN __always_make_i386lib
	@echo depending the i386 library objects
	@${I386MAKE} depend
	@echo done

__always_make_i386lib:	.NOTMAIN
	@mkdir -p ${I386DST}
