# The ACPI driver is compilable only with a "gnu" type compiler
PROG=	acpi

SRCS= \
	acpi.c \
	pci.c \
	osminixxf.c

ACPICA_SRCS= \
	utxface.c \
	utdelete.c \
	utobject.c \
	utcopy.c \
	utids.c \
	utglobal.c \
	utalloc.c \
	utstate.c \
	utdebug.c \
	uteval.c \
	utcache.c \
	utlock.c \
	uttrack.c \
	utclib.c \
	utinit.c \
	utmisc.c \
	utmutex.c \
	utresrc.c \
	utmath.c \
	nsparse.c \
	nsutils.c \
	nsaccess.c \
	nsrepair.c \
	nsalloc.c \
	nswalk.c \
	nsnames.c \
	nssearch.c \
	nsxfname.c \
	nseval.c \
	nsxfobj.c \
	nsobject.c \
	nspredef.c \
	nsdumpdv.c \
	nsload.c \
	nsdump.c \
	nsinit.c \
	nsrepair2.c \
	nsxfeval.c \
	rsirq.c \
	rsmisc.c \
	rsio.c \
	rsaddr.c \
	rsutils.c \
	rscreate.c \
	rsdump.c \
	rscalc.c \
	rslist.c \
	rsxface.c \
	rsinfo.c \
	rsmemory.c \
	dsobject.c \
	dsmethod.c \
	dsopcode.c \
	dswscope.c \
	dsinit.c \
	dswexec.c \
	dswstate.c \
	dsfield.c \
	dsmthdat.c \
	dsutils.c \
	dswload.c \
	evxfregn.c \
	evrgnini.c \
	evregion.c \
	evgpeblk.c \
	evxface.c \
	evmisc.c \
	evgpeutil.c \
	evgpeinit.c \
	evxfevnt.c \
	evevent.c \
	evsci.c \
	evgpe.c \
	exoparg1.c \
	exutils.c \
	excreate.c \
	exstore.c \
	exregion.c \
	exoparg6.c \
	exprep.c \
	exmutex.c \
	exnames.c \
	exoparg2.c \
	exdump.c \
	exmisc.c \
	exresolv.c \
	exdebug.c \
	exstoren.c \
	exoparg3.c \
	exstorob.c \
	exconfig.c \
	exresop.c \
	exfield.c \
	exfldio.c \
	exresnte.c \
	exconvrt.c \
	exsystem.c \
	tbfadt.c \
	tbxface.c \
	tbxfroot.c \
	tbutils.c \
	tbinstal.c \
	tbfind.c \
	psutils.c \
	psargs.c \
	psloop.c \
	psparse.c \
	pswalk.c \
	pstree.c \
	psopcode.c \
	psxface.c \
	psscope.c \
	hwregs.c \
	hwsleep.c \
	hwvalid.c \
	hwgpe.c \
	hwacpi.c \
	hwtimer.c \
	hwxface.c

.PATH:  ${.CURDIR}/utilities ${.CURDIR}/namespace ${.CURDIR}/resources \
	${.CURDIR}/dispatcher ${.CURDIR}/events ${.CURDIR}/executer \
	${.CURDIR}/tables ${.CURDIR}/parser ${.CURDIR}/hardware

SRCS+=${ACPICA_SRCS} 

DPADD+=	${LIBSYS}
LDADD+=	-lsys

CPPFLAGS += -I${.CURDIR}/include
CFLAGS += -DACPI_LIBRARY
CFLAGS += -std=c99

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
