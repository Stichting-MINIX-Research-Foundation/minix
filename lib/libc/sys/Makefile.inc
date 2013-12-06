#	$NetBSD: Makefile.inc,v 1.221 2013/11/09 13:05:59 njoly Exp $
#	@(#)Makefile.inc	8.3 (Berkeley) 10/24/94

.if defined(__MINIX)
.PATH: ${.CURDIR}/sys
.else
# sys sources
.PATH: ${ARCHDIR}/sys ${.CURDIR}/sys

# modules with non-default implementations on at least one architecture:
SRCS+=	__clone.S __vfork14.S brk.S exect.S fork.S pipe.S \
	getcontext.S \
	ptrace.S sbrk.S shmat.S \
	syscall.S __syscall.S __clone.S cerror.S

# other sources shared with the kernel, used in syscalls
SRCS+=	cpuset.c

_LSRC+=	Lint___clone.c Lint___vfork14.c Lint_brk.c Lint_clone.c \
	Lint_getcontext.c Lint_exect.c Lint_pipe.c Lint_ptrace.c \
	Lint_sbrk.c Lint___sigaction14.c Lint_syscall.c Lint___syscall.c \
	LintSysNormal.c LintSysNoerr.c LintSysPseudoNoerr.c

.if ${MKLINT} != "no"
LSRCS+=		${_LSRC}
DPSRCS+=	${_LSRC:MLintSys*.c}
CLEANFILES+=	${_LSRC:MLintSys*.c}
.endif

# glue to offer userland wrappers for some syscalls
SRCS+=	posix_fadvise.c posix_madvise.c sched.c sigqueue.c sigtimedwait.c \
	sigwait.c sigwaitinfo.c statvfs.c swapon.c semctl.c 

# glue to provide compatibility between GCC 1.X and 2.X and for compat
# with old syscall interfaces.
GLUE+= ftruncate.c lseek.c mmap.c pread.c preadv.c pwrite.c \
	pwritev.c truncate.c ntp_adjtime.c mknodat.c

GLUE50+= adjtime.c clock_settime.c settimeofday.c

# 'glue' files might .c or .S depending on the architecture
.for glue in ${GLUE}
. if exists(${glue:.c=.S})
# Build the ASM glue file
SRCS+=${glue:.c=.S}
. else
.  if exists(__${glue:.c=.S})
SRCS+=__${glue:.c=.S} ${glue}
.  else
# Build the C glue file
SRCS+= ${glue}
# and an asm entry for __<syscall>
ASM_GLUE+= tmp_${glue:.c=.S}
.  endif
. endif
.endfor

# 'glue' files might .c or .S depending on the architecture
.for glue in ${GLUE50}
. if exists(${glue:.c=.S})
# Build the ASM glue file
SRCS+=${glue:.c=.S}
. else
.  if exists(__${glue:.c=}50.S)
SRCS+=__${glue:.c=}50.S ${glue}
.  else
# Build the C glue file
SRCS+= ${glue}
# and an asm entry for __<syscall>
ASM_GLUE+= tmp___${glue:.c=}50.S
.  endif
. endif
.endfor

# namespace purity wrappers
SRCS+=	_brk.c _fork.c _getcontext.c _pipe.c _sbrk.c

# modules with default implementations on all architectures:
ASM=	access.S acct.S \
	bind.S \
	chdir.S chflags.S chmod.S chown.S chroot.S clock_nanosleep.S \
		__clock_getres50.S __clock_gettime50.S \
	dup.S dup2.S dup3.S \
	extattrctl.S \
		extattr_delete_fd.S extattr_delete_file.S \
		extattr_delete_link.S extattr_get_fd.S extattr_get_file.S \
		extattr_get_link.S extattr_list_fd.S extattr_list_file.S \
		extattr_list_link.S extattr_set_fd.S extattr_set_file.S \
		extattr_set_link.S \
	setxattr.S getxattr.S listxattr.S removexattr.S \
		lsetxattr.S lgetxattr.S llistxattr.S lremovexattr.S \
		fsetxattr.S fgetxattr.S flistxattr.S fremovexattr.S \
	faccessat.S fchdir.S fchflags.S fchmod.S fchmodat.S fchown.S \
		fchownat.S fchroot.S fexecve.S __fhopen40.S __fhstat50.S \
		__fhstatvfs140.S fktrace.S flock.S fpathconf.S __fstat50.S \
		fstatvfs1.S fstatat.S  __futimes50.S futimens.S \
	__getcwd.S __getdents30.S __getfh30.S getvfsstat.S getgroups.S\
		__getitimer50.S __getlogin.S getpeername.S getpgid.S getpgrp.S \
		getpriority.S getrlimit.S __getrusage50.S getsid.S \
		getsockname.S getsockopt.S __gettimeofday50.S \
	ioctl.S \
	kqueue.S kqueue1.S ktrace.S \
		_ksem_close.S _ksem_destroy.S _ksem_getvalue.S _ksem_init.S \
		_ksem_post.S _ksem_timedwait.S _ksem_trywait.S _ksem_unlink.S \
		_ksem_wait.S _ksem_open.S \
	lchflags.S lchmod.S lchown.S lfs_bmapv.S lfs_markv.S lfs_segclean.S \
		__lfs_segwait50.S link.S linkat.S listen.S __lstat50.S \
		__lutimes50.S _lwp_create.S _lwp_exit.S _lwp_kill.S \
		_lwp_self.S _lwp_wait.S _lwp_unpark.S \
		_lwp_unpark_all.S _lwp_suspend.S _lwp_continue.S \
		_lwp_wakeup.S _lwp_detach.S _lwp_setprivate.S \
		_lwp_setname.S _lwp_getname.S _lwp_ctl.S \
	madvise.S mincore.S minherit.S mkdir.S mkdirat.S mkfifo.S mkfifoat.S \
		__mknod50.S mlock.S mlockall.S modctl.S __mount50.S \
		mprotect.S __msgctl50.S msgget.S munlock.S munlockall.S \
		munmap.S \
	nfssvc.S __ntp_gettime50.S \
	openat.S \
	paccept.S pathconf.S pipe2.S pmc_get_info.S pmc_control.S \
		__posix_chown.S __posix_fchown.S __posix_lchown.S \
		__posix_rename.S profil.S \
	__quotactl.S \
	rasctl.S readlinkat.S reboot.S recvfrom.S recvmmsg.S recvmsg.S rename.S \
		renameat.S revoke.S rmdir.S \
	semconfig.S semget.S semop.S \
		sendmmsg.S sendmsg.S sendto.S setegid.S seteuid.S \
		setgid.S setgroups.S __setitimer50.S __setlogin.S setpgid.S \
		setpriority.S \
		setregid.S setreuid.S setrlimit.S setsid.S setsockopt.S \
		setuid.S __shmctl50.S shmdt.S shmget.S shutdown.S \
		sigqueueinfo.S \
		__sigaltstack14.S __sigpending14.S __sigaction_sigtramp.S \
		____sigtimedwait50.S __socket30.S socketpair.S __stat50.S \
		statvfs1.S swapctl.S symlink.S symlinkat.S __sysctl.S \
	timer_create.S timer_delete.S __timer_gettime50.S timer_getoverrun.S \
		____semctl50.S __timer_settime50.S \
	umask.S undelete.S unlink.S unlinkat.S unmount.S __utimes50.S \
		utimensat.S utrace.S uuidgen.S vadvise.S

# sa compat syscalls
SAASM+=	sa_register.S sa_stacks.S sa_enable.S sa_setconcurrency.S sa_yield.S \
		sa_preempt.S

# modules with potentially non default implementations
ASM_MD=	_lwp_getprivate.S mremap.S 

.for f in ${ASM_MD}
. if !exists(${f})
   ASM+=	tmp_${f}
. else
   SRCS+=	${f}
. endif
.endfor

WEAKASM= accept.S __aio_suspend50.S close.S connect.S execve.S \
	fcntl.S fdatasync.S fsync.S fsync_range.S __kevent50.S \
	kill.S mq_receive.S mq_send.S __mq_timedreceive50.S __mq_timedsend50.S \
	msgrcv.S msgsnd.S __msync13.S  __nanosleep50.S open.S poll.S \
	__pollts50.S __pselect50.S read.S readlink.S \
	readv.S _sched_setparam.S _sched_getparam.S _sched_setaffinity.S \
	_sched_getaffinity.S sched_yield.S \
	__select50.S setcontext.S __sigprocmask14.S __sigsuspend14.S sysarch.S \
	__wait450.S write.S writev.S

NOERR=	getegid.S geteuid.S getgid.S getpid.S getppid.S getuid.S \
	issetugid.S posix_spawn.S sync.S __posix_fadvise50.S

PSEUDONOERR=	_exit.S

SRCS+=		${ASM} ${WEAKASM} ${NOERR} ${PSEUDONOERR} ${ASM_GLUE}
CLEANFILES+=	${ASM} ${WEAKASM} ${NOERR} ${PSEUDONOERR} ${ASM_GLUE}

ASMDEPS=	${.CURDIR}/sys/Makefile.inc ${ARCHDIR}/SYS.h \
		${DESTDIR}/usr/include/sys/syscall.h

_LINTASM=	${ASM:Nsa_*.S} ${WEAKASM:Nsa_*.S}

${ASM}: ${ASMDEPS}
	${_MKTARGET_CREATE}
	printf '#include "SYS.h"\nRSYSCALL(${.PREFIX:S/tmp_//})\n' >${.TARGET}

${ASM_GLUE}: ${ASMDEPS}
	${_MKTARGET_CREATE}
	printf '#include "SYS.h"\nPSEUDO(${.PREFIX:S/tmp_/__/},${.PREFIX:S/tmp_//})\n' >${.TARGET}

${WEAKASM}: ${ASMDEPS}
	${_MKTARGET_CREATE}
	printf '#include "SYS.h"\nWSYSCALL(${.PREFIX},_sys_${.PREFIX})\nWEAK_ALIAS(_${.PREFIX},_sys_${.PREFIX})\n' >${.TARGET}

${SAASM}: ${ASMDEPS}
	${_MKTARGET_CREATE}
	printf '#include "SYS.h"\n_SYSCALL(${.PREFIX:S/tmp_//},${.PREFIX:S/tmp_//:S/sa_/compat_60_sa_/})\n' >${.TARGET}

${NOERR}: ${ASMDEPS}
	${_MKTARGET_CREATE}
	printf '#include "SYS.h"\nRSYSCALL_NOERROR(${.PREFIX})\n' >${.TARGET}

${PSEUDONOERR}: ${ASMDEPS}
	${_MKTARGET_CREATE}
	printf '#include "SYS.h"\nPSEUDO_NOERROR(${.PREFIX},${.PREFIX:S/_//})\n' >${.TARGET}
	# add an _Exit alias to _exit
	printf 'STRONG_ALIAS(${.PREFIX:S/e/E/},${.PREFIX})\n' >>${.TARGET}

LintSysNormal.c: ${LIBCDIR}/sys/makelintstub \
    ${DESTDIR}/usr/include/sys/syscall.h ${_LINTASM}
	${_MKTARGET_CREATE}
	CPP=${CPP:Q} ${HOST_SH} ${LIBCDIR}/sys/makelintstub -o ${.TARGET} \
	    -s ${DESTDIR}/usr/include/sys/syscall.h ${_LINTASM:S/tmp_//}

LintSysNoerr.c: ${LIBCDIR}/sys/makelintstub \
    ${DESTDIR}/usr/include/sys/syscall.h
	${_MKTARGET_CREATE}
	CPP=${CPP:Q} ${HOST_SH} ${LIBCDIR}/sys/makelintstub -o ${.TARGET} -n \
	    -s ${DESTDIR}/usr/include/sys/syscall.h ${NOERR}

LintSysPseudoNoerr.c: ${LIBCDIR}/sys/makelintstub \
    ${DESTDIR}/usr/include/sys/syscall.h
	${_MKTARGET_CREATE}
	CPP=${CPP:Q} ${HOST_SH} ${LIBCDIR}/sys/makelintstub -o ${.TARGET} -p \
	    -s ${DESTDIR}/usr/include/sys/syscall.h ${PSEUDONOERR}
.endif # !defined(__MINIX)

.if !defined(__MINIX)
MAN+=	accept.2 access.2 acct.2 adjtime.2 bind.2 brk.2 chdir.2 \
	chflags.2 chmod.2 chown.2 chroot.2 clock_settime.2 clone.2 close.2 \
	connect.2 dup.2 execve.2 _exit.2 extattr_get_file.2 \
	fcntl.2 fdatasync.2 fhopen.2 \
	flock.2 fork.2 fsync.2 getcontext.2 getdents.2 \
	getfh.2 getvfsstat.2 getgid.2 getgroups.2 \
	getitimer.2 getlogin.2 getpeername.2 getpgrp.2 getpid.2 \
	getpriority.2 getrlimit.2 getrusage.2 getsid.2 getsockname.2 \
	getsockopt.2 gettimeofday.2 getuid.2 intro.2 ioctl.2 issetugid.2 \
	kill.2 kqueue.2 ktrace.2 \
	lfs_bmapv.2 lfs_markv.2 lfs_segclean.2 lfs_segwait.2 \
	link.2 listen.2 lseek.2 \
	_lwp_create.2 _lwp_ctl.2 _lwp_detach.2 _lwp_exit.2 \
	_lwp_self.2 _lwp_setname.2 \
	_lwp_suspend.2 _lwp_wakeup.2 _lwp_wait.2 _lwp_kill.2 \
	_lwp_getname.2 _lwp_getprivate.2 \
	_lwp_park.2 _lwp_unpark.2 _lwp_unpark_all.2 \
	mkdir.2 mkfifo.2 mknod.2 \
	madvise.2 mincore.2 minherit.2 mlock.2 mlockall.2 mmap.2 modctl.2 \
	mount.2 \
	mprotect.2 mremap.2 msgctl.2 msgget.2 msgrcv.2 msgsnd.2 msync.2 \
	munmap.2 nanosleep.2 nfssvc.2 ntp_adjtime.2 open.2 pathconf.2 pipe.2 
.else
MAN+=	adjtime.2 clock_settime.2 getvfsstat.2 pipe.2 getrusage.2 reboot.2
.endif # !defined(__MINIX)
.if !defined(__MINIX)
MAN+=	pmc_control.2 poll.2 posix_fadvise.2 profil.2 ptrace.2 __quotactl.2 \
	rasctl.2 read.2 readlink.2 \
	reboot.2 recv.2 rename.2 revoke.2 rmdir.2 \
	select.2 semctl.2 \
	semget.2 semop.2 send.2 setgroups.2 setpgid.2 setregid.2 \
	setreuid.2 setsid.2 setuid.2 shmat.2 shmctl.2 shmget.2 \
	shutdown.2 sigaction.2 sigaltstack.2 sigpending.2 \
	sigprocmask.2 sigqueue.2 sigstack.2 sigsuspend.2 sigtimedwait.2 \
	socket.2 \
	socketpair.2 stat.2 statvfs.2 swapctl.2 swapon.3 symlink.2 \
	sync.2 sysarch.2 syscall.2 timer_create.2 timer_delete.2 \
	timer_settime.2 truncate.2 umask.2 undelete.2 \
	unlink.2 utimes.2 utrace.2 uuidgen.2 vfork.2 wait.2 write.2

MLINKS+=_exit.2 _Exit.2
MLINKS+=brk.2 sbrk.2
MLINKS+=clone.2 __clone.2
MLINKS+=dup.2 dup2.2
MLINKS+=dup.2 dup3.2
MLINKS+=chdir.2 fchdir.2
MLINKS+=chflags.2 fchflags.2 chflags.2 lchflags.2
MLINKS+=chmod.2 fchmod.2 chmod.2 lchmod.2
MLINKS+=chown.2 fchown.2 chown.2 lchown.2
MLINKS+=chroot.2 fchroot.2
.else
MLINKS+=clock_settime.2 clock_gettime.2
MLINKS+=clock_settime.2 clock_getres.2
.endif # !defined(__MINIX)
.if !defined(__MINIX)
MLINKS+=extattr_get_file.2 extattr_set_file.2 \
	extattr_get_file.2 extattr_delete_file.2 \
	extattr_get_file.2 extattr_list_file.2 \
	extattr_get_file.2 extattr_get_fd.2 \
	extattr_get_file.2 extattr_set_fd.2 \
	extattr_get_file.2 extattr_delete_fd.2 \
	extattr_get_file.2 extattr_list_fd.2 \
	extattr_get_file.2 extattr_get_link.2 \
	extattr_get_file.2 extattr_set_link.2 \
	extattr_get_file.2 extattr_delete_link.2 \
	extattr_get_file.2 extattr_list_link.2
MLINKS+=access.2 faccessat.2
MLINKS+=chmod.2 fchmodat.2
MLINKS+=chown.2 fchownat.2
MLINKS+=fhopen.2 fhstat.2 fhopen.2 fhstatvfs.2 fhopen.2 fhstatvfs1.2
MLINKS+=stat.2 fstatat.2
MLINKS+=fsync.2 fsync_range.2
MLINKS+=getcontext.2 setcontext.2
MLINKS+=getgid.2 getegid.2
MLINKS+=getitimer.2 setitimer.2
MLINKS+=getlogin.2 setlogin.2
MLINKS+=getlogin.2 getlogin_r.2
MLINKS+=getpgrp.2 getpgid.2
MLINKS+=getpid.2 getppid.2
MLINKS+=getpriority.2 setpriority.2
MLINKS+=getrlimit.2 setrlimit.2
MLINKS+=getsockopt.2 setsockopt.2
MLINKS+=gettimeofday.2 settimeofday.2
MLINKS+=getuid.2 geteuid.2
MLINKS+=intro.2 errno.2
MLINKS+=kqueue.2 kevent.2
MLINKS+=ktrace.2 fktrace.2
MLINKS+=lseek.2 seek.2
MLINKS+=link.2 linkat.2
MLINKS+=_lwp_suspend.2 _lwp_continue.2
MLINKS+=_lwp_getprivate.2 _lwp_setprivate.2
MLINKS+=madvise.2 posix_madvise.2
MLINKS+=mlock.2 munlock.2
MLINKS+=mlockall.2 munlockall.2
MLINKS+=mkdir.2 mkdirat.2
MLINKS+=mkfifo.2 mkfifoat.2
MLINKS+=mknod.2 mknodat.2
MLINKS+=mount.2 unmount.2
MLINKS+=open.2 openat.2
MLINKS+=ntp_adjtime.2 ntp_gettime.2
MLINKS+=pathconf.2 fpathconf.2
MLINKS+=pmc_control.2 pmc_get_info.2
MLINKS+=poll.2 pollts.2
MLINKS+=read.2 readv.2 read.2 pread.2 read.2 preadv.2
MLINKS+=readlink.2 readlinkat.2
MLINKS+=recv.2 recvfrom.2 recv.2 recvmsg.2 recv.2 recvmmsg.2
MLINKS+=rename.2 renameat.2
MLINKS+=select.2 pselect.2
MLINKS+=send.2 sendmsg.2 send.2 sendto.2 send.2 sendmmsg.2
MLINKS+=setpgid.2 setpgrp.2
MLINKS+=setuid.2 setegid.2 setuid.2 seteuid.2 setuid.2 setgid.2
MLINKS+=shmat.2 shmdt.2
MLINKS+=symlink.2 symlinkat.2
MLINKS+=timer_settime.2 timer_gettime.2 timer_settime.2 timer_getoverrun.2
MLINKS+=sigqueue.2 sigqueueinfo.2
MLINKS+=sigtimedwait.2 sigwaitinfo.2
MLINKS+=sigtimedwait.2 sigwait.2
MLINKS+=stat.2 fstat.2 stat.2 lstat.2
MLINKS+=statvfs.2 fstatvfs.2
MLINKS+=statvfs.2 statvfs1.2
MLINKS+=statvfs.2 fstatvfs1.2
MLINKS+=syscall.2 __syscall.2
MLINKS+=truncate.2 ftruncate.2
MLINKS+=unlink.2 unlinkat.2
MLINKS+=utimes.2 futimes.2 utimes.2 lutimes.2
MLINKS+=utimes.2 utimens.2 utimes.2 futimens.2 utimes.2 lutimens.2
MLINKS+=utimes.2 utimensat.2
MLINKS+=wait.2 wait3.2 wait.2 wait4.2 wait.2 waitpid.2
MLINKS+=write.2 writev.2 write.2 pwrite.2 write.2 pwritev.2
.else
MLINKS+=pipe.2 pipe2.2
MAN+=	statvfs.2
MLINKS+=statvfs.2 fstatvfs.2
MLINKS+=statvfs.2 statvfs1.2
MLINKS+=statvfs.2 fstatvfs1.2
.endif # !defined(__MINIX)
.if !defined(__MINIX)
MLINKS+=accept.2 paccept.2
MLINKS+=nanosleep.2 clock_nanosleep.2
.endif # !defined(__MINIX)
