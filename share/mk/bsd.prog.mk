#	$NetBSD: bsd.prog.mk,v 1.278 2012/08/24 20:26:24 jmmv Exp $
#	@(#)bsd.prog.mk	8.2 (Berkeley) 4/2/94

.ifndef HOSTPROG

.include <bsd.init.mk>
.include <bsd.shlib.mk>
.include <bsd.gcc.mk>
.include <minix.gcov.mk>

#
# Definitions and targets shared among all programs built by a single
# Makefile.
#

##### Basic targets
realinstall:	proginstall scriptsinstall

.gdbinit:
	rm -f .gdbinit
.if defined(DESTDIR) && !empty(DESTDIR)
	echo "set solib-absolute-prefix ${DESTDIR}" > .gdbinit
.else
	touch .gdbinit
.endif
.for __gdbinit in ${GDBINIT}
	echo "source ${__gdbinit}" >> .gdbinit
.endfor

CLEANFILES+= a.out [Ee]rrs mklog core *.core .gdbinit

.if defined(SHAREDSTRINGS)
CLEANFILES+=strings
.c.o:
	${CC} -E ${CPPFLAGS} ${CFLAGS} ${.IMPSRC} | xstr -c -
	@${CC} ${CPPFLAGS} ${CFLAGS} -c x.c -o ${.TARGET}
	@rm -f x.c

.cc.o .cpp.o .cxx.o .C.o:
	${CXX} -E ${CPPFLAGS} ${CXXFLAGS} ${.IMPSRC} | xstr -c -
	@mv -f x.c x.cc
	@${CXX} ${CPPFLAGS} ${CXXFLAGS} -c x.cc -o ${.TARGET}
	@rm -f x.cc
.endif

.if defined(MKPIE) && (${MKPIE} != "no")
CFLAGS+=	${PIE_CFLAGS}
AFLAGS+=	${PIE_AFLAGS}
LDFLAGS+=	${PIE_LDFLAGS}
.endif

CFLAGS+=	${COPTS}
OBJCFLAGS+=	${OBJCOPTS}
MKDEP_SUFFIXES?=	.o .ln

# CTF preserve debug symbols
.if defined(MKDTRACE) && (${MKDTRACE} != "no") && (${CFLAGS:M-g} != "")
CTFFLAGS+= -g
CTFMFLAGS+= -g
.endif

# ELF platforms depend on crti.o, crtbegin.o, crtend.o, and crtn.o
.ifndef LIBCRTBEGIN
LIBCRTBEGIN=	${DESTDIR}/usr/lib/crti.o ${_GCC_CRTBEGIN}
.MADE: ${LIBCRTBEGIN}
.endif
.ifndef LIBCRTEND
LIBCRTEND=	${_GCC_CRTEND} ${DESTDIR}/usr/lib/crtn.o
.MADE: ${LIBCRTEND}
.endif
_SHLINKER=	${SHLINKDIR}/ld.elf_so

.ifndef LIBCRT0
LIBCRT0=	${DESTDIR}/usr/lib/crt0.o
.MADE: ${LIBCRT0}
.endif

##### Installed system library definitions
#
#	E.g.
#		LIBC?=${DESTDIR}/usr/lib/libc.a
#		LIBX11?=${DESTDIR}/usr/X11R7/lib/libX11.a
#	etc..
#	NB:	If you are a library here, add it in bsd.README

.for _lib in \
	audiodriver \
	blockdriver \
	chardriver \
	netdriver \
	c \
	curses \
	edit \
	expat \
	end \
	sys \
	timers \
	util \
	bz2 \
	l \
	m  \
	exec \
	ddekit \
	devman \
	usb \
	elf \
	bdev \
	sffs \
	hgfs \
	vboxfs \
	minc \
	minlib \
	min\
	lib

.ifndef LIB${_lib:tu}
LIB${_lib:tu}=	${DESTDIR}/usr/lib/lib${_lib:S/xx/++/:S/atf_c/atf-c/}.a
.MADE:		${LIB${_lib:tu}}	# Note: ${DESTDIR} will be expanded
.endif
.endfor

# PAM applications, if linked statically, need more libraries
.if (${MKPIC} == "no")
.if (${MKCRYPTO} != "no")
PAM_STATIC_LDADD+= -lssh
PAM_STATIC_DPADD+= ${LIBSSH}
.endif
.if (${MKKERBEROS} != "no")
PAM_STATIC_LDADD+= -lkafs -lkrb5 -lhx509 -lwind -lasn1 \
	-lroken -lcom_err -lheimbase -lcrypto
PAM_STATIC_DPADD+= ${LIBKAFS} ${LIBKRB5} ${LIBHX509} ${LIBWIND} ${LIBASN1} \
	${LIBROKEN} ${LIBCOM_ERR} ${LIBHEIMBASE} ${LIBCRYPTO}
.endif
.if (${MKSKEY} != "no")
PAM_STATIC_LDADD+= -lskey
PAM_STATIC_DPADD+= ${LIBSKEY}
.endif
PAM_STATIC_LDADD+= -lradius -lcrypt -lrpcsvc -lutil
PAM_STATIC_DPADD+= ${LIBRADIUS} ${LIBCRYPT} ${LIBRPCSVC} ${LIBUTIL}
.else
PAM_STATIC_LDADD=
PAM_STATIC_DPADD=
.endif

#	NB:	If you are a library here, add it in bsd.README
.for _lib in \
	FS \
	GL \
	GLU \
	ICE \
	SM \
	X11 \
	XTrap \
	Xau \
	Xaw \
	Xdmcp \
	Xext \
	Xfont \
	Xft \
	Xi \
	Xinerama \
	Xmu \
	Xmuu \
	Xpm \
	Xrandr \
	Xrender \
	Xss \
	Xt \
	Xtst \
	Xv \
	Xxf86dga \
	Xxf86misc \
	Xxf86vm \
	dps \
	fntstubs \
	fontcache \
	fontconfig \
	fontenc \
	freetype \
	lbxutil \
	xkbfile
.ifndef LIB${_lib:tu}
LIB${_lib:tu}=	${DESTDIR}${X11USRLIBDIR}/lib${_lib}.a
.MADE:		${LIB${_lib:tu}}	# Note: ${DESTDIR} will be expanded
.endif
.endfor

.if defined(RESCUEDIR)
CPPFLAGS+=	-DRESCUEDIR=\"${RESCUEDIR}\"
.endif

_PROGLDOPTS=
.if ${SHLINKDIR} != "/usr/libexec"	# XXX: change or remove if ld.so moves
_PROGLDOPTS+=	-Wl,-dynamic-linker=${_SHLINKER}
.endif
.if ${SHLIBDIR} != "/usr/lib"
_PROGLDOPTS+=	-Wl,-rpath,${SHLIBDIR} \
		-L=${SHLIBDIR}
.elif ${SHLIBINSTALLDIR} != "/usr/lib"
_PROGLDOPTS+=	-Wl,-rpath-link,${DESTDIR}${SHLIBINSTALLDIR} \
		-L=${SHLIBINSTALLDIR}
.endif

__proginstall: .USE
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${STRIPFLAG} ${.ALLSRC} ${.TARGET}

__progrumpinstall: .USE
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${RUMPBINOWN} -g ${RUMPBINGRP} -m ${RUMPBINMODE} \
		${STRIPFLAG} ${.ALLSRC} ${.TARGET}

__progdebuginstall: .USE
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${DEBUGOWN} -g ${DEBUGGRP} -m ${DEBUGMODE} \
		${.ALLSRC} ${.TARGET}



#
# Backwards compatibility with Makefiles that assume that bsd.prog.mk
# can only build a single binary.
#

_APPEND_MANS=yes
_APPEND_SRCS=yes

_CCLINKFLAGS=

.if defined(PROG_CXX)
PROG=		${PROG_CXX}
_CCLINK=	${CXX} ${_CCLINKFLAGS}
.endif

.if defined(RUMPPRG)
PROG=			${RUMPPRG}
. ifndef CRUNCHEDPROG
.  if (${MKRUMP} != "no")
PROGS=			${RUMPPRG} rump.${RUMPPRG}
.  else
PROGS=			${RUMPPRG}
.  endif
.  if defined(SRCS)
.   if (${MKRUMP} != "no")
SRCS.rump.${PROG}:=	${SRCS} ${PROG}_rumpops.c ${RUMPSRCS}
.   endif
SRCS+=			${PROG}_hostops.c
.  else
SRCS=			${PROG}.c ${PROG}_hostops.c
.   if (${MKRUMP} != "no")
SRCS.rump.${PROG}=	${PROG}.c ${PROG}_rumpops.c ${RUMPSRCS}
.   endif
.  endif
.   if (${MKRUMP} != "no")
DPSRCS+=		${PROG}_rumpops.c ${RUMPSRCS}
LDADD.rump.${PROG}+=	-lrumpclient
DPADD.rump.${PROG}+=	${LIBRUMPCLIENT}
MAN.rump.${PROG}=	# defined but feeling empty
_RUMPINSTALL.rump.${PROG}=# defined
.   endif
. else # CRUNCHEDPROG
PROGS=			${PROG}
CPPFLAGS+=		-DCRUNCHOPS
. endif
.endif

.if defined(PROG)
_CCLINK?=	${CC} ${_CCLINKFLAGS}
.  if defined(MAN)
MAN.${PROG}=	${MAN}
_APPEND_MANS=	no
.  endif
.  if !defined(OBJS)
OBJS=		${OBJS.${PROG}}
.  endif
.  if defined(PROGNAME)
PROGNAME.${PROG}=	${PROGNAME}
.  endif
.  if defined(SRCS)
SRCS.${PROG}=	${SRCS}
_APPEND_SRCS=	no
.  endif
.endif

# Turn the single-program PROG and PROG_CXX variables into their multi-word
# counterparts, PROGS and PROGS_CXX.
.if !defined(RUMPPRG)
.  if defined(PROG_CXX) && !defined(PROGS_CXX)
PROGS_CXX=	${PROG_CXX}
.  elif defined(PROG) && !defined(PROGS)
PROGS=		${PROG}
.  endif
.endif

#
# Per-program definitions and targets.
#

_CCLINK.CDEFAULT= ${CC} ${_CCLINKFLAGS}
# Definitions specific to C programs.
.for _P in ${PROGS}
SRCS.${_P}?=	${_P}.c
_CCLINK.${_P}=	${CC} ${_CCLINKFLAGS}
_CFLAGS.${_P}=	${CFLAGS} ${CPUFLAGS}
_CPPFLAGS.${_P}=	${CPPFLAGS}
_COPTS.${_P}=	${COPTS}
.endfor

_CCLINK.CXXDEFAULT= ${CXX} ${_CCLINKFLAGS}
# Definitions specific to C++ programs.
.for _P in ${PROGS_CXX}
SRCS.${_P}?=	${_P}.cc
_CCLINK.${_P}=	${CXX} ${_CCLINKFLAGS}
.endfor

# Language-independent definitions.
.for _P in ${PROGS} ${PROGS_CXX}					# {

BINDIR.${_P}?=		${BINDIR}
PROGNAME.${_P}?=	${_P}

.if ${MKDEBUG} != "no" && !commands(${_P})
_PROGDEBUG.${_P}:=	${PROGNAME.${_P}}.debug
.endif

.if defined(PAXCTL_FLAGS)
PAXCTL_FLAGS.${_P}?= ${PAXCTL_FLAGS}
.endif

##### PROG specific flags.

_DPADD.${_P}=		${DPADD}    ${DPADD.${_P}}
_LDADD.${_P}=		${LDADD}    ${LDADD.${_P}}
_LDFLAGS.${_P}=		${LDFLAGS}  ${LDFLAGS.${_P}}
_LDSTATIC.${_P}=	${LDSTATIC} ${LDSTATIC.${_P}}

##### Build and install rules
.if !empty(_APPEND_SRCS:M[Yy][Ee][Ss])
SRCS+=		${SRCS.${_P}}	# For bsd.dep.mk
.endif

_YPSRCS.${_P}=	${SRCS.${_P}:M*.[ly]:C/\..$/.c/} ${YHEADER:D${SRCS.${_P}:M*.y:.y=.h}}

DPSRCS+=		${_YPSRCS.${_P}}
CLEANFILES+=		${_YPSRCS.${_P}}

.if !empty(SRCS.${_P}:N*.h:N*.sh:N*.fth)
OBJS.${_P}+=	${SRCS.${_P}:N*.h:N*.sh:N*.fth:R:S/$/.o/g}
LOBJS.${_P}+=	${LSRCS:.c=.ln} ${SRCS.${_P}:M*.c:.c=.ln}
.endif

.if defined(OBJS.${_P}) && !empty(OBJS.${_P})			# {
.NOPATH: ${OBJS.${_P}} ${_P} ${_YPSRCS.${_P}}

.if (defined(USE_COMBINE) && ${USE_COMBINE} != "no" && !commands(${_P}) \
   && (${_CCLINK.${_P}} == ${_CCLINK.CDEFAULT} \
       || ${_CCLINK.${_P}} == ${_CCLINK.CXXDEFAULT}) \
   && !defined(NOCOMBINE.${_P}) && !defined(NOCOMBINE))
.for f in ${SRCS.${_P}:N*.h:N*.sh:N*.fth:C/\.[yl]$/.c/g}
#_XFLAGS.$f := ${CPPFLAGS.$f:D1} ${CPUFLAGS.$f:D2} \
#     ${COPTS.$f:D3} ${OBJCOPTS.$f:D4} ${CXXFLAGS.$f:D5}
.if (${CPPFLAGS.$f:D1} == "1" || ${CPUFLAGS.$f:D2} == "2" \
     || ${COPTS.$f:D3} == "3" || ${OBJCOPTS.$f:D4} == "4" \
     || ${CXXFLAGS.$f:D5} == "5") \
    || ("${f:M*.[cyl]}" == "" || commands(${f:R:S/$/.o/}))
XOBJS.${_P}+=	${f:R:S/$/.o/}
.else
XSRCS.${_P}+=	${f}
NODPSRCS+=	${f}
.endif
.endfor

${_P}: .gdbinit ${LIBCRT0} ${XOBJS.${_P}} ${SRCS.${_P}} ${DPSRCS} ${LIBC} ${LIBCRTBEGIN} ${LIBCRTEND} ${_DPADD.${_P}}
	${_MKTARGET_LINK}
.if defined(DESTDIR)
	${_CCLINK.${_P}} -Wl,-nostdlib \
	    ${_LDFLAGS.${_P}} ${_LDSTATIC.${_P}} -o ${.TARGET} ${_PROGLDOPTS} \
	    -B${_GCC_CRTDIR}/ -B${DESTDIR}/usr/lib/ \
	    -MD --combine ${_CPPFLAGS.${_P}} ${_CFLAGS.${_P}} ${_COPTS.${_P}} \
	    ${XSRCS.${_P}:@.SRC.@${.ALLSRC:M*.c:M*${.SRC.}}@:O:u} ${XOBJS.${_P}} \
	    ${_LDADD.${_P}} -L${_GCC_LIBGCCDIR} -L${DESTDIR}/usr/lib
.else
	${_CCLINK.${_P}} ${_LDFLAGS.${_P}} ${_LDSTATIC.${_P}} -o ${.TARGET} ${_PROGLDOPTS} \
	    -MD --combine ${_CPPFLAGS.${_P}} ${_COPTS.${_P}}
	    ${XSRCS.${_P}:@.SRC.@${.ALLSRC:M*.c:M*${.SRC.}}@:O:u} ${XOBJS.${_P}} \
	    ${_LDADD.${_P}}
.endif	# defined(DESTDIR)
.if defined(CTFMERGE)
	${CTFMERGE} ${CTFMFLAGS} -o ${.TARGET} ${OBJS.${_P}}
.endif
.if defined(PAXCTL_FLAGS.${_P})
	${PAXCTL} ${PAXCTL_FLAGS.${_P}} ${.TARGET}
.endif
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

CLEANFILES+=	${_P}.d
.if exists(${_P}.d)
.include "${_P}.d"		# include -MD depend for program.
.endif
.else	# USE_COMBINE

${OBJS.${_P}} ${LOBJS.${_P}}: ${DPSRCS}

${_P}: .gdbinit ${LIBCRT0} ${OBJS.${_P}} ${LIBC} ${LIBCRTBEGIN} ${LIBCRTEND} ${_DPADD.${_P}}
.if !commands(${_P})
	${_MKTARGET_LINK}
	${_CCLINK.${_P}} \
	    ${_LDFLAGS.${_P}} ${_LDSTATIC.${_P}} -o ${.TARGET} \
	    ${OBJS.${_P}} ${_LDADD.${_P}} \
	    ${_PROGLDOPTS}
.if defined(CTFMERGE)
	${CTFMERGE} ${CTFMFLAGS} -o ${.TARGET} ${OBJS.${_P}}
.endif
.if defined(PAXCTL_FLAGS.${_P})
	${PAXCTL} ${PAXCTL_FLAGS.${_P}} ${.TARGET}
.endif
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif
.endif	# !commands(${_P})
.endif	# USE_COMBINE

${_P}.ro: ${OBJS.${_P}} ${_DPADD.${_P}}
	${_MKTARGET_LINK}
	${CC} ${LDFLAGS} -nostdlib -r -Wl,-dc -o ${.TARGET} ${OBJS.${_P}}

.if defined(_PROGDEBUG.${_P})
${_PROGDEBUG.${_P}}: ${_P}
	${_MKTARGET_CREATE}
	(  ${OBJCOPY} --only-keep-debug ${_P} ${_PROGDEBUG.${_P}} \
	&& ${OBJCOPY} --strip-debug -p -R .gnu_debuglink \
		--add-gnu-debuglink=${_PROGDEBUG.${_P}} ${_P} \
	) || (rm -f ${_PROGDEBUG.${_P}}; false)
.endif

.endif	# defined(OBJS.${_P}) && !empty(OBJS.${_P})			# }

.if !defined(MAN.${_P})
MAN.${_P}=	${_P}.1
.endif	# !defined(MAN.${_P})
.if !empty(_APPEND_MANS:M[Yy][Ee][Ss])
MAN+=		${MAN.${_P}}
.endif

realall: ${_P} ${_PROGDEBUG.${_P}}

CLEANFILES+= ${_P} ${_PROGDEBUG.${_P}}

.if defined(OBJS.${_P}) && !empty(OBJS.${_P})
CLEANFILES+= ${OBJS.${_P}} ${LOBJS.${_P}}
.endif

_PROG_INSTALL+=	proginstall-${_P}

.if !target(proginstall-${_P})						# {
proginstall-${_P}::	${DESTDIR}${BINDIR.${_P}}/${PROGNAME.${_P}} \
		${_PROGDEBUG.${_P}:D${DESTDIR}${DEBUGDIR}${BINDIR.${_P}}/${_PROGDEBUG.${_P}}}
.PRECIOUS:	${DESTDIR}${BINDIR.${_P}}/${PROGNAME.${_P}} \
		${_PROGDEBUG.${_P}:D${DESTDIR}${DEBUGDIR}${BINDIR.${_P}}/${_PROGDEBUG.${_P}}}

.if ${MKUPDATE} == "no"
.if defined(_RUMPINSTALL.${_P})
${DESTDIR}${BINDIR.${_P}}/${PROGNAME.${_P}}! ${_P} __progrumpinstall
.else
${DESTDIR}${BINDIR.${_P}}/${PROGNAME.${_P}}! ${_P} __proginstall
.endif
.if !defined(BUILD) && !make(all) && !make(${_P})
${DESTDIR}${BINDIR.${_P}}/${PROGNAME.${_P}}! .MADE
.endif
.if defined(_PROGDEBUG.${_P})
${DESTDIR}${DEBUGDIR}${BINDIR.${_P}}/${_PROGDEBUG.${_P}}! ${_PROGDEBUG.${_P}} __progdebuginstall
.if !defined(BUILD) && !make(all) && !make(${_P})
${DESTDIR}${DEBUGDIR}${BINDIR.${_P}}/${_PROGDEBUG.${_P}}! .MADE
.endif
.endif	#  define(_PROGDEBUG.${_P})
.else	# MKUPDATE != no
.if defined(_RUMPINSTALL.${_P})
${DESTDIR}${BINDIR.${_P}}/${PROGNAME.${_P}}: ${_P} __progrumpinstall
.else
${DESTDIR}${BINDIR.${_P}}/${PROGNAME.${_P}}: ${_P} __proginstall
.endif
.if !defined(BUILD) && !make(all) && !make(${_P})
${DESTDIR}${BINDIR.${_P}}/${PROGNAME.${_P}}: .MADE
.endif
.if defined(_PROGDEBUG.${_P})
${DESTDIR}${DEBUGDIR}${BINDIR.${_P}}/${_PROGDEBUG.${_P}}: ${_PROGDEBUG.${_P}} __progdebuginstall
.if !defined(BUILD) && !make(all) && !make(${_P})
${DESTDIR}${DEBUGDIR}${BINDIR.${_P}}/${_PROGDEBUG.${_P}}: .MADE
.endif
.endif	#  defined(_PROGDEBUG.${_P})
.endif	# MKUPDATE != no

.endif	# !target(proginstall-${_P})					# }

lint: lint-${_P}
lint-${_P}: ${LOBJS.${_P}}
.if defined(LOBJS.${_P}) && !empty(LOBJS.${_P})
	${LINT} ${LINTFLAGS} ${_LDFLAGS.${_P}:C/-L[  ]*/-L/Wg:M-L*} ${LOBJS.${_P}} ${_LDADD.${_P}}
.endif

.endfor # _P in ${PROGS} ${PROGS_CXX}					# }

.if defined(OBJS) && !empty(OBJS) && \
    (empty(PROGS) && empty(PROGS_CXX))
CLEANFILES+= ${OBJS} ${LOBJS}
.endif

.if !target(proginstall)
proginstall:: ${_PROG_INSTALL}
.endif
.PHONY:		proginstall



realall: ${SCRIPTS}
.if defined(SCRIPTS) && !target(scriptsinstall)				# {
SCRIPTSDIR?=${BINDIR}
SCRIPTSOWN?=${BINOWN}
SCRIPTSGRP?=${BINGRP}
SCRIPTSMODE?=${BINMODE}

scriptsinstall:: ${SCRIPTS:@S@${DESTDIR}${SCRIPTSDIR_${S}:U${SCRIPTSDIR}}/${SCRIPTSNAME_${S}:U${SCRIPTSNAME:U${S:T:R}}}@}
.PRECIOUS: ${SCRIPTS:@S@${DESTDIR}${SCRIPTSDIR_${S}:U${SCRIPTSDIR}}/${SCRIPTSNAME_${S}:U${SCRIPTSNAME:U${S:T:R}}}@}

__scriptinstall: .USE
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} \
	    -o ${SCRIPTSOWN_${.ALLSRC:T}:U${SCRIPTSOWN}} \
	    -g ${SCRIPTSGRP_${.ALLSRC:T}:U${SCRIPTSGRP}} \
	    -m ${SCRIPTSMODE_${.ALLSRC:T}:U${SCRIPTSMODE}} \
	    ${.ALLSRC} ${.TARGET}

.for S in ${SCRIPTS:O:u}
.if ${MKUPDATE} == "no"
${DESTDIR}${SCRIPTSDIR_${S}:U${SCRIPTSDIR}}/${SCRIPTSNAME_${S}:U${SCRIPTSNAME:U${S:T:R}}}! ${S} __scriptinstall
.if !defined(BUILD) && !make(all) && !make(${S})
${DESTDIR}${SCRIPTSDIR_${S}:U${SCRIPTSDIR}}/${SCRIPTSNAME_${S}:U${SCRIPTSNAME:U${S:T:R}}}! .MADE
.endif
.else
${DESTDIR}${SCRIPTSDIR_${S}:U${SCRIPTSDIR}}/${SCRIPTSNAME_${S}:U${SCRIPTSNAME:U${S:T:R}}}: ${S} __scriptinstall
.if !defined(BUILD) && !make(all) && !make(${S})
${DESTDIR}${SCRIPTSDIR_${S}:U${SCRIPTSDIR}}/${SCRIPTSNAME_${S}:U${SCRIPTSNAME:U${S:T:R}}}: .MADE
.endif
.endif
.endfor
.endif									# }

.if !target(scriptsinstall)
scriptsinstall::
.endif
.PHONY:		scriptsinstall

##### Pull in related .mk logic
LINKSOWN?= ${BINOWN}
LINKSGRP?= ${BINGRP}
LINKSMODE?= ${BINMODE}
.include <bsd.man.mk>
.include <bsd.nls.mk>
.include <bsd.files.mk>
.include <bsd.inc.mk>
.include <bsd.links.mk>
.include <bsd.sys.mk>
.include <bsd.dep.mk>
.include <bsd.clang-analyze.mk>
.include <bsd.clean.mk>

${TARGETS}:	# ensure existence

.endif	# HOSTPROG
