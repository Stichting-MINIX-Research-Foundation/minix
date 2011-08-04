#	$NetBSD: Makefile,v 1.126 2010/03/01 21:10:13 darran Exp $

NETBSDINCSDIR= /usr/include
INCSDIR= /usr/include/sys

.include <bsd.sys.mk>


# Present only in minix
INCS+= ioc_net.h statfs.h

INCS+=	ansi.h atomic.h \
	bitops.h bswap.h \
	cdefs.h cdefs_aout.h cdefs_elf.h ctype_bits.h ctype_inline.h \
	dirent.h exec_elf.h \
	endian.h errno.h \
	fcntl.h fd_set.h featuretest.h file.h \
	float_ieee754.h gcq.h gmon.h hash.h \
	ieee754.h inttypes.h ioctl.h ipc.h \
	localedef.h \
	md4.h md5.h \
	mman.h mount.h \
	null.h \
	param.h poll.h ptrace.h \
	ptree.h \
	queue.h \
	rbtree.h resource.h rmd160.h \
	select.h sha1.h \
	sha2.h siginfo.h signal.h sigtypes.h \
	reboot.h sem.h shm.h socket.h \
	stat.h statvfs.h \
	sysctl.h stdint.h \
	syslimits.h syslog.h \
	termios.h time.h times.h \
	tree.h ttycom.h ttydefaults.h \
	types.h \
	ucontext.h ucred.h uio.h un.h unistd.h utsname.h uuid.h \
	wait.h

INCSYMLINKS=\
	sys/fcntl.h ${NETBSDINCSDIR}/fcntl.h \
	sys/poll.h ${NETBSDINCSDIR}/poll.h \
	sys/stdint.h ${NETBSDINCSDIR}/stdint.h \
	sys/syslog.h ${NETBSDINCSDIR}/syslog.h \
	sys/termios.h ${NETBSDINCSDIR}/termios.h \
	sys/rmd160.h ${NETBSDINCSDIR}/rmd160.h \
	sys/sha1.h ${NETBSDINCSDIR}/sha1.h \
	sys/sha2.h ${NETBSDINCSDIR}/sha2.h \
	sys/md4.h ${NETBSDINCSDIR}/md4.h \
	sys/md5.h ${NETBSDINCSDIR}/md5.h \
	sys/exec_elf.h ${NETBSDINCSDIR}/elf.h \


#INCSYMLINKS+=	../soundcard.h ${INCSDIR}/soundcard.h

namei: namei.src gennameih.awk
	${TOOL_AWK} -f gennameih.awk < namei.src

.include <bsd.own.mk>
.include "${MINIXSRCDIR}/common/include/sys/Makefile.inc"

.include <bsd.files.mk>
.include <bsd.kinc.mk>
