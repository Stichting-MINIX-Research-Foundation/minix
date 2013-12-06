#	$NetBSD: Makefile,v 1.4 2013/11/30 14:52:40 christos Exp $

.include <bsd.own.mk>

USE_WIDECHAR?=yes

CWARNFLAGS.clang+=	-Wno-uninitialized -Wno-format-security

CPPFLAGS+=-I${DIST}/include -I${.CURDIR} -I. -DGTAGS
#DBG=-g
#CPPFLAGS+=-DLOGDEBUG -DTRACE
#CPPFLAGS+=-DDBDEBUG -DTRACE

#CWARNFLAGS+=-Wno-parentheses -Wno-unused -Wno-missing-prototypes
#.if defined(HAVE_GCC)
#CWARNFLAGS+=-Wno-pointer-sign
#COPTS+=-fno-strict-aliasing
#.endif

LDADD+=	-lcurses -lterminfo
DPADD+=	${LIBCURSES} ${LIBTERMINFO}
PROG=	vi
SRCS=	api.c cl_bsd.c cl_funcs.c cl_main.c cl_read.c cl_screen.c cl_term.c \
	conv.c cut.c delete.c ex.c ex_abbrev.c ex_append.c \
	ex_args.c ex_argv.c ex_at.c ex_bang.c ex_cd.c ex_cmd.c ex_cscope.c \
	ex_delete.c ex_display.c ex_edit.c ex_equal.c ex_file.c ex_filter.c \
	ex_global.c ex_init.c ex_join.c ex_map.c ex_mark.c ex_mkexrc.c \
	ex_move.c ex_open.c ex_perl.c ex_preserve.c ex_print.c ex_put.c \
	ex_quit.c ex_read.c ex_screen.c ex_script.c ex_set.c ex_shell.c \
	ex_shift.c ex_source.c ex_stop.c ex_subst.c ex_tag.c ex_tcl.c \
	ex_txt.c ex_undo.c ex_usage.c ex_util.c ex_version.c ex_visual.c \
	ex_write.c ex_yank.c ex_z.c exf.c getc.c gs.c \
	ip_funcs.c ip_read.c ip_screen.c ip_term.c \
	ip_run.c ip_send.c ip_trans.c ipc_cmd.c ipc_method.c \
	key.c trace.c \
	main.c mark.c msg.c nothread.c options.c options_f.c put.c recover.c \
	screen.c search.c seq.c util.c v_at.c v_ch.c v_cmd.c v_delete.c \
	v_event.c v_ex.c v_increment.c v_init.c v_itxt.c v_left.c v_mark.c \
	v_match.c v_paragraph.c v_put.c v_redraw.c v_replace.c v_right.c \
	v_screen.c v_scroll.c v_search.c v_section.c v_sentence.c v_status.c \
	v_txt.c v_ulcase.c v_undo.c v_util.c v_word.c v_xchar.c v_yank.c \
	v_z.c v_zexit.c vi.c vs_line.c vs_msg.c vs_refresh.c \
	vs_relative.c vs_smap.c vs_split.c

# For wide char support
.if ${USE_WIDECHAR} == "yes"
SRCS+=	regcomp.c regerror.c regexec.c regfree.c
CPPFLAGS+=-I${DIST}/regex -D__REGEX_PRIVATE -DUSE_WIDECHAR
.endif

# For db3 db1 emulation
#SRCS+=db1.c log.c vi_db.c
#CPPFLAGS+=-DUSE_DB1

# For built-in db1 emulation
SRCS+=vi_db1.c log1.c
CPPFLAGS+=-DUSE_BUNDLED_DB

LINKS=	${BINDIR}/vi ${BINDIR}/ex ${BINDIR}/vi ${BINDIR}/view
MLINKS=	vi.1 ex.1 vi.1 view.1

DPSRCS+=options_def.h ipc_def.h ex_def.h # ipc_gen.c
CLEANFILES+=options_def.h ipc_def.h ex_def.h # ipc_gen.c

options_def.h: options.awk options.c
	${_MKTARGET_CREATE}
	${TOOL_AWK} -f ${.ALLSRC} >${.TARGET}

ex_def.h: ex.awk ex_cmd.c
	${_MKTARGET_CREATE}
	${TOOL_AWK} -f ${.ALLSRC} >${.TARGET}

ipc_def.h: ipc.awk vipc.awk ipc_cmd.c ip_read.c
	${_MKTARGET_CREATE}
	(${TOOL_AWK} -f ${.ALLSRC:M*/ipc.awk} ${.ALLSRC:M*/ipc_cmd.c} && \
	    ${TOOL_AWK} -f ${.ALLSRC:M*/vipc.awk} ${.ALLSRC:M*/ip_read.c}) > \
	    ${.TARGET}

.for i in cl common ex ip ipc vi
DPSRCS+=${i}_extern.h
CLEANFILES+=${i}_extern.h
_${i}_SRCS != echo ${DIST}/${i}/*.c ${DIST}/${i}/*.xs
${i}_extern.h: ${SRCS}
	${_MKTARGET_CREATE}
	@${TOOL_SED} -n "s/^ \* PUBLIC: \(.*\)/\1/p" ${.ALLSRC:M*/$i/*} > ${.TARGET}.tmp
	@if cmp -s ${.TARGET}.tmp ${.TARGET}; then \
		rm -f ${.TARGET}.tmp; \
	else \
		mv -f ${.TARGET}.tmp ${.TARGET}; \
	fi
.endfor

DPSRCS+=version.h
CLEANFILES+=version.h
version.h:	../../Makefile.inc
	${_MKTARGET_CREATE}
	echo '#define VI_VERSION "(${VERSION})"' > ${.TARGET}

#ipc_gen.c: vipc.pl ip_read.c
#	${_MKTARGET_CREATE}
#	${TOOL_PERL} -n ${.ALLSRC} > ${.TARGET}


.include "${.CURDIR}/../../Makefile.inc"

WARNS=	4

.PATH: ${DIST}/vi ${DIST}/ex ${DIST}/cl
.PATH: ${DIST}/common
.PATH: ${DIST}/ipc
.PATH: ${DIST}/ip
.PATH: ${DIST}/regex
.PATH: ${DIST}/docs/vi.man

COPTS.exf.c += -Wno-format-nonliteral
COPTS.msg.c += -Wno-format-nonliteral

.include <bsd.prog.mk>
