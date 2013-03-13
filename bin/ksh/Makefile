#	$NetBSD: Makefile,v 1.30 2011/10/16 17:12:11 joerg Exp $

WARNS=3

.include <bsd.own.mk>

CPPFLAGS+=	-I.

PROG=	ksh
SRCS=	alloc.c c_ksh.c c_sh.c c_test.c c_ulimit.c edit.c emacs.c \
	eval.c exec.c expr.c history.c io.c jobs.c lex.c mail.c \
	main.c misc.c path.c shf.c sigact.c syn.c table.c trap.c \
	tree.c tty.c var.c version.c vi.c
DPSRCS=	emacs.out siglist.out
.if (${MKMAN} != "no")
DPSRCS+=ksh.1
.endif

# needs tbl for the man page.
USETBL= 

# Environment for scripts executed during build.
SCRIPT_ENV= \
	AWK=${TOOL_AWK:Q} \
	SED=${TOOL_SED:Q}

CLEANFILES+=	siglist.out siglist.out.tmp
# two steps to prevent the creation of a bogus siglist.out
siglist.out: config.h sh.h siglist.in siglist.sh
	${_MKTARGET_CREATE}
	${SCRIPT_ENV} \
	${HOST_SH} $(.CURDIR)/siglist.sh "$(CC) -E $(CPPFLAGS) $(DEFS) -I. -I$(.CURDIR)" < $(.CURDIR)/siglist.in > siglist.out.tmp \
	    && mv siglist.out.tmp siglist.out

# two steps to prevent the creation of a bogus emacs.out
CLEANFILES+=	emacs.out emacs.out.tmp
emacs.out: emacs.c
	${_MKTARGET_CREATE}
	${SCRIPT_ENV} \
	${HOST_SH} $(.CURDIR)/emacs-gen.sh $(.CURDIR)/emacs.c > emacs.out.tmp \
	    && mv emacs.out.tmp emacs.out

CLEANFILES+=	ksh.1 ksh.1.tmp
ksh.1: ksh.Man mkman
	${_MKTARGET_CREATE}
	${SCRIPT_ENV} \
	${HOST_SH} $(.CURDIR)/mkman ksh $(.CURDIR)/ksh.Man >ksh.1.tmp \
	    && mv ksh.1.tmp ksh.1

.include <bsd.prog.mk>
