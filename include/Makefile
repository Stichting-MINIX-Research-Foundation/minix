#	$NetBSD: Makefile,v 1.141 2015/07/29 00:10:26 christos Exp $
#	@(#)Makefile	8.2 (Berkeley) 1/4/94

# Doing a make includes builds /usr/include

NOOBJ=		# defined

# Missing: mp.h

INCS=	a.out.h aio.h ar.h assert.h atomic.h \
	bitstring.h bm.h cdbw.h complex.h cpio.h ctype.h \
	db.h dirent.h disktab.h dlfcn.h err.h errno.h fenv.h fmtmsg.h fnmatch.h \
	fstab.h fts.h ftw.h getopt.h glob.h grp.h ifaddrs.h iconv.h \
	inttypes.h iso646.h kvm.h langinfo.h libgen.h \
	limits.h link.h link_aout.h link_elf.h locale.h \
	login_cap.h lwp.h malloc.h math.h md2.h \
	memory.h mntopts.h monetary.h mpool.h mqueue.h \
	ndbm.h netconfig.h netdb.h netgroup.h nlist.h nl_types.h nsswitch.h \
	paths.h pwd.h quota.h randomid.h ranlib.h re_comp.h regex.h regexp.h \
	resolv.h res_update.h rmt.h sched.h search.h semaphore.h setjmp.h \
	string.h sgtty.h signal.h spawn.h stab.h stddef.h stdio.h \
	stdlib.h strings.h stringlist.h struct.h sysexits.h tar.h time.h \
	ttyent.h tzfile.h ucontext.h ulimit.h unistd.h util.h utime.h utmp.h \
	utmpx.h uuid.h vis.h wchar.h wctype.h wordexp.h
INCS+=	arpa/ftp.h arpa/inet.h arpa/nameser.h arpa/nameser_compat.h \
	arpa/telnet.h arpa/tftp.h
INCS+=	protocols/dumprestore.h protocols/routed.h protocols/rwhod.h \
	protocols/talkd.h protocols/timed.h

INCS+=	hesiod.h

INCS+=	rpcsvc/yp_prot.h rpcsvc/ypclnt.h

INCS+=  ssp/ssp.h ssp/stdio.h ssp/string.h ssp/strings.h ssp/unistd.h

.if (${MACHINE_ARCH} != "vax")
INCS+=	ieeefp.h
.endif

.include <bsd.own.mk>


INCSDIR=	/usr/include

.if defined(__MINIX)
# RPC is not compiled in the libc. This include also needs
# rpcgen, which can be compiled if needed.
SUBDIR+=	../minix/include
.else
SUBDIR=		rpc
.endif # defined(__MINIX)
SUBDIR+=	../common/include/prop
SUBDIR+=	../common/include/ppath

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
