#	$NetBSD: Makefile.inc,v 1.22 2015/09/06 15:34:55 uebayasi Exp $
#
#	Configuration variables (default values are below):
#
#	S	must be set to the top of the 'sys' tree.
#	SADST	may be set to the location of the directory where library
#		objects are to be built.  Defaults to ${.OBJDIR}/lib/sa.
#	SA_AS	may be set to 'obj' to build a object from the library's
#		object files.  (Otherwise, a library will be built.)
#		Defaults to 'library'.
#	SAMISCCPPFLAGS
#		Miscellaneous cpp flags to be passed to the library's Makefile
#		when building.
#	SAMISCMAKEFLAGS
#		Miscellaneous flags to be passed to the library's Makefile when
#		building.  See library's Makefile for more details about
#		supported flags and their default values.

# Default values:
SADST?=		${.OBJDIR}/lib/sa
SA_AS?=		library
SADOTDIR?= ../../.

CWARNFLAGS.clang+=	-Wno-format-extra-args

SADIR=		${S:S@^.@${SADOTDIR}@:Q}/lib/libsa
.if (${SA_AS} == "obj")
SALIB=		${SADST}/libsa.o
SALIB_PROF=	${SADST}/libsa.po
.else
SALIB=		${SADST}/libsa.a
SALIB_PROF=	${SADST}/libsa_p.a
.endif
ZLIBSRCDIR:=	${.PARSEDIR}/../../../common/dist/zlib
SAMISCCPPFLAGS+=-I${ZLIBSRCDIR}

SAMAKE= \
	cd ${SADST} && ${MAKE} -f ${SADIR:Q}/Makefile \
	    SADIR=${SADIR:Q} \
	    CC=${CC:Q} CFLAGS=${CFLAGS:Q} CPUFLAGS= \
	    AS=${AS:Q} AFLAGS=${AFLAGS:Q} \
	    LORDER=${LORDER:Q} \
	    TSORT=${TSORT:Q} \
	    LD=${LD:Q} STRIP=${STRIP:Q} \
	    AR=${AR:Q} NM=${NM:Q} \
	    RANLIB=${RANLIB:Q} SIZE=${SIZE:Q} \
	    MACHINE=${MACHINE} MACHINE_ARCH=${MACHINE_ARCH:Q} \
	    SACPPFLAGS=${CPPFLAGS:S@^-I.@-I${SADOTDIR}@g:Q} \
	    SAMISCCPPFLAGS=${SAMISCCPPFLAGS:Q} \
	    ${SAMISCMAKEFLAGS}

${SALIB}:		.NOTMAIN .MAKE __always_make_salib
	@echo making sure the sa library is up to date...
.if (${SA_AS} == "library")
	@${SAMAKE} libsa.a
.else
	@${SAMAKE} libsa.o
.endif
	@echo done

${SALIB_PROF}:		.NOTMAIN .MAKE __always_make_salib
	@echo making sure the profiled sa library is up to date...
.if (${SA_AS} == "library")
	@${SAMAKE} libsa_p.a
.else
	@${SAMAKE} libsa.po
.endif
	@echo done

clean:			.NOTMAIN cleansalib
cleansalib:		.NOTMAIN
	@echo cleaning the sa library objects
	@if [ -d "${SADST}" ]; then ${SAMAKE} clean; fi
	@echo done

cleandir distclean:	.NOTMAIN cleandirsalib
cleandirsalib:		.NOTMAIN
	@echo cleandiring the sa library objects
	@if [ -d "${SADST}" ]; then ${SAMAKE} cleandir; fi
	@echo done

dependall depend:	.NOTMAIN dependsalib
dependsalib:		.NOTMAIN .MAKE __always_make_salib
	@echo depending the sa library objects
	@${SAMAKE} depend
	@echo done

__always_make_salib:	.NOTMAIN
	@mkdir -p ${SADST}

.PHONY: __always_make_salib
.PHONY: cleansalib cleandirsalib dependsalib
