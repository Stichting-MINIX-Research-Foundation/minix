#	$NetBSD: bsd.hostprog.mk,v 1.74 2014/12/01 01:34:30 erh Exp $
#	@(#)bsd.prog.mk	8.2 (Berkeley) 4/2/94

.include <bsd.init.mk>
.include <bsd.sys.mk>

##### Basic targets

##### Default values
LIBATF_C?=	/usr/lib/libatf-c.a
LIBATF_CXX?=	/usr/lib/libatf-c++.a
LIBBLUETOOTH?=	/usr/lib/libbluetooth.a
LIBBZ2?=	/usr/lib/libbz2.a
LIBC?=		/usr/lib/libc.a
LIBC_PIC?=	/usr/lib/libc_pic.a
LIBC_SO?=	/usr/lib/libc.so
LIBCOMPAT?=	/usr/lib/libcompat.a
LIBCRYPT?=	/usr/lib/libcrypt.a
LIBCURSES?=	/usr/lib/libcurses.a
LIBDBM?=	/usr/lib/libdbm.a
LIBDES?=	/usr/lib/libdes.a
LIBEDIT?=	/usr/lib/libedit.a
LIBEVENT?=	/usr/lib/libevent.a
LIBEVENT_OPENSSL?=	/usr/lib/libevent_openssl.a
LIBEVENT_PTHREADS?=	/usr/lib/libevent_pthreads.a
LIBEXPAT?=	/usr/lib/libexpat.a
LIBFETCH?=	/usr/lib/libfetch.a
LIBFORM?=	/usr/lib/libform.a
LIBGCC?=	/usr/lib/libgcc.a
LIBGNUMALLOC?=	/usr/lib/libgnumalloc.a
LIBINTL?=	/usr/lib/libintl.a
LIBIPSEC?=	/usr/lib/libipsec.a
LIBKVM?=	/usr/lib/libkvm.a
LIBL?=		/usr/lib/libl.a
LIBLUTOK?=	/usr/lib/liblutok.a
LIBLZMA?=	/usr/lib/liblzma.a
LIBM?=		/usr/lib/libm.a
LIBMAGIC?=	/usr/lib/libmagic.a
LIBMENU?=	/usr/lib/libmenu.a
LIBMP?=		/usr/lib/libmp.a
LIBNTP?=	/usr/lib/libntp.a
LIBOBJC?=	/usr/lib/libobjc.a
LIBP2K?=	/usr/lib/libp2k.a
LIBPC?=		/usr/lib/libpc.a
LIBPCAP?=	/usr/lib/libpcap.a
LIBPCI?=	/usr/lib/libpci.a
LIBPLOT?=	/usr/lib/libplot.a
LIBPOSIX?=	/usr/lib/libposix.a
LIBPUFFS?=	/usr/lib/libpuffs.a
LIBQUOTA?=	/usr/lib/libquota.a
LIBRESOLV?=	/usr/lib/libresolv.a
LIBRPCSVC?=	/usr/lib/librpcsvc.a
LIBRUMP?=	/usr/lib/librump.a
LIBRUMPCLIENT?=	/usr/lib/librumpclient.a
LIBRUMPNET?=	/usr/lib/librumpnet.a
LIBRUMPUSER?=	/usr/lib/librumpuser.a
LIBRUMPVFS?=	/usr/lib/librumpvfs.a
LIBSKEY?=	/usr/lib/libskey.a
LIBSQLITE3?=	/usr/lib/libsqlite3.a
LIBSSP?=	/usr/lib/libssp.a
LIBSTDCXX?=	/usr/lib/libstdc++.a
LIBSUPCXX?=	/usr/lib/libsupc++.a
LIBTERMINFO?=	/usr/lib/libterminfo.a
LIBUTIL?=	/usr/lib/libutil.a
LIBWRAP?=	/usr/lib/libwrap.a
LIBUKFS?=	/usr/lib/libukfs.a
LIBY?=		/usr/lib/liby.a
LIBZ?=		/usr/lib/libz.a

##### rump file system modules
LIBRUMPFS_CD9660FS?=	/usr/lib/librumpfs_cd9660fs.a
LIBRUMPFS_EFS?=		/usr/lib/librumpfs_efs.a
LIBRUMPFS_EXT2FS?=	/usr/lib/librumpfs_ext2fs.a
LIBRUMPFS_FFS?=		/usr/lib/librumpfs_ffs.a
LIBRUMPFS_HFS?=		/usr/lib/librumpfs_hfs.a
LIBRUMPFS_LFS?=		/usr/lib/librumpfs_lfs.a
LIBRUMPFS_MSDOSFS?=	/usr/lib/librumpfs_msdosfs.a
LIBRUMPFS_NTFS?=	/usr/lib/librumpfs_ntfs.a
LIBRUMPFS_SYSPUFFS?=	/usr/lib/librumpfs_syspuffs.a
LIBRUMPFS_TMPFS?=	/usr/lib/librumpfs_tmpfs.a
LIBRUMPFS_UDF?=		/usr/lib/librumpfs_udf.a
LIBRUMPFS_UFS?=		/usr/lib/librumpfs_ufs.a

MKDEP_SUFFIXES?=	.lo .ln .d

# Override these:
INSTALL:=	${INSTALL:NSTRIP=*}
MKDEP:=		${HOST_MKDEP}
MKDEPCXX:=	${HOST_MKDEPCXX}

.if ${TOOLCHAIN_MISSING} == "no" || defined(EXTERNAL_TOOLCHAIN)
OBJHOSTMACHINE=	# set
.endif

##### Build rules
.if defined(HOSTPROG_CXX)
HOSTPROG=	${HOSTPROG_CXX}
.endif

.if defined(HOSTPROG)
SRCS?=		${HOSTPROG}.c

_YHPSRCS=	${SRCS:M*.[ly]:C/\..$/.c/} ${YHEADER:D${SRCS:M*.y:.y=.h}}
DPSRCS+=	${_YHPSRCS}
CLEANFILES+=	${_YHPSRCS}

.if !empty(SRCS:N*.h:N*.sh)
OBJS+=		${SRCS:N*.h:N*.sh:R:S/$/.lo/g}
LOBJS+=		${LSRCS:.c=.ln} ${SRCS:M*.c:.c=.ln}
.endif

.if defined(OBJS) && !empty(OBJS)
.NOPATH: ${OBJS} ${HOSTPROG} ${_YHPSRCS}

${OBJS} ${LOBJS}: ${DPSRCS}
${HOSTPROG}: ${OBJS} ${DPADD}
	${_MKTARGET_LINK}
	${HOST_LINK.c} ${HOST_LDSTATIC} -o ${.TARGET} ${OBJS} ${LDADD}
.if !empty(.MAKE.OS:M*CYGWIN*)
	${HOST_SH} ${NETBSDSRCDIR}/tools/binstall/mkmanifest ${HOSTPROG}
.endif


.endif	# defined(OBJS) && !empty(OBJS)

.if !defined(MAN)
MAN=	${HOSTPROG}.1
.endif	# !defined(MAN)
.endif	# defined(HOSTPROG)

realall: ${HOSTPROG}

CLEANFILES+= a.out [Ee]rrs mklog core *.core ${HOSTPROG} ${OBJS} ${LOBJS}

beforedepend:
CFLAGS:=	${HOST_CFLAGS}
CPPFLAGS:=	${HOST_CPPFLAGS:N-Wp,-iremap,*}

lint: ${LOBJS}
.if defined(LOBJS) && !empty(LOBJS)
	${LINT} ${LINTFLAGS} ${LDFLAGS:C/-L[  ]*/-L/Wg:M-L*} ${LOBJS} ${LDADD}
.endif

##### Pull in related .mk logic
LINKSMODE?= ${BINMODE}
.include <bsd.man.mk>
.include <bsd.nls.mk>
.include <bsd.files.mk>
.include <bsd.inc.mk>
.include <bsd.links.mk>
.include <bsd.dep.mk>
.include <bsd.clean.mk>

${TARGETS}:	# ensure existence

# Override YACC/LEX rules so nbtool_config.h can be forced as the 1st include
.l.c:
	${_MKTARGET_LEX}
	${LEX.l} -o${.TARGET} ${.IMPSRC}
	echo '#if HAVE_NBTOOL_CONFIG_H' > ${.TARGET}.1
	echo '#include "nbtool_config.h"' >> ${.TARGET}.1
	echo '#endif' >> ${.TARGET}.1
	cat ${.TARGET} >> ${.TARGET}.1
	mv ${.TARGET}.1 ${.TARGET}
.y.c:
	${_MKTARGET_YACC}
	${YACC.y} -o ${.TARGET} ${.IMPSRC}
	echo '#if HAVE_NBTOOL_CONFIG_H' > ${.TARGET}.1
	echo '#include "nbtool_config.h"' >> ${.TARGET}.1
	echo '#endif' >> ${.TARGET}.1
	cat ${.TARGET} >> ${.TARGET}.1
	mv ${.TARGET}.1 ${.TARGET}
