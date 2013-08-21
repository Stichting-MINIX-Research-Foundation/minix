#	$NetBSD: Makefile,v 1.9 2011/09/29 09:44:08 dholland Exp $

.include <bsd.own.mk>

.PATH:	${X11SRCDIR.xorg-cf-files}

FILESDIR=${X11LIBDIR}/config

FILES=	Amoeba.cf DGUX.cf FreeBSD.cf Imake.cf Imake.rules Imake.tmpl \
	Library.tmpl Mips.cf Motif.rules Motif.tmpl NetBSD.cf Oki.cf \
	OpenBSD.cf QNX4.cf QNX4.rules Server.tmpl ServerLib.tmpl \
	Threads.tmpl Win32.cf Win32.rules WinLib.tmpl X11.rules \
	X11.tmpl apollo.cf bsd.cf bsdLib.rules bsdLib.tmpl bsdi.cf \
	cde.rules cde.tmpl convex.cf cray.cf cygwin.cf cygwin.rules \
	cygwin.tmpl darwin.cf darwinLib.rules darwinLib.tmpl \
	fujitsu.cf generic.cf gnu.cf gnuLib.rules gnuLib.tmpl hp.cf \
	hpLib.rules hpLib.tmpl ibm.cf ibmLib.rules ibmLib.tmpl \
	isc.cf linux.cf lnxLib.rules lnxLib.tmpl lnxdoc.rules \
	lnxdoc.tmpl luna.cf lynx.cf macII.cf mach.cf minix.cf \
	moto.cf ncr.cf nec.cf necLib.rules necLib.tmpl noop.rules \
	nto.cf nto.rules oldlib.rules os2.cf os2.rules os2Lib.rules \
	os2Lib.tmpl osf1.cf osfLib.rules osfLib.tmpl pegasus.cf \
	sco.cf sco5.cf scoLib.rules sequent.cf sgi.cf sgiLib.rules \
	sgiLib.tmpl sony.cf sun.cf sunLib.rules sunLib.tmpl \
	sv3Lib.rules sv3Lib.tmpl sv4Lib.rules sv4Lib.tmpl svr3.cf \
	svr4.cf ultrix.cf usl.cf x386.cf xf86.rules \
	xf86.tmpl xf86site.def xfree86.cf xorg.cf xorgversion.def

# Note: acts as '#define InstallEmptyHostDef'
#
FILES+=	date.def host.def xorgsite.def

# Handle site.def specificly because it is provided in dist
#
FILES+=				netbsd.site.def
FILESNAME_netbsd.site.def=	site.def
CLEANFILES+=			netbsd.site.def

netbsd.site.def:	site.def.in
	${_MKTARGET_CREATE}
	${TOOL_SED} -e 's,@prefix@,${X11ROOTDIR},' <${.ALLSRC} |\
	   ${TOOL_SED} -e 's,@FONTROOTDIR@,${X11ROOTDIR}/share/fonts/X11,' |\
	   ${TOOL_SED} -e 's,@ENCODINGSDIR@,\$${FONTROOTDIR}/encodings,' \
	   >${.TARGET}
realall:	netbsd.site.def

.include <bsd.prog.mk>
