.PATH:	${.CURDIR}/sys-minix

SRCS+= 	accept.c access.c bind.c brk.c sbrk.c m_closefrom.c getsid.c \
	chdir.c chmod.c fchmod.c chown.c fchown.c chroot.c close.c \
	connect.c dup.c dup2.c execve.c fcntl.c flock.c fpathconf.c fork.c \
	fstatfs.c fstatvfs.c fsync.c ftruncate.c getdents.c getegid.c getgid.c \
	getgroups.c getitimer.c setitimer.c __getlogin.c getpeername.c \
	getpgrp.c getpid.c getppid.c priority.c getrlimit.c getsockname.c \
	getsockopt.c setsockopt.c gettimeofday.c geteuid.c getuid.c \
	ioctl.c issetugid.c kill.c link.c listen.c loadname.c lseek.c \
	minix_rs.c mkdir.c mkfifo.c mknod.c mmap.c mount.c nanosleep.c \
	open.c pathconf.c pipe.c poll.c pread.c ptrace.c pwrite.c \
	read.c readlink.c reboot.c recvfrom.c recvmsg.c rename.c\
	rmdir.c select.c sem.c sendmsg.c sendto.c setgroups.c setsid.c \
	setgid.c settimeofday.c setuid.c shmat.c shmctl.c shmget.c stime.c \
	vectorio.c shutdown.c sigaction.c sigpending.c sigreturn.c sigsuspend.c\
	sigprocmask.c socket.c socketpair.c stat.c statvfs.c symlink.c \
	sync.c syscall.c sysuname.c truncate.c umask.c unlink.c write.c \
	_exit.c _ucontext.c environ.c __getcwd.c vfork.c sizeup.c init.c

# Minix specific syscalls.
SRCS+= cprofile.c lseek64.c sprofile.c _mcontext.c

.include "${ARCHDIR}/sys-minix/Makefile.inc"
