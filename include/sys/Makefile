#	$NetBSD: Makefile,v 1.143 2012/02/19 21:06:57 rmind Exp $

.include <bsd.sys.mk>

INCSDIR= /usr/include/sys

# Present only in minix
INCS+= ioc_net.h statfs.h

INCS+=	ansi.h atomic.h \
	bitops.h bswap.h \
	cdefs.h cdefs_aout.h cdefs_elf.h ctype_bits.h ctype_inline.h \
	dirent.h \
	endian.h errno.h \
	fcntl.h fd_set.h featuretest.h file.h \
	float_ieee754.h gcq.h gmon.h hash.h \
	ieee754.h inttypes.h ioctl.h ipc.h \
	localedef.h lock.h \
	md4.h md5.h \
	mman.h mount.h \
	null.h \
	param.h poll.h ptrace.h \
	ptree.h \
	queue.h \
	rbtree.h resource.h rmd160.h \
	select.h sha1.h \
	sha2.h siginfo.h signal.h sigtypes.h \
	ras.h reboot.h sem.h shm.h socket.h \
	stat.h statvfs.h \
	sysctl.h stdint.h \
	syslimits.h syslog.h \
	termios.h time.h times.h \
	tree.h ttycom.h ttydefaults.h \
	types.h \
	ucontext.h ucred.h uio.h un.h unistd.h utsname.h uuid.h \
	extattr.h \
	wait.h exec.h

# Present only in minix
INCS+=	ioc_net.h statfs.h
INCS+=	elf64.h elf_common.h elf_generic.h \
	ioc_block.h ioc_fbd.h ioc_file.h ioc_tape.h ioc_disk.h \
	ioc_memory.h ioc_sound.h ioc_tty.h \
	kbdio.h mtio.h svrctl.h video.h vm.h procfs.h elf_core.h exec_elf.h \
	disk.h dkio.h ioccom.h mutex.h iostat.h disklabel.h disklabel_gpt.h \
	bootblock.h dkbad.h

INCSYMLINKS=\
	sys/fcntl.h /usr/include/fcntl.h \
	sys/poll.h /usr/include/poll.h \
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
