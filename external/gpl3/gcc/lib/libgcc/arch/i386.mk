# This file is automatically generated.  DO NOT EDIT!
# Generated from: 	NetBSD: mknative-gcc,v 1.61 2011/07/03 12:26:02 mrg Exp 
# Generated from: NetBSD: mknative.common,v 1.9 2007/02/05 18:26:01 apb Exp 
#
G_INCLUDES=-I. -I. -I${GNUHOSTDIST}/gcc -I${GNUHOSTDIST}/gcc/. -I${GNUHOSTDIST}/gcc/../include -I./../intl -I${GNUHOSTDIST}/gcc/../libcpp/include     -I${GNUHOSTDIST}/gcc/../libdecnumber -I${GNUHOSTDIST}/gcc/../libdecnumber/dpd -I../libdecnumber   -I/usr/include/libelf
G_LIB2ADD=
G_LIB2ADDEH=${GNUHOSTDIST}/gcc/unwind-dw2.c ${GNUHOSTDIST}/gcc/unwind-dw2-fde-glibc.c ${GNUHOSTDIST}/gcc/unwind-sjlj.c ${GNUHOSTDIST}/gcc/gthr-gnat.c ${GNUHOSTDIST}/gcc/unwind-c.c
G_LIB2ADD_ST=
G_LIB1ASMFUNCS=
G_LIB1ASMSRC=
G_LIB2_DIVMOD_FUNCS=_divdi3 _moddi3 _udivdi3 _umoddi3 _udiv_w_sdiv _udivmoddi4
G_LIB2FUNCS_ST=_eprintf __gcc_bcmp
G_LIB2FUNCS_EXTRA=
.if !defined(__MINIX)
G_LIBGCC2_CFLAGS=-O2   -DIN_GCC   -W -Wall -Wwrite-strings -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition  -isystem ./include  -fPIC -g -DHAVE_GTHR_DEFAULT -DIN_LIBGCC2 -D__GCC_FLOAT_NOT_NEEDED 
.else
G_LIBGCC2_CFLAGS=-O2   -DIN_GCC   -W -Wall -Wwrite-strings -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition  -isystem ./include  -fPIC -g -DIN_LIBGCC2 -D__GCC_FLOAT_NOT_NEEDED 
.endif
G_SHLIB_MKMAP=${GNUHOSTDIST}/gcc/mkmap-symver.awk
G_SHLIB_MKMAP_OPTS=
G_SHLIB_MAPFILES=${GNUHOSTDIST}/gcc/libgcc-std.ver
G_SHLIB_NM_FLAGS=-pg
G_EXTRA_HEADERS=${GNUHOSTDIST}/gcc/config/i386/cpuid.h ${GNUHOSTDIST}/gcc/config/i386/mmintrin.h ${GNUHOSTDIST}/gcc/config/i386/mm3dnow.h ${GNUHOSTDIST}/gcc/config/i386/xmmintrin.h ${GNUHOSTDIST}/gcc/config/i386/emmintrin.h ${GNUHOSTDIST}/gcc/config/i386/pmmintrin.h ${GNUHOSTDIST}/gcc/config/i386/tmmintrin.h ${GNUHOSTDIST}/gcc/config/i386/ammintrin.h ${GNUHOSTDIST}/gcc/config/i386/smmintrin.h ${GNUHOSTDIST}/gcc/config/i386/nmmintrin.h ${GNUHOSTDIST}/gcc/config/i386/bmmintrin.h ${GNUHOSTDIST}/gcc/config/i386/fma4intrin.h ${GNUHOSTDIST}/gcc/config/i386/wmmintrin.h ${GNUHOSTDIST}/gcc/config/i386/immintrin.h ${GNUHOSTDIST}/gcc/config/i386/x86intrin.h ${GNUHOSTDIST}/gcc/config/i386/avxintrin.h ${GNUHOSTDIST}/gcc/config/i386/xopintrin.h ${GNUHOSTDIST}/gcc/config/i386/ia32intrin.h ${GNUHOSTDIST}/gcc/config/i386/cross-stdarg.h ${GNUHOSTDIST}/gcc/config/i386/lwpintrin.h ${GNUHOSTDIST}/gcc/config/i386/popcntintrin.h ${GNUHOSTDIST}/gcc/config/i386/abmintrin.h ${GNUHOSTDIST}/gcc/ginclude/tgmath.h mm_malloc.h
G_xm_defines=
G_tm_defines=NETBSD_ENABLE_PTHREADS
G_COLLECT2=collect2
G_UNWIND_H=${GNUHOSTDIST}/gcc/unwind-generic.h
G_xm_include_list=auto-host.h ansidecl.h
