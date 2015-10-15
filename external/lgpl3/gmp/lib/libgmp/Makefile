#	$NetBSD: Makefile,v 1.24 2013/12/19 01:07:03 gdt Exp $

.include <bsd.init.mk>

# We don't install gmp, just provide it for GCC.
LIBISPRIVATE?=	yes

GMP_MACHINE_ARCH?=	${MACHINE_ARCH:S/earmv4/arm/:C/armv[5-7]/arm/}

.if ${LIBISPRIVATE} != "yes"
INCS=         gmp.h
INCSDIR=      /usr/include
.endif

DIST=${.CURDIR}/../../dist

LIB=	gmp

MPF_SRCS = \
	init.c init2.c inits.c set.c			\
	set_ui.c set_si.c set_str.c set_d.c		\
	set_z.c iset.c iset_ui.c iset_si.c		\
	iset_str.c iset_d.c clear.c clears.c		\
	get_str.c dump.c size.c eq.c reldiff.c		\
	sqrt.c random2.c inp_str.c out_str.c		\
	add.c add_ui.c sub.c sub_ui.c ui_sub.c		\
	mul.c mul_ui.c div.c div_ui.c			\
	cmp.c cmp_d.c cmp_ui.c cmp_si.c			\
	mul_2exp.c div_2exp.c abs.c neg.c		\
	set_q.c get_d.c get_d_2exp.c set_dfl_prec.c	\
	set_prc.c set_prc_raw.c get_dfl_prec.c		\
	get_prc.c ui_div.c sqrt_ui.c			\
	ceilfloor.c trunc.c pow_ui.c			\
	urandomb.c swap.c				\
	fits_sint.c fits_slong.c fits_sshort.c		\
	fits_uint.c fits_ulong.c fits_ushort.c		\
	get_si.c get_ui.c				\
	int_p.c

MPZ_SRCS = \
	abs.c add.c add_ui.c				\
	aorsmul.c aorsmul_i.c and.c array_init.c	\
	bin_ui.c bin_uiui.c				\
	cdiv_q.c cdiv_q_ui.c				\
	cdiv_qr.c cdiv_qr_ui.c				\
	cdiv_r.c cdiv_r_ui.c cdiv_ui.c			\
	cfdiv_q_2exp.c cfdiv_r_2exp.c			\
	clear.c clears.c clrbit.c			\
	cmp.c cmp_d.c cmp_si.c cmp_ui.c			\
	cmpabs.c cmpabs_d.c cmpabs_ui.c			\
	com.c combit.c					\
	cong.c cong_2exp.c cong_ui.c			\
	divexact.c divegcd.c dive_ui.c			\
	divis.c divis_ui.c divis_2exp.c dump.c		\
	export.c fac_ui.c fdiv_q.c			\
	fdiv_q_ui.c fdiv_qr.c fdiv_qr_ui.c		\
	fdiv_r.c fdiv_r_ui.c				\
	fdiv_ui.c fib_ui.c fib2_ui.c fits_sint.c	\
	fits_slong.c fits_sshort.c fits_uint.c		\
	fits_ulong.c fits_ushort.c gcd.c		\
	gcd_ui.c gcdext.c get_d.c get_d_2exp.c		\
	get_si.c get_str.c get_ui.c getlimbn.c		\
	hamdist.c					\
	import.c init.c init2.c inits.c 		\
	inp_raw.c inp_str.c invert.c			\
	ior.c iset.c iset_d.c iset_si.c			\
	iset_str.c iset_ui.c jacobi.c kronsz.c		\
	kronuz.c kronzs.c kronzu.c			\
	lcm.c lcm_ui.c lucnum_ui.c lucnum2_ui.c		\
	millerrabin.c mod.c mul.c mul_2exp.c		\
	mul_si.c mul_ui.c				\
	n_pow_ui.c neg.c nextprime.c			\
	out_raw.c out_str.c perfpow.c perfsqr.c		\
	popcount.c pow_ui.c powm.c powm_sec.c		\
	powm_ui.c pprime_p.c random.c random2.c		\
	realloc.c realloc2.c remove.c			\
	root.c rootrem.c rrandomb.c scan0.c		\
	scan1.c set.c set_d.c set_f.c			\
	set_q.c set_si.c set_str.c set_ui.c		\
	setbit.c					\
	size.c sizeinbase.c sqrt.c			\
	sqrtrem.c sub.c sub_ui.c swap.c			\
	tdiv_ui.c tdiv_q.c tdiv_q_2exp.c		\
	tdiv_q_ui.c tdiv_qr.c tdiv_qr_ui.c		\
	tdiv_r.c tdiv_r_2exp.c tdiv_r_ui.c		\
	tstbit.c ui_pow_ui.c ui_sub.c urandomb.c	\
	urandomm.c xor.c

MPQ_SRCS = \
	abs.c aors.c					\
	canonicalize.c clear.c clears.c			\
	cmp.c cmp_si.c cmp_ui.c div.c			\
	get_d.c get_den.c get_num.c get_str.c		\
	init.c inits.c inp_str.c inv.c			\
	md_2exp.c mul.c neg.c out_str.c			\
	set.c set_den.c set_num.c			\
	set_si.c set_str.c set_ui.c			\
	equal.c set_z.c set_d.c				\
	set_f.c swap.c

MPN_GENERIC_SRCS = \
	popcount.c \
	hamdist.c

PRINTF_SRCS =						\
	asprintf.c asprntffuns.c			\
	doprnt.c doprntf.c doprnti.c			\
	fprintf.c					\
	obprintf.c obvprintf.c obprntffuns.c		\
	printf.c printffuns.c				\
	snprintf.c snprntffuns.c			\
	sprintf.c sprintffuns.c				\
	vasprintf.c vfprintf.c vprintf.c		\
	vsnprintf.c vsprintf.c				\
	repl-vsnprintf.c

SCANF_SRCS =						\
	doscan.c fscanf.c fscanffuns.c			\
	scanf.c sscanf.c sscanffuns.c			\
	vfscanf.c vscanf.c vsscanf.c

SRCS+= 									\
	assert.c compat.c errno.c extract-dbl.c invalid.c memory.c	\
	mp_bpl.c mp_clz_tab.c mp_dv_tab.c mp_minv_tab.c mp_get_fns.c	\
	mp_set_fns.c randclr.c randdef.c randiset.c randlc2s.c 	\
	randlc2x.c randmt.c randmts.c rands.c randsd.c randsdui.c 	\
	randbui.c randmui.c version.c nextprime.c

SRCS+=	tal-reent.c

# these are generated
SRCS+=	fib_table.c mp_bases.c

# Build the files that may have multiple identical base names under
# different names.
.for _dir _srcfile in \
	${MPF_SRCS:C/^/mpf /} \
	${MPZ_SRCS:C/^/mpz /} \
	${MPQ_SRCS:C/^/mpq /} \
	${PRINTF_SRCS:C/^/printf /} \
	${SCANF_SRCS:C/^/scanf /}
${_dir}_${_srcfile}: #Makefile
	echo '#include "'${_dir}/${_srcfile}'"' > ${.TARGET}

SRCS+=		${_dir}_${_srcfile}
CLEANFILES+=	${_dir}_${_srcfile}

.endfor

CPPFLAGS+=	-I. -I${DIST} -I${.CURDIR}/arch/${GMP_MACHINE_ARCH} \
		-D__GMP_WITHIN_GMP -DHAVE_CONFIG_H

WITHOUT_MAN=	yes

DPSRCS= fac_table.h fib_table.h fib_table.c jacobitab.h mp_bases.h \
	mp_bases.c perfsqr.h trialdivtab.h

gen-fac: gen-fac.c
	${HOST_CC} -o ${.OBJDIR}/gen-fac ${.ALLSRC}

fac_table.h: gen-fac
	${.OBJDIR}/gen-fac ${GMP_LIMB_BITS} 0 > ${.OBJDIR}/fac_table.h

gen-fib: gen-fib.c
	${HOST_CC} -o ${.OBJDIR}/gen-fib ${.ALLSRC}

fib_table.h: gen-fib
	${.OBJDIR}/gen-fib header ${GMP_LIMB_BITS} 0 > ${.OBJDIR}/fib_table.h

fib_table.c: gen-fib
	${.OBJDIR}/gen-fib table ${GMP_LIMB_BITS} 0 > ${.OBJDIR}/fib_table.c

gen-bases: gen-bases.c
	${HOST_CC} -o ${.OBJDIR}/gen-bases ${.ALLSRC} -lm

mp_bases.h: gen-bases
	${.OBJDIR}/gen-bases header ${GMP_LIMB_BITS} 0 > ${.OBJDIR}/mp_bases.h

mp_bases.c: gen-bases
	${.OBJDIR}/gen-bases table ${GMP_LIMB_BITS} 0 > ${.OBJDIR}/mp_bases.c

gen-jacobitab: gen-jacobitab.c
	${HOST_CC} -o ${.OBJDIR}/gen-jacobitab ${.ALLSRC} -lm

jacobitab.h: gen-jacobitab
	${.OBJDIR}/gen-jacobitab > ${.OBJDIR}/jacobitab.h

perfsqr.h: gen-psqr.c
	${HOST_CC} -o ${.OBJDIR}/gen-psqr ${.ALLSRC}
	${.OBJDIR}/gen-psqr ${GMP_LIMB_BITS} 0 > ${.OBJDIR}/perfsqr.h

gen-trialdivtab: gen-trialdivtab.c
	${HOST_CC} -o ${.OBJDIR}/gen-trialdivtab ${.ALLSRC} -lm

trialdiv.c: trialdivtab.h
trialdivtab.h: gen-trialdivtab Makefile
	${.OBJDIR}/gen-trialdivtab $(GMP_LIMB_BITS) 8000 > ${.OBJDIR}/trialdivtab.h

CLEANFILES+=	${DPSRCS} gen-fac gen-fib gen-bases gen-jacobitab \
		gen-psqr gen-trialdivtab

.include "${.CURDIR}/arch/${GMP_MACHINE_ARCH}/Makefile.inc"

# Makefile.inc must define GMP_LIMB_BITS.
.if !defined(GMP_LIMB_BITS)
.BEGIN:
	@echo "GMP_LIMB_BITS not defined!"
	false
.endif

# XXX stuff looks for "../config.m4"
dummy:
	mkdir dummy
${.OBJDIR}/config.m4: ${.CURDIR}/arch/${GMP_MACHINE_ARCH}/config.m4
	rm -f ${.OBJDIR}/config.m4 && cp ${.CURDIR}/arch/${GMP_MACHINE_ARCH}/config.m4 ${.OBJDIR}/config.m4

CLEANFILES+=	config.m4
clean: cleandummy
cleandummy: .PHONY
	-rmdir dummy

BUILD_S_INCS=	-I${.CURDIR} -I${.OBJDIR}/dummy -I${DIST}/ -I${DIST}/mpn \
		-I${.OBJDIR}

.for _build _src in ${ASM_SRCS_LIST}
${_build:R}.s: Makefile dummy ${.OBJDIR}/config.m4 ${DIST}/${_src}
	${TOOL_M4} -DCONFIG_TOP_SRCDIR=${DIST} ${BUILD_S_INCS} \
		${M4FLAGS} ${M4FLAGS.${.TARGET:R}} \
		-DOPERATION_${.TARGET:R} ${DIST}/${_src} > ${.TARGET}

SRCS+=		${_build:R}.s
CLEANFILES+=	${_build:R}.s
.endfor

.for _build _src in ${C_SRCS_LIST}
mpn_${_build}: Makefile
	echo '#include "${_src}"' > ${.TARGET}

CPPFLAGS.mpn_${_build}+= -DOPERATION_${_build:R}

SRCS+=		mpn_${_build}   
CLEANFILES+=	mpn_${_build}   
.endfor

.include <bsd.lib.mk>

.PATH:	${DIST} \
	${DIST}/rand \
	${DIST}/mpn/generic \
	${.CURDIR}/arch/${GMP_MACHINE_ARCH} \
	${DIST}/mpf \
	${DIST}/mpz

# Don't warn about functions which cannot be stack smash protected as
# there are a lot of them.
COPTS += -Wno-stack-protector
.if !empty(GMP_MACHINE_ARCH:M*arm*)
COPTS += -marm
.endif

CWARNFLAGS.clang+=	-Wno-unused-value -Wno-tautological-compare -Wno-parentheses
