#	$NetBSD: Makefile.inc,v 1.90 2015/05/01 14:17:56 christos Exp $
#	from: @(#)Makefile.inc	8.3 (Berkeley) 2/4/95

# stdlib sources
.PATH: ${ARCHDIR}/stdlib ${.CURDIR}/stdlib

SRCS+=	_env.c _rand48.c \
	a64l.c abort.c atexit.c atof.c atoi.c atol.c atoll.c \
	bsearch.c drand48.c exit.c \
	getenv.c getopt.c getopt_long.c getsubopt.c \
	hcreate.c heapsort.c imaxdiv.c insque.c jrand48.c l64a.c lldiv.c \
	lcong48.c lrand48.c lsearch.c merge.c mi_vector_hash.c mrand48.c \
	nrand48.c putenv.c qabs.c qdiv.c qsort.c posix_openpt.c pty.c \
	quick_exit.c radixsort.c rand.c rand_r.c random.c remque.c \
	reallocarr.c reallocarray.c seed48.c setenv.c srand48.c strsuftoll.c \
	strtoi.c strtou.c strtonum.c \
	strtoimax.c strtol.c strtoll.c strtoq.c strtoul.c strtoull.c \
	strtoumax.c strtouq.c system.c tdelete.c tfind.c tsearch.c twalk.c \
	unsetenv.c strfmon.c

# These are often replaced by MD .S versions
SRCS+=	abs.c labs.c llabs.c imaxabs.c
SRCS+=	div.c ldiv.c imaxdiv.c

# This might be replaced by erand48.c
SRCS+= erand48_ieee754.c

.if ${RUMPRUN} != "yes"
.if (${USE_JEMALLOC} != "no")
SRCS+=	jemalloc.c
.else
SRCS+=	malloc.c
.endif
.endif

.if defined(__MINIX)
.include "${NETBSDSRCDIR}/minix/lib/libc/Makefile.inc"
.endif # defined(__MINIX)

CPPFLAGS.strtol.c+=	-I${LIBCDIR}/../../common/lib/libc/stdlib
CPPFLAGS.strtoq.c+=	-I${LIBCDIR}/../../common/lib/libc/stdlib
CPPFLAGS.strtouq.c+=	-I${LIBCDIR}/../../common/lib/libc/stdlib

.include "${ARCHDIR}/stdlib/Makefile.inc"

MAN+=	a64l.3 abort.3 abs.3 alloca.3 atexit.3 atof.3 atoi.3 atol.3 atoll.3 \
	at_quick_exit.3 \
	bsearch.3 \
	div.3 \
	exit.3 \
	getenv.3 getopt.3 getopt_long.3 getsubopt.3 grantpt.3 \
	hcreate.3 \
	insque.3 \
	jemalloc.3 \
	lsearch.3 \
	malloc.3 memory.3 mi_vector_hash.3 \
	posix_memalign.3 posix_openpt.3 ptsname.3 \
	qabs.3 qdiv.3 quick_exit.3 qsort.3 \
	radixsort.3 rand48.3 rand.3 random.3 reallocarr.3 reallocarray.3 \
	strfmon.3 strsuftoll.3 strtod.3 strtoi.3 strtol.3 strtou.3 strtoul.3 \
	strtonum.3 system.3 \
	tsearch.3 \
	unlockpt.3

MLINKS+=a64l.3 l64a.3
MLINKS+=a64l.3 l64a_r.3
MLINKS+=abs.3 labs.3 \
	abs.3 llabs.3 \
	abs.3 imaxabs.3
MLINKS+=div.3 ldiv.3 \
	div.3 lldiv.3 \
	div.3 imaxdiv.3
MLINKS+=getenv.3 setenv.3 getenv.3 unsetenv.3 getenv.3 putenv.3
MLINKS+=getenv.3 getenv_r.3
MLINKS+=hcreate.3 hdestroy.3 hcreate.3 hsearch.3
MLINKS+=hcreate.3 hcreate_r.3 hcreate.3 hdestroy_r.3 hcreate.3 hsearch_r.3
MLINKS+=hcreate.3 hdestroy1.3 hcreate.3 hdestroy1_r.3
MLINKS+=insque.3 remque.3
MLINKS+=lsearch.3 lfind.3
MLINKS+=malloc.3 calloc.3 malloc.3 realloc.3 malloc.3 free.3
MLINKS+=qsort.3 heapsort.3 qsort.3 mergesort.3
MLINKS+=ptsname.3 ptsname_r.3
MLINKS+=rand.3 rand_r.3
MLINKS+=rand.3 srand.3
MLINKS+=rand48.3 drand48.3 rand48.3 erand48.3 rand48.3 lrand48.3
MLINKS+=rand48.3 mrand48.3 rand48.3 nrand48.3 rand48.3 jrand48.3
MLINKS+=rand48.3 srand48.3 rand48.3 seed48.3 rand48.3 lcong48.3
MLINKS+=random.3 initstate.3 random.3 setstate.3 random.3 srandom.3
MLINKS+=radixsort.3 sradixsort.3
MLINKS+=strsuftoll.3 strsuftollx.3
MLINKS+=strtod.3 strtof.3 strtod.3 strtold.3
MLINKS+=strtol.3 strtoimax.3
MLINKS+=strtol.3 strtoll.3
MLINKS+=strtol.3 strtoq.3
MLINKS+=strtoul.3 strtoull.3
MLINKS+=strtoul.3 strtoumax.3
MLINKS+=strtoul.3 strtouq.3
MLINKS+=tsearch.3 tfind.3 tsearch.3 twalk.3 tsearch.3 tdelete.3
