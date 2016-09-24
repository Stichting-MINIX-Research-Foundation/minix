LCOV=lcov.$(PROG)
CLEANFILES+= *.gcno *.gcda $(LCOV)

# Right now we support obtaining coverage information for system services only,
# and for their main program code (not including their libraries) only.
#
# Why not userland as well: because we do not care as much, and it should be
# possible to produce coverage information for system services without
# recompiling the entire system with coverage support.  Moreover, as of writing
# we do not have libprofile_rt, making it impossible to compile regular
# programs with coverage support altogether.
#
# Why not system service libraries (eg libsys) as well: practical concerns..
# 1) As of writing, even for such libraries we make a regular and a PIC
#    version, both producing a .gcno file for each object.  The PIC version is
#    compiled last, while the regular version is used for the library archive.
#    The result is a potential mismatch between the compile-time coverage
#    metadata and the run-time coverage counts.
# 2) The kernel has no coverage support, and with its self-relocation it would
#    be tricky to add support for it.  As a result, libraries used by the
#    kernel would have to be excluded from being compiled with coverage support
#    so as not to create problems.  One could argue that that is a good thing
#    because eg libminc and libsys create too many small result units (see also
#    the current hardcoded limit in libsys/llvm_gcov.c).
# 3) gcov-pull(8) strips paths, which results in lots of manual work to figure
#    out what file belongs to which library, even ignoring object name
#    conflicts, for example between libraries.
# 4) In order to produce practically useful results ("how much of libsockevent
#    is covered by the combination of LWIP and UDS" etc), gcov-pull(8) would
#    have to be extended with support for merging .gcda files.  The standard
#    LLVM libprofile_rt implementation supports this, but we do not.
# All of these issues are solvable, but for now anyone interested in coverage
# for a particular system service library will have to mess with individual
# makefiles themselves.

.if ${MKCOVERAGE:Uno} == "yes"
.if ${ACTIVE_CC} == "gcc"
# Leftovers for GCC.  It is not clear whether these still work at all.
COVCPPFLAGS?= -fno-builtin -fprofile-arcs -ftest-coverage
COVLDADD?= -lgcov
.else # ${ACTIVE_CC} != "gcc"
# We assume LLVM/clang here.  For other compilers this will likely break the
# MKCOVERAGE compilation, which is a good indication that support for them
# should be added here.
COVCPPFLAGS?= --coverage -g -O0
COVLDADD?=
.endif # ${ACTIVE_CC} != "gcc"
.endif # ${MKCOVERAGE:Uno} == "yes"

lcov:
	lcov -c -d . >$(LCOV)
