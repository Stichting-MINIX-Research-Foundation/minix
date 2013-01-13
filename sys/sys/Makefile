#	$NetBSD: Makefile,v 1.143 2012/02/19 21:06:57 rmind Exp $

.include <bsd.sys.mk>

INCSDIR= /usr/include/sys

INCS=	ansi.h atomic.h \
	bitops.h bootblock.h bswap.h \
	cdefs.h cdefs_aout.h \
	cdefs_elf.h condvar.h \
	ctype_bits.h ctype_inline.h \
	\
	dirent.h \
	disk.h disklabel.h disklabel_acorn.h disklabel_gpt.h \
	dkbad.h dkio.h \
	endian.h errno.h exec.h \
	exec_elf.h extattr.h \
	fcntl.h fd_set.h featuretest.h file.h  \
	float_ieee754.h fstypes.h gcq.h gmon.h hash.h \
	ieee754.h inttypes.h ioccom.h ioctl.h iostat.h ipc.h \
	\
	lwp.h \
	localedef.h lock.h \
	md4.h md5.h \
	mman.h mount.h mtio.h mutex.h \
	null.h \
	param.h poll.h \
	ptrace.h ptree.h \
	queue.h \
	ras.h rbtree.h reboot.h resource.h rmd160.h \
	rwlock.h \
	select.h sem.h sha1.h \
	sha2.h shm.h siginfo.h signal.h sigtypes.h \
	socket.h specificdata.h \
	stat.h \
	statvfs.h sysctl.h stdarg.h stdint.h \
	syslimits.h syslog.h \
	termios.h time.h times.h \
	tls.h tree.h ttycom.h \
	ttydefaults.h types.h \
	ucontext.h ucred.h uio.h un.h unistd.h utsname.h uuid.h \
	\
	wait.h

INCSYMLINKS=\
	sys/exec_elf.h /usr/include/elf.h \
	sys/fcntl.h /usr/include/fcntl.h \
	sys/poll.h /usr/include/poll.h \
	sys/stdarg.h /usr/include/stdarg.h \
	sys/stdint.h /usr/include/stdint.h \
	sys/syslog.h /usr/include/syslog.h \
	sys/termios.h /usr/include/termios.h \
	sys/rmd160.h /usr/include/rmd160.h \
	sys/sha1.h /usr/include/sha1.h \
	sys/sha2.h /usr/include/sha2.h \
	sys/md4.h /usr/include/md4.h \
	sys/md5.h /usr/include/md5.h

#INCSYMLINKS+=	../soundcard.h ${INCSDIR}/soundcard.h

namei: namei.src gennameih.awk
	${TOOL_AWK} -f gennameih.awk < namei.src

.include <bsd.kinc.mk>
