# The ACPI driver is compilable only with a "gnu" type compiler
PROG=	acpi

SRCS= \
	acpi.c \
	pci.c \
	osminixxf.c

ACPICA_SRCS= \
	dsargs.c \
	dscontrol.c \
	dsfield.c \
	dsinit.c \
	dsmethod.c \
	dsmthdat.c \
	dsobject.c \
	dsopcode.c \
	dsutils.c \
	dswexec.c \
	dswload.c \
	dswload2.c \
	dswscope.c \
	dswstate.c \
	evevent.c \
	evglock.c \
	evgpe.c \
	evgpeblk.c \
	evgpeinit.c \
	evgpeutil.c \
	evhandler.c \
	evmisc.c \
	evregion.c \
	evrgnini.c \
	evsci.c \
	evxface.c \
	evxfevnt.c \
	evxfregn.c \
	exconfig.c \
	exconvrt.c \
	excreate.c \
	exdebug.c \
	exdump.c \
	exfield.c \
	exfldio.c \
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
	exutils.c \
	hwacpi.c \
	hwesleep.c \
	hwgpe.c \
	hwpci.c \
	hwregs.c \
	hwsleep.c \
	hwtimer.c \
	hwvalid.c \
	hwxface.c \
	nsaccess.c \
	nsalloc.c \
	nsarguments.c \
	nsconvert.c \
	nsdump.c \
	nsdumpdv.c \
	nseval.c \
	nsinit.c \
	nsload.c \
	nsnames.c \
	nsobject.c \
	nsparse.c \
	nspredef.c \
	nsprepkg.c \
	nsrepair.c \
	nsrepair2.c \
	nssearch.c \
	nsutils.c \
	nswalk.c \
	nsxfeval.c \
	nsxfname.c \
	nsxfobj.c \
	psargs.c \
	psloop.c \
	psobject.c \
	psopcode.c \
	psopinfo.c \
	psparse.c \
	psscope.c \
	pstree.c \
	psutils.c \
	pswalk.c \
	psxface.c \
	rsaddr.c \
	rscalc.c \
	rscreate.c \
	rsdump.c \
	rsinfo.c \
	rsio.c \
	rsirq.c \
	rslist.c \
	rsmemory.c \
	rsmisc.c \
	rsserial.c \
	rsutils.c \
	rsxface.c \
	tbdata.c \
	tbfadt.c \
	tbfind.c \
	tbinstal.c \
	tbprint.c \
	tbutils.c \
	tbxface.c \
	tbxfload.c \
	tbxfroot.c \
	utaddress.c \
	utalloc.c \
	utbuffer.c \
	utcache.c \
	utclib.c \
	utcopy.c \
	utdebug.c \
	utdecode.c \
	utdelete.c \
	uterror.c \
	uteval.c \
	utexcep.c \
	utglobal.c \
	uthex.c \
	utids.c \
	utinit.c \
	utlock.c \
	utmath.c \
	utmisc.c \
	utmutex.c \
	utobject.c \
	utosi.c \
	utownerid.c \
	utpredef.c \
	utresrc.c \
	utstate.c \
	utstring.c \
	uttrack.c \
	utxface.c \
	utxferror.c \
	utxfinit.c

.PATH:  ${.CURDIR}/utilities ${.CURDIR}/namespace ${.CURDIR}/resources \
	${.CURDIR}/dispatcher ${.CURDIR}/events ${.CURDIR}/executer \
	${.CURDIR}/tables ${.CURDIR}/parser ${.CURDIR}/hardware

SRCS+=${ACPICA_SRCS} 

DPADD+=	${LIBSYS}
LDADD+=	-lsys

CPPFLAGS += -I${.CURDIR}/include
CFLAGS += -DACPI_LIBRARY
CFLAGS += -std=c99

.include <minix.service.mk>
