#	$NetBSD: Makefile,v 1.299 2012/08/17 16:22:27 joerg Exp $

#
# This is the top-level makefile for building NetBSD. For an outline of
# how to build a snapshot or release, as well as other release engineering
# information, see http://www.NetBSD.org/developers/releng/index.html
#
# Not everything you can set or do is documented in this makefile. In
# particular, you should review the files in /usr/share/mk (especially
# bsd.README) for general information on building programs and writing
# Makefiles within this structure, and see the comments in src/etc/Makefile
# for further information on installation and release set options.
#
# Variables listed below can be set on the make command line (highest
# priority), in /etc/mk.conf (middle priority), or in the environment
# (lowest priority).
#
# Variables:
#   DESTDIR is the target directory for installation of the compiled
#	software. It defaults to /. Note that programs are built against
#	libraries installed in DESTDIR.
#   MKMAN, if `no', will prevent building of manual pages.
#   MKOBJDIRS, if not `no', will build object directories at
#	an appropriate point in a build.
#   MKSHARE, if `no', will prevent building and installing
#	anything in /usr/share.
#   MKUPDATE, if not `no', will avoid a `make cleandir' at the start of
#	`make build', as well as having the effects listed in
#	/usr/share/mk/bsd.README.
#   NOCLEANDIR, if defined, will avoid a `make cleandir' at the start
#	of the `make build'.
#   NOINCLUDES will avoid the `make includes' usually done by `make build'.
#
#   See mk.conf(5) for more details.
#
#
# Targets:
#   build:
#	Builds a full release of NetBSD in DESTDIR, except for the
#	/etc configuration files.
#	If BUILD_DONE is set, this is an empty target.
#   distribution:
#	Builds a full release of NetBSD in DESTDIR, including the /etc
#	configuration files.
#   buildworld:
#	As per `make distribution', except that it ensures that DESTDIR
#	is not the root directory.
#   installworld:
#	Install the distribution from DESTDIR to INSTALLWORLDDIR (which
#	defaults to the root directory).  Ensures that INSTALLWORLDDIR
#	is not the root directory if cross compiling.
#   release:
#	Does a `make distribution', and then tars up the DESTDIR files
#	into ${RELEASEDIR}/${RELEASEMACHINEDIR}, in release(7) format.
#	(See etc/Makefile for more information on this.)
#   regression-tests:
#	Runs the regression tests in "regress" on this host.
#   sets:
#	Populate ${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/sets
#	from ${DESTDIR}
#   sourcesets:
#	Populate ${RELEASEDIR}/source/sets from ${NETBSDSRCDIR}
#   syspkgs:
#	Populate ${RELEASEDIR}/${RELEASEMACHINEDIR}/binary/syspkgs
#	from ${DESTDIR}
#   iso-image:
#	Create CD-ROM image in RELEASEDIR/iso.
#	RELEASEDIR must already have been populated by `make release'
#	or equivalent.
#   iso-image-source:
#	Create CD-ROM image with source in RELEASEDIR/iso.
#	RELEASEDIR must already have been populated by
#	`make release sourcesets' or equivalent.
#   live-image:
#	Create bootable live image for emulators or USB stick etc.
#	in RELEASEDIR/liveimage.
#	RELEASEDIR must already have been populated by `make release'
#	or equivalent.
#   install-image:
#	Create bootable installation image for USB stick etc.
#	in RELEASEDIR/installimage.
#	RELEASEDIR must already have been populated by `make release'
#	or equivalent.
#
# Targets invoked by `make build,' in order:
#   cleandir:        cleans the tree.
#   do-top-obj:      creates the top level object directory.
#   do-tools-obj:    creates object directories for the host toolchain.
#   do-tools:        builds host toolchain.
#   obj:             creates object directories.
#   do-distrib-dirs: creates the distribution directories.
#   includes:        installs include files.
#   do-tools-compat: builds the "libnbcompat" library; needed for some
#                    random host tool programs in the source tree.
#   do-lib:          builds and installs prerequisites from lib
#                    if ${MKCOMPAT} != "no".
#   do-compat-lib:   builds and installs prerequisites from compat/lib
#                    if ${MKCOMPAT} != "no".
#   do-build:        builds and installs the entire system.
#   do-x11:          builds and installs X11 if ${MKX11} != "no"; either
#                    X11R7 from src/external/mit/xorg if ${X11FLAVOUR} == "Xorg"
#                    or X11R6 from src/x11
#   do-extsrc:       builds and installs extsrc if ${MKEXTSRC} != "no".
#   do-obsolete:     installs the obsolete sets (for the postinstall-* targets).
#

.if ${.MAKEFLAGS:M${.CURDIR}/share/mk} == ""
.MAKEFLAGS: -m ${.CURDIR}/share/mk
.endif

#
# If _SRC_TOP_OBJ_ gets set here, we will end up with a directory that may
# not be the top level objdir, because "make obj" can happen in the *middle*
# of "make build" (long after <bsd.own.mk> is calculated it).  So, pre-set
# _SRC_TOP_OBJ_ here so it will not be added to ${.MAKEOVERRIDES}.
#
_SRC_TOP_OBJ_=

.include <bsd.own.mk>

#
# Sanity check: make sure that "make build" is not invoked simultaneously
# with a standard recursive target.
#

.if make(build) || make(release) || make(snapshot)
.for targ in ${TARGETS:Nobj:Ncleandir}
.if make(${targ}) && !target(.BEGIN)
.BEGIN:
	@echo 'BUILD ABORTED: "make build" and "make ${targ}" are mutually exclusive.'
	@false
.endif
.endfor
.endif

#
# _SUBDIR is used to set SUBDIR, after removing directories that have
# BUILD_${dir}=no, or that have no ${dir}/Makefile.
#
_SUBDIR=	tools lib .WAIT include gnu external crypto/external bin games
_SUBDIR+=	libexec sbin usr.bin
_SUBDIR+=	commands man
_SUBDIR+=	common drivers kernel servers
_SUBDIR+=	usr.sbin share rescue sys etc test tests compat .WAIT distrib regress

.if defined(__MINIX)
_SUBDIR+=	benchmarks
.endif

.for dir in ${_SUBDIR}
.if "${dir}" == ".WAIT" \
	|| (${BUILD_${dir}:Uyes} != "no" && exists(${dir}/Makefile))
SUBDIR+=	${dir}
.endif
.endfor

.if exists(regress)
regression-tests: .PHONY .MAKE
	@echo Running regression tests...
	${MAKEDIRTARGET} regress regress
.endif

.if ${MKUNPRIVED} != "no"
NOPOSTINSTALL=	# defined
.endif

afterinstall: .PHONY .MAKE
.if ${MKMAN} != "no"
.if !defined(__MINIX)
	${MAKEDIRTARGET} share/man makedb
.else
	${MAKEDIRTARGET} man makedb
.endif # !defined(__MINIX)
.endif
.if (${MKUNPRIVED} != "no" && ${MKINFO} != "no")
	${MAKEDIRTARGET} gnu/usr.bin/texinfo/install-info infodir-meta
.endif
.if !defined(NOPOSTINSTALL)
	${MAKEDIRTARGET} . postinstall-check
.endif

_POSTINSTALL=	${.CURDIR}/usr.sbin/postinstall/postinstall
_POSTINSTALL_ENV= \
	AWK=${TOOL_AWK:Q}		\
	DB=${TOOL_DB:Q}			\
	HOST_SH=${HOST_SH:Q}		\
	MAKE=${MAKE:Q}			\
	PWD_MKDB=${TOOL_PWD_MKDB:Q}	\
	STAT=${TOOL_STAT:Q}

postinstall-check: .PHONY
	@echo "   === Post installation checks ==="
	${_POSTINSTALL_ENV} ${HOST_SH} ${_POSTINSTALL} -s ${.CURDIR} -d ${DESTDIR}/ check; if [ $$? -gt 1 ]; then exit 1; fi
	@echo "   ================================"

postinstall-fix: .NOTMAIN .PHONY
	@echo "   === Post installation fixes ==="
	${_POSTINSTALL_ENV} ${HOST_SH} ${_POSTINSTALL} -s ${.CURDIR} -d ${DESTDIR}/ fix
	@echo "   ==============================="

postinstall-fix-obsolete: .NOTMAIN .PHONY
	@echo "   === Removing obsolete files ==="
	${_POSTINSTALL_ENV} ${HOST_SH} ${_POSTINSTALL} -s ${.CURDIR} -d ${DESTDIR}/ fix obsolete
	@echo "   ==============================="


#
# Targets (in order!) called by "make build".
#
.if defined(HAVE_GCC)
.if ${HAVE_GCC} == "4"
LIBGCC_EXT=4
BUILD_CC_LIB_BASEDIR= gnu/lib
BUILD_CC_LIB_BASETARGET= gnu-lib
.else
LIBGCC_EXT=
BUILD_CC_LIB_BASEDIR= external/gpl3/gcc/lib
BUILD_CC_LIB_BASETARGET= external-gpl3-gcc-lib
.endif
.endif

BUILDTARGETS+=	check-tools
.if ${MKUPDATE} == "no" && !defined(NOCLEANDIR)
BUILDTARGETS+=	cleandir
.endif
.if ${MKOBJDIRS} != "no"
BUILDTARGETS+=	do-top-obj
.endif
.if ${USETOOLS} == "yes"	# {
.if ${MKOBJDIRS} != "no"
BUILDTARGETS+=	do-tools-obj
.endif
BUILDTARGETS+=	do-tools
.endif # USETOOLS		# }
.if ${MKOBJDIRS} != "no"
BUILDTARGETS+=	obj
.endif
BUILDTARGETS+=	clean_METALOG
.if !defined(NODISTRIBDIRS)
BUILDTARGETS+=	do-distrib-dirs
.endif
.if !defined(NOINCLUDES)
BUILDTARGETS+=	includes
.endif
BUILDTARGETS+=	do-lib
.if !defined(__MINIX)
# LSC Not used in MINIX3
BUILDTARGETS+=	do-compat-lib
.endif # !defined(__MINIX)
BUILDTARGETS+=	do-build
.if ${MKX11} != "no"
BUILDTARGETS+=	do-x11
.endif
.if ${MKEXTSRC} != "no"
BUILDTARGETS+=	do-extsrc
.endif
BUILDTARGETS+=	do-obsolete

#
# Enforce proper ordering of some rules.
#

.ORDER:		${BUILDTARGETS}
includes-lib:	.PHONY includes-include includes-sys
includes-gnu:	.PHONY includes-lib

#
# Build the system and install into DESTDIR.
#

START_TIME!=	date

.if defined(__MINIX)
world: build .PHONY .MAKE
	${MAKEDIRTARGET} . etcfiles
	@echo "WARNING: "
	@echo "WARNING: The 'world' target is obsolete, please use 'build' instead."
	@echo "WARNING: "

etcfiles: .PHONY .MAKE
	${MAKEDIRTARGET} etc install-etc-files-safe  DESTDIR=${DESTDIR:U/}

etcforce: .PHONY .MAKE
	${MAKEDIRTARGET} etc install-etc-files  DESTDIR=${DESTDIR:U/}

.endif # defined(__MINIX)

build: .PHONY .MAKE
.if defined(BUILD_DONE)
	@echo "Build already installed into ${DESTDIR}"
.else
	@echo "Build started at: ${START_TIME}"
.for tgt in ${BUILDTARGETS}
	${MAKEDIRTARGET} . ${tgt}
.endfor
	${MAKEDIRTARGET} etc install-etc-release
.if defined(__MINIX)
	${MAKEDIRTARGET} etc install-etc-files-safe DESTDIR=${DESTDIR:U/}
	${MAKEDIRTARGET} releasetools do-hdboot
.endif # defined(__MINIX)
	@echo   "Build started at:  ${START_TIME}"
	@printf "Build finished at: " && date
.endif

#
# Build a full distribution, but not a release (i.e. no sets into
# ${RELEASEDIR}).  "buildworld" enforces a build to ${DESTDIR} != /
#

distribution buildworld: .PHONY .MAKE
.if make(buildworld) && \
    (!defined(DESTDIR) || ${DESTDIR} == "" || ${DESTDIR} == "/")
	@echo "Won't make ${.TARGET} with DESTDIR=/"
	@false
.endif
	${MAKEDIRTARGET} . build NOPOSTINSTALL=1
	${MAKEDIRTARGET} etc distribution INSTALL_DONE=1
.if defined(DESTDIR) && ${DESTDIR} != "" && ${DESTDIR} != "/"
	${MAKEDIRTARGET} . postinstall-fix-obsolete
	${MAKEDIRTARGET} distrib/sets checkflist
.endif
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

#
# Install the distribution from $DESTDIR to $INSTALLWORLDDIR (defaults to `/')
# If installing to /, ensures that the host's operating system is NetBSD and
# the host's `uname -m` == ${MACHINE}.
#

HOST_UNAME_S!=	uname -s
.if ${HOST_OSTYPE:C/\-.*//} != "Minix"
HOST_UNAME_M!=	uname -m
.else
HOST_UNAME_M:= ${MACHINE}
.endif

installworld: .PHONY .MAKE
.if (!defined(DESTDIR) || ${DESTDIR} == "" || ${DESTDIR} == "/")
	@echo "Can't make ${.TARGET} to DESTDIR=/"
	@false
.endif
.if !defined(INSTALLWORLDDIR) || \
    ${INSTALLWORLDDIR} == "" || ${INSTALLWORLDDIR} == "/"
.if (${HOST_UNAME_S} != "Minix")
	@echo "Won't cross-make ${.TARGET} from ${HOST_UNAME_S} to Minix with INSTALLWORLDDIR=/"
	@false
.endif
.if (${HOST_UNAME_M} != ${MACHINE})
	@echo "Won't cross-make ${.TARGET} from ${HOST_UNAME_M} to ${MACHINE} with INSTALLWORLDDIR=/"
	@false
.endif
.endif
	${MAKEDIRTARGET} distrib/sets installsets \
		INSTALLDIR=${INSTALLWORLDDIR:U/} INSTALLSETS=${INSTALLSETS:Q}
	${MAKEDIRTARGET} . postinstall-check DESTDIR=${INSTALLWORLDDIR}
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

#
# Install modules from $DESTDIR to $INSTALLMODULESDIR
#
installmodules: .PHONY .MAKE
.if (!defined(DESTDIR) || ${DESTDIR} == "" || ${DESTDIR} == "/")
	@echo "Can't make ${.TARGET} to DESTDIR=/"
	@false
.endif
.if !defined(INSTALLMODULESDIR) || \
    ${INSTALLMODULESDIR} == "" || ${INSTALLMODULESDIR} == "/"
.if (${HOST_UNAME_S} != "NetBSD")
	@echo "Won't cross-make ${.TARGET} from ${HOST_UNAME_S} to NetBSD with INSTALLMODULESDIR=/"
	@false
.endif
.if (${HOST_UNAME_M} != ${MACHINE})
	@echo "Won't cross-make ${.TARGET} from ${HOST_UNAME_M} to ${MACHINE} with INSTALLMODULESDIR=/"
	@false
.endif
.endif
	${MAKEDIRTARGET} sys/modules install DESTDIR=${INSTALLMODULESDIR:U/}
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

#
# Create sets from $DESTDIR or $NETBSDSRCDIR into $RELEASEDIR
#

.for tgt in sets sourcesets syspkgs
${tgt}: .PHONY .MAKE
	${MAKEDIRTARGET} distrib/sets ${tgt}
.endfor

#
# Build a release or snapshot (implies "make distribution").  Note that
# in this case, the set lists will be checked before the tar files
# are made.
#

release snapshot: .PHONY .MAKE
	${MAKEDIRTARGET} . distribution
	${MAKEDIRTARGET} etc release DISTRIBUTION_DONE=1
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

#
# Create a CD-ROM image.
#

iso-image: .PHONY
	${MAKEDIRTARGET} distrib iso_image
	${MAKEDIRTARGET} etc iso-image
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

iso-image-source: .PHONY
	${MAKEDIRTARGET} distrib iso_image CDSOURCE=true
	${MAKEDIRTARGET} etc iso-image
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

#
# Create bootable live images.
#

live-image: .PHONY
	${MAKEDIRTARGET} etc live-image
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

#
# Create bootable installation images.
#

install-image: .PHONY
	${MAKEDIRTARGET} etc install-image
	@echo   "make ${.TARGET} started at:  ${START_TIME}"
	@printf "make ${.TARGET} finished at: " && date

#
# Special components of the "make build" process.
#

check-tools: .PHONY
.if ${TOOLCHAIN_MISSING} != "no" && !defined(EXTERNAL_TOOLCHAIN)
	@echo '*** WARNING:  Building on MACHINE=${MACHINE} with missing toolchain.'
	@echo '*** May result in a failed build or corrupt binaries!'
.elif defined(EXTERNAL_TOOLCHAIN)
	@echo '*** Using external toolchain rooted at ${EXTERNAL_TOOLCHAIN}.'
.endif
.if defined(NBUILDJOBS)
	@echo '*** WARNING: NBUILDJOBS is obsolete; use -j directly instead!'
.endif

# Delete or sanitise a leftover METALOG from a previous build.
clean_METALOG: .PHONY .MAKE
.if ${MKUPDATE} != "no"
	${MAKEDIRTARGET} distrib/sets clean_METALOG
.endif

do-distrib-dirs: .PHONY .MAKE
.if !defined(DESTDIR) || ${DESTDIR} == ""
	${MAKEDIRTARGET} etc distrib-dirs DESTDIR=/
.else
	${MAKEDIRTARGET} etc distrib-dirs DESTDIR=${DESTDIR}
.endif

.for targ in cleandir obj includes
do-${targ}: .PHONY ${targ}
	@true
.endfor

.for dir in tools tools/compat
do-${dir:S/\//-/g}: .PHONY .MAKE
.for targ in dependall install
	${MAKEDIRTARGET} ${dir} ${targ}
.endfor
.endfor

do-lib: .PHONY .MAKE
	${MAKEDIRTARGET} lib build_install

do-compat-lib: .PHONY .MAKE
	${MAKEDIRTARGET} compat build_install BOOTSTRAP_SUBDIRS="../../../lib"

do-top-obj: .PHONY .MAKE
	${MAKEDIRTARGET} . obj NOSUBDIR=

do-tools-obj: .PHONY .MAKE
	${MAKEDIRTARGET} tools obj

do-build: .PHONY .MAKE
.for targ in dependall install
	${MAKEDIRTARGET} . ${targ} BUILD_tools=no BUILD_lib=no
.endfor

do-x11: .PHONY .MAKE
.if ${MKX11} != "no"
.if ${X11FLAVOUR} == "Xorg"
	${MAKEDIRTARGET} external/mit/xorg build
.else
	${MAKEDIRTARGET} x11 build
.endif
.else
	@echo "MKX11 is not enabled"
	@false
.endif

do-extsrc: .PHONY .MAKE
.if ${MKEXTSRC} != "no"
	${MAKEDIRTARGET} extsrc build
.else
	@echo "MKEXTSRC is not enabled"
	@false
.endif

do-obsolete: .PHONY .MAKE
	${MAKEDIRTARGET} etc install-obsolete-lists

#
# Speedup stubs for some subtrees that don't need to run these rules.
# (Tells <bsd.subdir.mk> not to recurse for them.)
#

.for dir in bin etc distrib games libexec regress sbin usr.bin usr.sbin tools
includes-${dir}: .PHONY
	@true
.endfor
.for dir in etc distrib regress
install-${dir}: .PHONY
	@true
.endfor

#
# XXX this needs to change when distrib Makefiles are recursion compliant
# XXX many distrib subdirs need "cd etc && make snap_pre snap_kern" first...
#
dependall-distrib depend-distrib all-distrib: .PHONY
	@true

.include <bsd.sys.mk>
.include <bsd.obj.mk>
.include <bsd.kernobj.mk>
.include <bsd.subdir.mk>

#
# Display current make(1) parameters
#
params: .PHONY .MAKE
	${MAKEDIRTARGET} etc params
