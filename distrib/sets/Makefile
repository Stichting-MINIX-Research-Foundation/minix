#	$NetBSD: Makefile,v 1.98 2014/08/22 10:51:18 apb Exp $

# Experimental RCS METALOG versioning
# (Needs host's rcs(1) commands)
#
# TODO:
# - In 'build.sh distribution', print diff to previous
#RCSMETALOG=1

# The `all' target must appear before bsd.own.mk is pulled in.
all:
	@echo "Please understand what you are doing, first."
	@false

.include <bsd.own.mk>
.include "${NETBSDSRCDIR}/distrib/common/Makefile.distrib"

COMPRESS_PROGRAM=${TOOL_GZIP}
GZIP_FLAGS= ${GZIP_N_FLAG}

SETSENV=	DESTDIR=${DESTDIR:Q} \
		MACHINE=${MACHINE:Q} \
		MACHINE_ARCH=${MACHINE_ARCH:Q} \
		AWK=${TOOL_AWK:Q} \
		CKSUM=${TOOL_CKSUM:Q} \
		DB=${TOOL_DB:Q} \
		HOST_SH=${HOST_SH:Q} \
		MAKE=${MAKE:Q} \
		MKTEMP=${TOOL_MKTEMP:Q} \
		MTREE=${TOOL_MTREE:Q} \
		PAX=${TOOL_PAX:Q} \
		COMPRESS_PROGRAM=${COMPRESS_PROGRAM:Q} \
		GZIP=${GZIP_FLAGS:Q} \
		PKG_CREATE=${TOOL_PKG_CREATE:Q} \
		SED=${TOOL_SED:Q} \
		TSORT=${TSORT:Q}
SETSCMD=	cd ${.CURDIR} && \
		${SETSENV} \
		${HOST_SH}

MAKETARS_FLAGS=
.if ${MKX11} != "no"
MAKESRCTARS_X11_FLAGS=	-x ${X11SRCDIR}
.endif
.if ${MKEXTSRC} != "no"
MAKESRCTARS_EXTSRC_FLAGS=	-y ${EXTSRCSRCDIR}
.endif
.if ${MKX11} != "no"
MAKEFLIST_X11_FLAGS=	,x
.endif
.if ${MKEXTSRC} != "no"
MAKEFLIST_EXTSRC_FLAGS=	,ext
.endif
MAKEFLIST_FLAGS=	-L base${MAKEFLIST_X11_FLAGS}${MAKEFLIST_EXTSRC_FLAGS}
MAKESRCTARS_FLAGS= 	-N ${NETBSDSRCDIR}/etc

.if ${MAKEVERBOSE} < 2
MAKETARS_FLAGS+=	-q
MAKESRCTARS_FLAGS+= 	-q
.endif

.if !defined(MAKETARSETS)
MAKETARSETS!= ${SETSCMD} ./makeflist -l ${MAKEFLIST_FLAGS}
.endif

print_have_gcc: .PHONY
	@echo ${HAVE_GCC}

print_machine: .PHONY
	@echo ${MACHINE}

print_machine_arch: .PHONY
	@echo ${MACHINE_ARCH}

print_machine_cpu: .PHONY
	@echo ${MACHINE_CPU}

print_object_fmt: .PHONY
	@echo ${OBJECT_FMT}

print_toolchain_missing: .PHONY
	@echo "${TOOLCHAIN_MISSING}"

print_mkvars \
print_set_lists_base print_set_lists_x print_set_lists_ext \
list_set_lists_base list_set_lists_x list_set_lists_ext \
list_set_files_base list_set_files_x list_set_files_ext \
		: .PHONY
	@${SETSENV}; rundir="${.CURDIR}"; . ./sets.subr; ${.TARGET}

#
# METALOG MANIPULATION TARGETS
#
# METALOG is the name of a metadata log file, and is set in <bsd.own.mk>
# if MKUNPRIVED is not "no".
#
# METALOG.unpriv is a command line option passed to various scripts;
# it is either blank or "-M ${METALOG}.sanitised", depending on the
# MKUNPRIVED flag.
#
# The sanitise_METALOG target creates METALOG.sanitised from METALOG,
# without modifying METALOG itself.  This is used near the end of
# a build, after build products have been installed in DESTDIR
# and corresponding lines have been added to METALOG, but before
# METALOG.sanitised is used in the creation of sets in RELEASEDIR.
# For update builds, METALOG.sanitised has duplicates merged (keeping
# only the last of multiple entries for the same item), but for clean
# builds it's an error if a file or directory name appears twice.
# METALOG.sanitised is always sorted and has timestamps removed.
#
# The clean_METALOG target either deletes METALOG, or replaces METALOG with
# a sanitised version of itself, depending on the MKUPDATE flag, and
# deletes old METALOG.* files.  This is intended to be used at the start
# of a build, to ensure that repeated MKUPDATE builds do not cause
# unbounded growth of METALOG.
#

METALOG_REMOVE_DUPLICATES= \
	${TOOL_AWK} '{ a[$$1] = $$0; } END { for (f in a) print a[f]; }'

.if ${MKUNPRIVED} == "no"
METALOG.unpriv=
sanitise_METALOG: .PHONY
	@true
clean_METALOG: .PHONY
	@true
.else	# MKUNPRIVED
METALOG.unpriv= -M ${METALOG}.sanitised
sanitise_METALOG: .PHONY ${METALOG}.sanitised
${METALOG}.sanitised: ${METALOG}
	<${METALOG} \
	    ${${MKUPDATE} != "no" :? ${METALOG_REMOVE_DUPLICATES} | :} \
	    sort | ${TOOL_MTREE} -CSM -k all -R time -N ${NETBSDSRCDIR}/etc \
	    >${METALOG}.new
	mv ${METALOG}.new ${METALOG}.sanitised
.if defined(RCSMETALOG)
	. ./metalog.subr; \
	xrcs_descr="build distribution METALOG"; \
	xrcs_msg="$$(date)"; \
	xrcs_cur=${METALOG}.sanitised; \
	xrcs update
.endif
.if ${MKUPDATE} == "no" || !exists(${METALOG})
clean_METALOG: .PHONY
	rm -f ${METALOG} ${METALOG}.*
.else	# MKUPDATE
clean_METALOG: .PHONY ${METALOG}.sanitised
	mv ${METALOG}.sanitised ${METALOG}
.if defined(RCSMETALOG)
	[ -f ${METALOG}.sanitised,v ] && mv ${METALOG}.sanitised,v ${METALOG},v
.endif
	rm -f ${METALOG}.*
.if defined(RCSMETALOG)
	[ -f ${METALOG},v ] && mv ${METALOG},v ${METALOG}.sanitised,v
.endif
.endif	# MKUPDATE
.endif	# MKUNPRIVED

#
# FILE LIST TARGETS
#

# This target has debugging value only, really.
makeflist: .PHONY .PRECIOUS check_DESTDIR
	${SETSCMD} ./makeflist ${MAKEFLIST_FLAGS}

checkflist: .PRECIOUS .PHONY check_DESTDIR sanitise_METALOG
	${SETSCMD} ${.CURDIR}/checkflist \
	    ${MAKEFLIST_FLAGS} ${CHECKFLIST_FLAGS} ${METALOG.unpriv}

checkflist-x11: .PHONY check_DESTDIR
	${SETSCMD} ./checkflist -x ${CHECKFLIST_FLAGS}

checkflist-extsrc: .PHONY check_DESTDIR
	${SETSCMD} ./checkflist -y ${CHECKFLIST_FLAGS}

.if defined(DESTDIR) && ${DESTDIR} != ""
checkflist_if_DESTDIR: checkflist
.else
checkflist_if_DESTDIR:
.endif

#
# SET BUILDING TARGETS
#

TARDIR=		${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/sets
SOURCETARDIR=	${RELEASEDIR}/source/sets

# If MAKETARS_SERIAL is set to "YES" or "yes", then the tar sets will be
# made in series, even if make's "-j" flag requests multiple parallel
# jobs.  This is useful on systems that exhibit poor performance when
# running multiple parallel disk-intensive operations.  The default is
# MAKETARS_SERIAL=NO, which will obey the "-j" flag passed to the make
# command.
#
MAKETARS_SERIAL?= NO
.if empty(MAKETARS_SERIAL:M[Yy][Ee][Ss])
_MAKETARS_WAIT= # empty
.else
_MAKETARS_WAIT= .WAIT
.endif

maketars: .PRECIOUS .PHONY check_DESTDIR check_RELEASEDIR \
		sanitise_METALOG checkflist_if_DESTDIR .WAIT \
		maketarsetup .WAIT \
		${MAKETARSETS:@.TARS.@${_MAKETARS_WAIT} do-${.TARS.}@}
	@true

maketarsetup: .EXEC
.if defined(DESTDIR) && ${DESTDIR} != ""
	${_MKMSG} "execute  checkflist"
	cd ${.CURDIR}; ${MAKE} checkflist
.endif
	mkdir -p ${TARDIR}
	for i in MD5 SHA512; do \
		rm -f ${TARDIR}/$$i ${TARDIR}/$$i.tmp; \
	done

makesetfiles: .PHONY sanitise_METALOG
	${_MKMSG_CREATE} "set lists"
	${SETSCMD} ./maketars -S -d ${DESTDIR:S,^$,/,} ${MAKETARS_FLAGS} \
	    ${METALOG.unpriv} \
	    -N ${NETBSDSRCDIR}/etc ${MAKEFLIST_FLAGS} -t ${TARDIR}

.for tar in ${MAKETARSETS}
do-${tar}: .PHONY sanitise_METALOG
	${_MKMSG_CREATE} "${tar}.tgz"
	${SETSCMD} ./maketars -d ${DESTDIR:S,^$,/,} ${MAKETARS_FLAGS} \
	    ${METALOG.unpriv} \
	    -N ${NETBSDSRCDIR}/etc -t ${TARDIR} ${tar} \
	|| { rm -f ${TARDIR}/${tar}.tgz; false; }
.endfor


makesrctars: .PRECIOUS .PHONY check_RELEASEDIR
	${_MKMSG_CREATE} "source tar files"
	mkdir -p ${SOURCETARDIR}
	${SETSCMD} ./makesrctars ${MAKESRCTARS_FLAGS} \
	    ${MAKESRCTARS_X11_FLAGS} ${MAKESRCTARS_EXTSRC_FLAGS} \
	    ${NETBSDSRCDIR} ${SOURCETARDIR}


makesums: .PRECIOUS .PHONY check_RELEASEDIR .WAIT \
		${MAKETARSETS:@.TARS.@do-sum-${.TARS.}@}
	for i in MD5 SHA512; do \
		mv ${TARDIR}/$$i.tmp ${TARDIR}/$$i; \
	done

.for tar in ${MAKETARSETS}
do-sum-${tar}: .PHONY do-${tar}
	${_MKMSG_CREATE} "${tar} checksums"
	${MAKESUMS} -t ${TARDIR} ${tar}.tgz
	for i in MD5 SHA512; do \
		${TOOL_CAT} ${TARDIR}/$$i >> ${TARDIR}/$$i.tmp; \
	done
.endfor
.ORDER: ${MAKETARSETS:@.TARS.@do-sum-${.TARS.}@}


installsets: .PHONY check_DESTDIR sanitise_METALOG
.if !defined(INSTALLDIR)
	@echo "setenv INSTALLDIR before doing that!"
	@false
.endif
	${SETSCMD} ./maketars -d ${DESTDIR:S,^$,/,} ${MAKETARS_FLAGS} \
	    ${METALOG.unpriv} -N ${NETBSDSRCDIR}/etc \
	    ${MAKEFLIST_FLAGS} -i ${INSTALLDIR} ${INSTALLSETS}

# Should we ignore errors like extra or missing files in the flists?
SLOPPY_FLIST?= NO
.if !empty(SLOPPY_FLIST:M[Yy][Ee][Ss])
CHECKFLIST_FLAGS+=	-e -m
REGPKG.sloppy=		-m
.endif

.if ${MAKEVERBOSE} == 0
REGPKG.verbose?= -q
.elif ${MAKEVERBOSE} == 1
REGPKG.verbose?=
.else	# MAKEVERBOSE >= 2
REGPKG.verbose?= -v
.endif
REGPKG.force?=		# -f, or empty
REGPKG.cache?= -c	# -c, or empty
REGPKG.update:= ${MKUPDATE:tl:Nno:C/..*/-u/}
SYSPKGSETS?= all
makesyspkgs: .PHONY check_DESTDIR check_RELEASEDIR \
		sanitise_METALOG checkflist_if_DESTDIR
	mkdir -p ${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/syspkgs
	${SETSCMD} ${.CURDIR}/regpkgset \
	    ${REGPKG.verbose} ${REGPKG.force} ${REGPKG.sloppy} \
	    ${REGPKG.cache} ${REGPKG.update} \
	    -d ${DESTDIR:S,^$,/,} ${METALOG.unpriv} \
	    -N ${NETBSDSRCDIR}/etc \
	    -t ${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/syspkgs ${SYSPKGSETS}

makesyspkgsums: .PHONY check_RELEASEDIR
	${MAKESUMS} -t ${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/syspkgs

# Update the "deps" file.
# XXX: Why is "deps" checked in to the source tree, instead of
# just being created as necessary?
makedeps: .PHONY
	${SETSCMD} ./syspkgdeps all >${.CURDIR}/deps

# Sort the lists files.
#
# The tricky stuff with awk and sort -k options is to ensure that the
# heading remains undisturbed, and on non-heading lines anything before
# the first "." is ignored, so that "./dir/file" and "#./dir/file" are
# sorted together.
#
sortlists: .PHONY
	find ${.CURDIR}/lists \! \( -name CVS -prune \) \! -name .#\* \
	    -type f -print \
	| while read f ; do \
	    ${_MKSHMSG} "sorting $${f#${.CURDIR}/}" ; \
	    awk 'BEGIN { inheader = 1 } \
		 /^#*\.\// { inheader = 0 } \
		 // { tag = (inheader ? NR : 999999); \
		      print tag "." $$0 }' \
		<"$$f" \
	    | sort -t . -k 1n,2 -k 3 \
	    | cut -d . -f 2- \
	    | cat -s >"$$f".tmp; \
	    if cmp "$$f" "$$f".tmp >/dev/null; then \
	      : "$$f is unchanged" ; \
	      rm "$$f".tmp ; \
	    else \
	      mv "$$f".tmp "$$f" ; \
	    fi ; \
	  done

#
# MAIN ENTRY POINTS
#

syspkgs: .PHONY makesyspkgs .WAIT makesyspkgsums
	@true

sets: .PHONY maketars .WAIT makesums
	@true

sourcesets: .PHONY makesrctars
	@true

.include <bsd.files.mk>
