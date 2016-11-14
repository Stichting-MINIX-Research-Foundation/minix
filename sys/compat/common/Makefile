#	$NetBSD: Makefile,v 1.54 2015/09/07 03:44:19 uebayasi Exp $

LIB=		compat
NOPIC=		# defined
LLIBS=		# defined

CPPFLAGS=	${COMPATCPPFLAGS} ${SKIP_AUTODEP::=}

.PATH.c: ${COMPATDIR}

# Common compatibility code, used by all emulations
SRCS=	compat_exec.c compat_util.c

# Compatibility code for 4.3BSD
SRCS+=	kern_exit_43.c kern_info_43.c kern_resource_43.c kern_sig_43.c \
	tty_43.c uipc_syscalls_43.c vfs_syscalls_43.c vm_43.c if_43.c

# Compatibility code for NetBSD 0.9
SRCS+=	kern_info_09.c

# Compatibility code for NetBSD 1.2
SRCS+=	kern_xxx_12.c vfs_syscalls_12.c vm_12.c

# Compatibility code for NetBSD 1.3
SRCS+=	kern_sig_13.c

# Compatibility code for NetBSD 1.6
SRCS+=	kern_sig_16.c

# Compatibility code for NetBSD 1.4
SRCS+=	sysv_msg_14.c sysv_shm_14.c sysv_sem_14.c rtsock_14.c

# Compatibility code for NetBSD 2.0
SRCS+=	vfs_syscalls_20.c

# Compatibility code for NetBSD 3.0
SRCS+=	kern_time_30.c vfs_syscalls_30.c uipc_syscalls_30.c

# Compatibility code for NetBSD 4.0
SRCS+=	vfs_syscalls_40.c uipc_syscalls_40.c

# Compatibility code for NetBSD 5.0
SRCS+=	kern_50.c kern_time_50.c kern_select_50.c rndpseudo_50.c rtsock_50.c \
	sysv_ipc_50.c sysv_msg_50.c sysv_sem_50.c sysv_shm_50.c \
	vfs_syscalls_50.c  uipc_syscalls_50.c

# Compatibility code for NetBSD 6.0
SRCS+=	kern_sa_60.c tty_60.c kern_time_60.c

# really, all machines where sizeof(int) != sizeof(long) (LP64)
.if (${MACHINE_ARCH} != "alpha" && ${MACHINE_ARCH} != "sparc64" \
  && ${MACHINE_ARCH} != "x86_64")
SRCS+=	kern_ipc_10.c
.endif

# only needed during build
libinstall::

.include <bsd.own.mk>
.undef DESTDIR
.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS:O}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `NM=${NM} ${LORDER} ${OBJS} | ${TSORT}`
	@echo done

lib${LIB}.po:: ${POBJS:O}
	@echo building profiled ${LIB} library
	@rm -f lib${LIB}.po
	@${LD} -r -o lib${LIB}.po `NM=${NM} ${LORDER} ${POBJS} | ${TSORT}`
	@echo done

showsources: ${SRCS}
	@echo ${.ALLSRC}
