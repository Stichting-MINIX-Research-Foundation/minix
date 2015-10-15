#	$NetBSD: Makefile,v 1.24 2015/02/01 17:47:00 chs Exp $

LIBISPRIVATE=	yes

LIB=		backend

SRCS=		${G_OBJS:.o=.c} ${G_out_file:T}

BOTH_CPPFLAGS+=	-I. -I${GCCARCH} ${G_ALL_CFLAGS:M-D*} ${G_INCLUDES:M-I*:N-I.*}
CPPFLAGS+=	${BOTH_CPPFLAGS} -DTARGET_NAME=\"${MACHINE_GNU_PLATFORM}\"
HOST_CPPFLAGS+=	${BOTH_CPPFLAGS} -DGENERATOR_FILE
DPSRCS+=	insn-attr.h insn-codes.h insn-config.h insn-flags.h tree-check.h
CPPFLAGS+=	-I${DIST}/libgcc
CPPFLAGS+=	-I${.CURDIR}/../../lib/libgcc/libgcov/arch/${MACHINE_ARCH}

CPPFLAGS.cppdefault.c+=	-DPREFIX=\"/usr\" \
			-DNATIVE_SYSTEM_HEADER_DIR=\"${G_NATIVE_SYSTEM_HEADER_DIR}\"

HOSTPROG_CXX=	1

.include <bsd.lib.mk>

# Force using C++ for this
ORIGHOST_CC:=	${HOST_CC}
HOST_CC:=	${HOST_CXX}
CC:=		${CXX}
CFLAGS:=	${CXXFLAGS}

# Check PREPROCESSOR_DEFINES for cppbuiltin.c and cppdefault.c
CPPFLAGS.cppbuiltin.c+=	-DBASEVER="\"${BASEVER}"\"

#
# Independent generation programs.
#
CLEANFILES+=	gengenrtl genrtl.h \
		gengtype gtyp-gen.h gtype-desc.c gtype-desc.h \
		gtype-c.h gtype-cp.h gtype-f.h gtype-objc.h \
		genflags genflags.h \
		gencheck tree-check.h \
		insn-preds.c genpreds tm-preds.h tm_p.h cs-tm_p.h \
		genmodes insn-modes.c insn-modes.h min-insn-modes.c \
		auto-host.h build-print-rtl.c \
		build-rtl.c build-errors.c build-varray.c \
		build-bitmap.c build-ggc-none.c \
		gt-*.h *.o *.lo \
		cs-config.h \
		gcov-iov gcov-iov.h \
		gtype-go.h gtype.state

# XXX
COPTS.varasm.c=			-Wno-error

# XXX Override the normal rule to use HOST_COMPILE.cc
.c.lo:
	${_MKTARGET_COMPILE}
	${HOST_COMPILE.cc} -o ${.TARGET}.o ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}
	mv ${.TARGET}.o ${.TARGET}

# Headers that host objects depend on (except gen*rtl*)
HH_NORTL=	${G_tm_file_list} ${G_build_xm_include_list}
HH=		${HH_NORTL} genrtl.h insn-modes.h

#
# Generate the various header files we need.
#
genrtl.h: gengenrtl
	${_MKTARGET_CREATE}
	./gengenrtl >${.TARGET}
gengenrtl.lo: ${HH_NORTL}
gengenrtl: gengenrtl.lo build-errors.lo
	${_MKTARGET_LINK}
	${HOST_LINK.cc} -o ${.TARGET} ${.ALLSRC} ${NBCOMPATLIB} ${HOSTLIBIBERTY}

insn-preds.c: genpreds ${G_md_file}
	${_MKTARGET_CREATE}
	./genpreds ${G_md_file} >${.TARGET}
tm-preds.h: genpreds ${G_md_file}
	${_MKTARGET_CREATE}
	./genpreds -h ${G_md_file} >${.TARGET}
tm-constrs.h: genpreds ${G_md_file}
	${_MKTARGET_CREATE}
	./genpreds -c ${G_md_file} >${.TARGET}

# be nice to generate this entirely; but difficult.
gtyp-input.list.tmp: ${GCCARCH}/gtyp-input.list
	sed s#SRCDIR#${DIST}# < ${.ALLSRC} > ${.TARGET}
CLEANFILES+=	gtyp-input.list.tmp

# XXX
CLEANFILES+=	gtype-lto.h gtype-objcp.h tm-constrs.h

gtype-desc.c: gtype-desc.h
gtype-desc.h: gengtype gtyp-input.list.tmp
	${_MKTARGET_CREATE}
	rm -f auto-host.h
	ln -s ${.CURDIR}/../gcc/arch/${GCC_MACHINE_ARCH}/auto-host.h .
	if [ -f ${.CURDIR}/../gcc/arch/${GCC_MACHINE_ARCH}/sysroot-suffix.h ]; then \
	  rm -f sysroot-suffix.h; \
	  ln -s ${.CURDIR}/../gcc/arch/${GCC_MACHINE_ARCH}/sysroot-suffix.h ; \
	fi
	./gengtype -S ${DIST}/gcc -I gtyp-input.list.tmp -w gtype.state
	./gengtype -r gtype.state
	# GCC 4.8 installs gtype-state and gengtype as a plugin

# gengtype is the real need for options.h
gengtype.lo gengtype-lex.lo gengtype-parse.lo gengtype-state.lo version.lo: ${HH} gtyp-gen.h config.h options.h
gengtype: gengtype.lo gengtype-lex.lo gengtype-parse.lo gengtype-state.lo build-errors.lo version.lo
	${_MKTARGET_LINK}
	${HOST_LINK.cc} -o ${.TARGET} ${.ALLSRC} ${NBCOMPATLIB} ${HOSTLIBIBERTY}

gtyp-gen.h: Makefile
	${_MKTARGET_CREATE}
	(\
	unset empty							; \
	echo "static const char *srcdir = \"$(G_GTFILES_SRCDIR)\";"	; \
	echo "static const char *lang_files[] = {"			; \
	for f in $$empty $(G_GTFILES_FILES_FILES); do echo "\"$$f\", "; done ; \
	echo "NULL };"							; \
	echo "static const char *langs_for_lang_files[] = {"		; \
	for f in $$empty $(G_GTFILES_FILES_LANGS); do echo "\"$$f\", "; done ; \
	echo "NULL };"							; \
	echo "static const char *all_files[] = {"			; \
	for f in $$empty $(G_GTFILES); do echo "\"$$f\", "; done	; \
	echo " NULL };"							; \
	echo "static const char *lang_dir_names[] = {"			; \
	for f in c $(G_GTFILES_LANG_DIR_NAMES); do echo "\"$$f\", "; done ; \
	echo "NULL };"							; \
	) >${.TARGET}

insn-modes.c: genmodes
	${_MKTARGET_CREATE}
	./genmodes >${.TARGET}
insn-modes.h: genmodes
	${_MKTARGET_CREATE}
	./genmodes -h >${.TARGET}
min-insn-modes.c: genmodes
	${_MKTARGET_CREATE}
	./genmodes -m >${.TARGET}
genmodes.lo: ${HH_NORTL}
genmodes: genmodes.lo build-errors.lo
	${_MKTARGET_LINK}
	${HOST_LINK.cc} -o ${.TARGET} ${.ALLSRC} ${NBCOMPATLIB} ${HOSTLIBIBERTY}

gcov-iov.h: gcov-iov
	${_MKTARGET_CREATE}
	./gcov-iov "${BASEVER}" "${DEVPHASE}" >${.TARGET}
gcov-iov.lo: ${HH_NORTL}
gcov-iov: gcov-iov.lo
	${_MKTARGET_CREATE}
	${HOST_LINK.cc} -o ${.TARGET} ${.ALLSRC} ${NBCOMPATLIB} ${HOSTLIBIBERTY}

tm_p.h: Makefile tm-preds.h
	${_MKTARGET_CREATE}
	TARGET_CPU_DEFAULT="" \
	HEADERS="$(G_tm_p_include_list)" DEFINES="" \
	${HOST_SH} ${GNUHOSTDIST}/gcc/mkconfig.sh ${.TARGET}

${G_OBJS} ${G_OBJS:.o=.d}:	tm_p.h		# XXX crude

.include "../Makefile.options"

# Yuck, we have to run configure to generate this one...
# We use ORIGHOST_CC here because GCC's ./configure expects (and
# depends upon) it.
CLEANFILES+=	auto-build.h
HOST_CFLAGS+= -I${.OBJDIR}
auto-build.h: gmp.h Makefile
	${_MKTARGET_CREATE}
	rm -rf .ab && \
	mkdir .ab && \
	(cd .ab && \
		AWK=${TOOL_AWK:Q} \
		CC=${ORIGHOST_CC:Q} CFLAGS=${HOST_CFLAGS:Q} \
		MAKE=${MAKE:Q} \
		CONFIG_SHELL=${HOST_SH:Q} \
		gcc_cv_ld=${LD} \
		gcc_cv_as=${AS} \
		${HOST_SH} ${GNUHOSTDIST}/gcc/configure \
			--enable-lto \
			--build=`${HOST_SH} ${GNUHOSTDIST}/config.guess` \
			--host=`${HOST_SH} ${GNUHOSTDIST}/config.guess` \
			--target=${MACHINE_GNU_PLATFORM} && \
		mv auto-host.h ../auto-build.h) && \
	rm -rf .ab

#
# These are copies of files we build for the build host that are used
# by the genfoo programs.
#

build-rtl.c: rtl.c \
  ${G_RTL_H} real.h ${G_GCC_H} errors.h gtype-desc.h
	${_MKTARGET_CREATE}
	rm -f build-rtl.c
	${TOOL_SED} -e 's/<config[.]h/hconfig.h/' \
		${GNUHOSTDIST}/gcc/rtl.c >${.TARGET}
build-rtl.lo: ${HH}

build-print-rtl.c: print-rtl.c \
  ${G_RTL_BASE_H} ${G_GTM_H} hard-reg-set.h
	${_MKTARGET_CREATE}
	rm -f build-print-rtl.c
	${TOOL_SED} -e 's/<config[.]h/hconfig.h/' \
		${GNUHOSTDIST}/gcc/print-rtl.c >${.TARGET}
build-print-rtl.lo: ${HH}
build/print-rtl.o: print-rtl.c $(BCONFIG_H) $(SYSTEM_H) coretypes.h     \
  $(GTM_H) $(RTL_BASE_H)


build-bitmap.c: bitmap.c \
  ${G_RTL_H} flags.h ${G_BASIC_BLOCK_H} ${REGS_H} ${G_GCC_H} gtype-desc.h
	${_MKTARGET_CREATE}
	rm -f build-bitmap.c
	${TOOL_SED} -e 's/<config[.]h/hconfig.h/' \
		${GNUHOSTDIST}/gcc/bitmap.c >${.TARGET}
build-bitmap.lo: ${HH}

build-errors.c: errors.c errors.h
	${_MKTARGET_CREATE}
	rm -f build-errors.c
	cat ${GNUHOSTDIST}/gcc/errors.c >${.TARGET}
build-errors.lo: ${HH_NORTL}

build-varray.c: varray.c \
  ${G_RTL_H} ${G_GCC_H} ${TREE_H} bitmap.h errors.h
	${_MKTARGET_CREATE}
	rm -f build-varray.c
	${TOOL_SED} -e 's/<config[.]h/hconfig.h/' \
		${GNUHOSTDIST}/gcc/varray.c >${.TARGET}
build-varray.lo: ${HH} gtype-desc.h

build-ggc-none.c: ggc-none.c \
  ${G_GCC_H} gtype-desc.h
	${_MKTARGET_CREATE}
	rm -f build-ggc-none.c
	${TOOL_SED} -e 's/<config[.]h/hconfig.h/' \
		${GNUHOSTDIST}/gcc/ggc-none.c >${.TARGET}
build-ggc-none.lo: ${HH}

#
# The normal insn-foo generators
#
.for f in gensupport.c read-rtl.c dummy-conditions.c
${f:R}.lo: ${HH}
.endfor

#
# There are 5 main classes of generator programs.  Ones
# that are self contained, ones that use error reporting
# mechanisms, ones that use the MD reader, and ones that
# use the RTL reader.
#
GENPROG_ERROR_DEPENDS=	${G_BUILD_ERRORS:.o=.lo}
GENPROG_READER_DEPENDS=	${G_BUILD_MD:.o=.lo} ${GENPROG_ERROR_DEPENDS}
GENPROG_RTL_DEPENDS=	${G_BUILD_RTL:.o=.lo} ${GENPROG_READER_DEPENDS} ${GENPROG_ERROR_DEPENDS}

#
# First we generate the rules for the generators.
#
.for f in attr attr-common attrtab automata codes conditions config emit \
	  extract flags opinit output peep preds recog mddump condmd
gen${f}.lo: ${HH} gen${f}.c
gen${f}: gen${f}.lo ${GENPROG_RTL_DEPENDS} 
	${_MKTARGET_LINK}
	${HOST_LINK.cc} -o ${.TARGET} ${.ALLSRC} ${NBCOMPATLIB} ${HOSTLIBIBERTY} ${LDFLAGS.${.TARGET}}
CLEANFILES+=	gen${f} gen${f}.lo
.endfor

.for f in mddeps constants enums
gen${f}.lo: ${HH} gen${f}.c
gen${f}: gen${f}.lo ${GENPROG_READER_DEPENDS} 
	${_MKTARGET_LINK}
	${HOST_LINK.cc} -o ${.TARGET} ${.ALLSRC} ${NBCOMPATLIB} ${HOSTLIBIBERTY} ${LDFLAGS.${.TARGET}}
CLEANFILES+=	gen${f} gen${f}.lo
.endfor

.for f in check checksum
gen${f}.lo: ${HH} gen${f}.c
gen${f}: gen${f}.lo
	${_MKTARGET_LINK}
	${HOST_LINK.cc} -o ${.TARGET} ${.ALLSRC} ${NBCOMPATLIB} ${HOSTLIBIBERTY} ${LDFLAGS.${.TARGET}}
CLEANFILES+=	gen${f} gen${f}.lo
.endfor

.include "../../Makefile.hooks"

#
# Second we generate the rules for the generated files.  There
# are 3 main classes:  RTL based or not, plus special cases.
#
# The special cases are:  gencheck, genattrtab, genopinit,
# gencondmd
#

# RTL based files
.for f in attr.h attr-common.h codes.h config.h flags.h \
	  automata.c emit.c extract.c output.c peep.c recog.c
insn-${f}: gen${f:R} ${G_md_file} insn-conditions.md
	${_MKTARGET_CREATE}
	./gen${f:R} ${G_md_file} insn-conditions.md >${.TARGET}
CLEANFILES+=	insn-${f}
.endfor

# normal files
.for f in constants.h enums.c
insn-${f}: gen${f:R} ${G_md_file}
	${_MKTARGET_CREATE}
	./gen${f:R} ${G_md_file} >${.TARGET}
CLEANFILES+=	insn-${f}
.endfor

# genconditions outputs gencondmd.c
gencondmd.c: genconditions ${G_md_file}
	${_MKTARGET_CREATE}
	./genconditions ${G_md_file} >${.TARGET}
CLEANFILES+=	gencondmd.c

# gencheck outputs tree-check.h
tree-check.h: gencheck
	${_MKTARGET_CREATE}
	./gencheck >${.TARGET}
CLEANFILES+=	tree-check.h

# Some generators output is not the normal name.
BUILDSYMLINKS+= insn-check.h tree-check.h

# genattrtab has three output files
# XXX fix me
insn-attrtab.c insn-dfatab.c insn-latencytab.c: genattrtab ${G_md_file} insn-conditions.md 
	${_MKTARGET_CREATE}
	./genattrtab ${G_md_file} insn-conditions.md -Ainsn-attrtab.c -Dinsn-dfatab.c -Linsn-latencytab.c
CLEANFILES+=	insn-attrtab.c insn-dfatab.c insn-latencytab.c

# genopinit has two output files
insn-opinit.c insn-opinit.h: genopinit ${G_md_file} insn-conditions.md
	${_MKTARGET_CREATE}
	./genopinit ${G_md_file} insn-conditions.md -hinsn-opinit.h -cinsn-opinit.c
CLEANFILES+=	insn-opinit.h insn-opinit.c

LDFLAGS.genautomata+=	-lm

# gencondmd outputs a special .md file
insn-conditions.md: gencondmd
	./gencondmd >${.TARGET}
CLEANFILES+=	insn-conditions.md 


#
# Required hard-coded dependancies.
#
genextract.lo: insn-config.h
gencondmd.lo: tm_p.h tm-constrs.h
read-rtl.lo gencodes.lo genattrtab.lo genautomata.lo: insn-constants.h
genattr-common.lo gensupport.lo genconditions.lo: insn-constants.h
genattr.lo genflags.lo genemit.lo genconfig.lo gencheck.lo: insn-constants.h
genopinit.lo genrecog.lo genpreds.lo genoutput.lo genpeep.lo: insn-constants.h
build-ggc-none.lo: gtype-desc.h
ggc-none.o: gtype-desc.h

insn-attrtab.o: insn-config.h
insn-extract.o: insn-config.h
toplev.d toplev.o: options.h
coverage.d: gcov-iov.h
gcov-io.h: gcov-iov.h
alias.d alias.o: insn-constants.h tm_p.h
cgraph.d cgraphunit.d cgraphunit.o: gcov-io.h
vec.lo: gtype-desc.h
gtype-desc.d gtype-desc.o: insn-constants.h
insn-emit.d insn-emit.o: tm-constrs.h
insn-attr.h: insn-attr-common.h
asan.d asan.o: insn-opinit.h
data-streamer.d data-streamer-out.d coverage.d gcov-dump.d gcov-io.d gcov-iov.d gcov.d libgcov.d lto-cgraph.d mcf.d modulo-sched.d profile.d value-prof.d: gcov-io.h
data-streamer.o data-streamer-out.o coverage.o gcov-dump.o gcov-io.o gcov-iov.o gcov.o libgcov.o lto-cgraph.o mcf.o modulo-sched.o profile.o value-prof.o: gcov-io.h
gcov-io.h: gcov-iov.h
df-scan.d df-scan.o: target-hooks-def.h
read-md.d read-md.o read-md.lo: auto-build.h

# XXX make all hooks generated for now.
final.o: target-hooks-def.h c-family/c-target-hooks-def.h common/common-target-hooks-def.h

.if ${GCC_MACHINE_ARCH} == "alpha"
alpha.d alpha.o: tm-constrs.h tm_p.h insn-opinit.h target-hooks-def.h
.endif

.if ${MACHINE_CPU} == "arm"
arm.d arm.o: insn-constants.h tm_p.h insn-opinit.h target-hooks-def.h
.endif

.if ${GCC_MACHINE_ARCH} == "i386" || ${GCC_MACHINE_ARCH} == "x86_64"
i386.d i386.o: tm-constrs.h
.endif

.if ${GCC_MACHINE_ARCH} == "vax"
CPPFLAGS+=-I${.CURDIR}/../../lib/libgcc/libgcov/arch/${GCC_MACHINE_ARCH}
COPTS.expmed.c=-O0
COPTS.recog.c=-O0
.endif

CFLAGS+=	-Wno-stack-protector

.if ${GCC_MACHINE_ARCH} == "m68000" || ${GCC_MACHINE_ARCH} == "m68k" || \
	${GCC_MACHINE_ARCH} == "coldfire"
COPTS.tree-loop-distribution.c=	-O2
.endif

COPTS.tree.c=	${${ACTIVE_CC} == "clang" :? -O0 :}

.PATH: ${DIST}/gcc ${DIST}/libiberty ${GCCARCH} ${G_out_file:H} ${DIST}/include
.PATH: ${DIST}/gcc/config
