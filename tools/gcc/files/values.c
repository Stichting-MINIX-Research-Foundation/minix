#if 0 /* $NetBSD$ */
#
# This is Solaris x86 specific GCC run-time environment patch, which
# makes it possible to reliably deploy .init snippets. Trouble is that
# Solaris linker erroneously pads .init segment with zeros [instead of
# nops], which is bound to SEGV early upon program start-up. This bug
# was recognized by GCC team [it is mentioned in source code], but
# workaround apparently and obviously erroneously slipped away in some
# newer GCC release. This patch compensates for this mishap by dropping
# modified values-X*.o into GCC installation tree. Object modules in
# question are normally provided by Sun and linked prior crtbegin.o.
# Modified versions are additionally crafted with custom .init segment,
# which does some magic:-)
#						<appro@fy.chalmers.se>
set -e
gcc=gcc
if [[ "x$1" = x*gcc ]]; then
	gcc=$1; shift
fi
gcc_dir=`${gcc} "$@" -print-libgcc-file-name`
gcc_dir=${gcc_dir%/*}	#*/
set -x 
${gcc} "$@" -c -o $gcc_dir/values-Xa.o -DXa $0
${gcc} "$@" -c -o $gcc_dir/values-Xc.o -DXc $0
${gcc} "$@" -c -o $gcc_dir/values-Xt.o -DXt $0
exit
#endif

#include <math.h>

#if defined(Xa)
const enum version _lib_version = ansi_1;
#elif defined(Xc)
const enum version _lib_version = strict_ansi;
#elif defined(Xt)
const enum version _lib_version = c_issue_4;
#else
#error "compile by issuing 'ksh -f values.c [gcc] [-m64]'"
#endif

#if defined(__x86_64__)
asm("\n"
".section	.init\n"
".align	1\n"
"	leaq	1f(%rip),%rax\n"
"1:	cmpl	$0,2f-1b(%rax)\n"
"	jne	2f\n"
"	jmp	2f+5\n"
"	.skip	9\n"	/* pad up to 0x1b bytes */
"2:\n"
);
#else
asm("\n"
".section	.init\n"
".align	1\n"
"	call	1f\n"
"1:	popl	%eax\n"
"	cmpl	$0,2f-1b(%eax)\n"
"	jne	2f\n"
"	jmp	2f+5\n"
"	.skip	10\n"	/* pad up to 0x1b bytes */
"2:\n"
);
#endif
