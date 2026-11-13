# $NetBSD: Makefile,v 1.24 2019/01/10 10:04:43 martin Exp $

.if (${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64" || ${MACHINE_ARCH} == "aarch64")
PROG=	iasl
.endif
BINDIR=	/usr/bin
MAN=	iasl.8
WARNS=	0
TOPDIR=	${NETBSDSRCDIR}/sys/external/bsd/acpica/dist/compiler

.include <bsd.own.mk>

CPPFLAGS+=	-I${TOPDIR}/../include -I${TOPDIR} -I.
CPPFLAGS+=	-DACPI_ASL_COMPILER -DACPI_USE_ALTERNATE_TIMEOUT
.if ${MKREPRO:Uno} == "yes"
CPPFLAGS+=	-DACPI_REPRO
.endif
LDADD+=		-ll -ly
DPADD+=		${LIBL} ${LIBY}

DPSRCS= aslcompilerparse.y aslcompilerlex.c aslcompilerparse.c \
	dtparserparse.c dtparserlex.c prparserparse.c prparserlex.c

.PATH:	${TOPDIR}
SRCS+= \
	adfile.c \
	aslallocate.c \
	aslanalyze.c \
	aslascii.c \
	aslbtypes.c \
	aslcache.c \
	aslcodegen.c \
	aslcompile.c \
	aslcompilerlex.c \
	aslcompilerparse.c \
	asldebug.c \
	aslerror.c \
	aslexternal.c \
	aslfileio.c \
	aslfiles.c \
	aslfold.c \
	aslhelp.c \
	aslhex.c \
	asllength.c \
	asllisting.c \
	asllistsup.c \
	aslload.c \
	asllookup.c \
	aslmain.c \
	aslmap.c \
	aslmapenter.c \
	aslmapoutput.c \
	aslmaputils.c \
	aslmessages.c \
	aslmethod.c \
	aslnamesp.c \
	asloffset.c \
	aslopcodes.c \
	asloperands.c \
	aslopt.c \
	asloptions.c \
	aslparseop.c \
	aslpld.c \
	aslpredef.c \
	aslprepkg.c \
	aslprintf.c \
	aslprune.c \
	aslresource.c \
	aslrestype1.c \
	aslrestype1i.c \
	aslrestype2.c \
	aslrestype2d.c \
	aslrestype2e.c \
	aslrestype2q.c \
	aslrestype2s.c \
	aslrestype2w.c \
	aslstartup.c \
	aslstubs.c \
	asltransform.c \
	asltree.c \
	aslutils.c \
	asluuid.c \
	aslwalks.c \
	aslxref.c \
	aslxrefout.c \
	cvcompiler.c \
	cvdisasm.c \
	cvparser.c \
	dtcompile.c \
	dtexpress.c \
	dtfield.c \
	dtio.c \
	dtparserlex.c \
	dtparserparse.c \
	dtsubtable.c \
	dttable.c \
	dttable1.c \
	dttable2.c \
	dttemplate.c \
	dtutils.c \
	prexpress.c \
	prmacros.c \
	prparserlex.c \
	prparserparse.c \
	prscan.c \
	prutils.c

.PATH: ${TOPDIR}/../common
SRCS+= \
	acfileio.c \
	adisasm.c \
	adwalk.c \
	ahids.c \
	ahpredef.c \
	ahtable.c \
	ahuuids.c \
	cmfsize.c \
	dmextern.c \
	dmrestag.c \
	dmswitch.c \
	dmtable.c \
	dmtables.c \
	dmtbdump.c \
	dmtbdump1.c \
	dmtbdump2.c \
	dmtbdump3.c \
	dmtbinfo.c \
	dmtbinfo1.c \
	dmtbinfo2.c \
	dmtbinfo3.c \
	getopt.c

.PATH: ${TOPDIR}/../debugger
SRCS+=  dbfileio.c

.PATH: ${TOPDIR}/../disassembler
SRCS+= \
	dmbuffer.c \
	dmcstyle.c \
	dmdeferred.c \
	dmnames.c \
	dmopcode.c \
	dmresrc.c \
	dmresrcl.c \
	dmresrcl2.c \
	dmresrcs.c \
	dmutils.c \
	dmwalk.c

.PATH: ${TOPDIR}/../dispatcher
SRCS+= \
	dsargs.c \
	dscontrol.c \
	dsfield.c \
	dsobject.c \
	dsopcode.c \
	dspkginit.c \
	dsutils.c \
	dswexec.c \
	dswload.c \
	dswload2.c \
	dswscope.c \
	dswstate.c

.PATH: ${TOPDIR}/../executer
SRCS+= \
	exconvrt.c \
	excreate.c \
	exconcat.c \
	exdump.c \
	exmisc.c \
	exmutex.c \
	exnames.c \
	exoparg1.c \
	exoparg2.c \
	exoparg3.c \
	exoparg6.c \
	exprep.c \
	exregion.c \
	exresnte.c \
	exresolv.c \
	exresop.c \
	exstore.c \
	exstoren.c \
	exstorob.c \
	exsystem.c \
	exutils.c

.PATH: ${TOPDIR}/../parser
SRCS+= \
	psargs.c \
	psloop.c \
	psobject.c \
	psopcode.c \
	psopinfo.c \
	psparse.c \
	psscope.c \
	pstree.c \
	psutils.c \
	pswalk.c

.PATH: ${TOPDIR}/../tables
SRCS+=	\
	tbfadt.c \
	tbdata.c \
	tbinstal.c \
	tbprint.c \
	tbutils.c \
	tbxface.c \
	tbxfload.c

.PATH: ${TOPDIR}/../namespace
SRCS+= \
	nsaccess.c \
	nsalloc.c \
	nsdump.c \
	nsnames.c \
	nsobject.c \
	nsparse.c \
	nssearch.c \
	nsutils.c \
	nswalk.c \
	nsxfobj.c \

.PATH: ${TOPDIR}/../utilities
SRCS+= \
	utaddress.c \
	utalloc.c \
	utascii.c \
	utbuffer.c \
	utcache.c \
	utcopy.c \
	utdebug.c \
	utdecode.c \
	utdelete.c \
	uterror.c \
	utexcep.c \
	utglobal.c \
	uthex.c \
	utinit.c \
	utlock.c \
	utmath.c \
	utmisc.c \
	utmutex.c \
	utnonansi.c \
	utobject.c \
	utosi.c \
	utownerid.c \
	utpredef.c \
	utresdecode.c \
	utresrc.c \
	utstate.c \
	utstring.c \
	utstrsuppt.c \
	utstrtoul64.c \
	utuuid.c \
	utxface.c \
	utxferror.c

#	utprint.c 

.PATH: ${TOPDIR}/../os_specific/service_layers
SRCS+=	osunixxf.c

YHEADER=	# Generate headers
YFLAGS+=-v	# Require debugging
LFLAGS+=-i	# Case insensitivive parser


# Prevent rules from firing.
asltypes.h: asltypes.y
	@true
asltypes.c: asltypes.y
	@true

aslparser.y: asltokens.y asltypes.y aslrules.y aslsupport.y

YPREFIX.aslcompilerparse.y=AslCompiler
aslcompilerparse.y: aslparser.y
	${TOOL_M4} -P -I${TOPDIR} ${.ALLSRC} > ${.TARGET}.tmp.${.MAKE.PID} \
	&& mv ${.TARGET}.tmp.${.MAKE.PID} ${.TARGET}

aslcompiler.y.h: aslcompilerparse.h
	ln -sf ${.ALLSRC} ${.TARGET}

LPREFIX.aslcompilerlex.l=AslCompiler
aslcompilerlex.l: ${TOPDIR}/aslcompiler.l
	ln -sf ${.ALLSRC} ${.TARGET}


YPREFIX.dtparserparse.y=DtParser
dtparserparse.y: ${TOPDIR}/dtparser.y
	ln -sf ${.ALLSRC} ${.TARGET}

dtparser.y.h: dtparserparse.h
	ln -sf ${.ALLSRC} ${.TARGET}

LPREFIX.dtparserlex.l=DtParser
dtparserlex.l: ${TOPDIR}/dtparser.l
	ln -sf ${.ALLSRC} ${.TARGET}


YPREFIX.prparserparse.y=PrParser
prparserparse.y: ${TOPDIR}/prparser.y
	ln -sf ${.ALLSRC} ${.TARGET}

prparser.y.h: prparserparse.h
	ln -sf ${.ALLSRC} ${.TARGET}

LPREFIX.prparserlex.l=PrParser
prparserlex.l: ${TOPDIR}/prparser.l
	ln -sf ${.ALLSRC} ${.TARGET}

CLEANFILES+=	aslcompilerparse.y  aslcompilerparse.c	\
		aslcompilerparse.h			\
		aslcompiler.y.h aslcompilerparse.output \
		aslcompilerlex.l aslcompilerlex.c	\
							\
		dtparserparse.y dtparserparse.c		\
		dtparserparse.h				\
		dtparserlex.l dtparserlex.c		\
		dtparser.y.h dtparserparse.output	\
							\
		prparserparse.y prparserparse.c		\
		prparserparse.h				\
		prparserlex.l prparserlex.c		\
		prparser.y.h prparserparse.output	\
							\
		y.output y.tab.h

.include <bsd.prog.mk>

${__DPSRCS.d} ${OBJS}: prparser.y.h dtparser.y.h aslcompiler.y.h
