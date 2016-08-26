.PATH:	${NETBSDSRCDIR}/minix/lib/libc/sys

SRCS+= 	accept.c access.c adjtime.c bind.c brk.c sbrk.c m_closefrom.c getsid.c \
	chdir.c chmod.c fchmod.c chown.c fchown.c chroot.c close.c \
	clock_getres.c clock_gettime.c clock_settime.c \
	connect.c dup.c dup2.c execve.c fcntl.c flock.c fpathconf.c fork.c \
	fstatfs.c fstatvfs.c fsync.c ftruncate.c gcov_flush_sys.c getdents.c \
	getegid.c getgid.c \
	getgroups.c getitimer.c setitimer.c __getlogin.c getpeername.c \
	getpgrp.c getpid.c getppid.c priority.c getrlimit.c getsockname.c \
	getsockopt.c setsockopt.c gettimeofday.c geteuid.c getuid.c \
	getvfsstat.c \
	ioctl.c issetugid.c kill.c link.c listen.c loadname.c lseek.c \
	minix_rs.c mkdir.c mkfifo.c mknod.c mmap.c mount.c nanosleep.c \
	open.c pathconf.c pipe.c poll.c posix_spawn.c pread.c ptrace.c pwrite.c \
	read.c readlink.c reboot.c recvfrom.c recvmsg.c rename.c \
	rmdir.c select.c sem.c sendmsg.c sendto.c setgroups.c setsid.c \
	setgid.c settimeofday.c setuid.c shmat.c shmctl.c shmget.c stime.c \
	vectorio.c shutdown.c sigaction.c sigpending.c sigreturn.c sigsuspend.c\
	sigprocmask.c socket.c socketpair.c stat.c statvfs.c svrctl.c \
	symlink.c \
	sync.c syscall.c truncate.c umask.c unlink.c \
	wait4.c write.c \
	utimensat.c utimes.c futimes.c lutimes.c futimens.c \
	_exit.c _ucontext.c environ.c __getcwd.c vfork.c sizeup.c init.c \
	getrusage.c setrlimit.c setpgid.c __sysctl.c

# Minix specific syscalls / utils.
SRCS+= kernel_utils.c sprofile.c stack_utils.c _mcontext.c

# Emulation for missing lchown/lchmod/lchflags
OBJS+= lchflags.o lchmod.o lchown.o
lchflags.go lchflags.o lchflags.pico lchflags.bc: ${NETBSDSRCDIR}/tools/compat/lchflags.c
lchmod.go lchmod.o lchmod.pico lchmod.bc: ${NETBSDSRCDIR}/tools/compat/lchmod.c
lchown.go lchown.o lchown.pico lchown.bc: ${NETBSDSRCDIR}/tools/compat/lchown.c

.include "${NETBSDSRCDIR}/minix/lib/libc/arch/${ARCHSUBDIR}/sys/Makefile.inc"
.include "${NETBSDSRCDIR}/minix/lib/libc/arch/${ARCHSUBDIR}/Makefile.inc"
