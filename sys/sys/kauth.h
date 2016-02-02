/* $NetBSD: kauth.h,v 1.73 2015/10/06 22:13:39 christos Exp $ */

/*-
 * Copyright (c) 2005, 2006 Elad Efrat <elad@NetBSD.org>  
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is based on Apple TN2127, available online at
 * http://developer.apple.com/technotes/tn2005/tn2127.html
 */

#ifndef _SYS_KAUTH_H_
#define	_SYS_KAUTH_H_

#include <secmodel/secmodel.h> /* for secmodel_t type */
#include <sys/stat.h> /* for modes */

struct uucred;
struct ki_ucred;
struct ki_pcred;
struct proc;
struct tty;
struct vnode;
struct cwdinfo;

/* Types. */
typedef struct kauth_scope     *kauth_scope_t;
typedef struct kauth_listener  *kauth_listener_t;
typedef uint32_t		kauth_action_t;
typedef int (*kauth_scope_callback_t)(kauth_cred_t, kauth_action_t,
				      void *, void *, void *, void *, void *);
typedef	struct kauth_key       *kauth_key_t;

#ifdef __KAUTH_PRIVATE	/* For the debugger */
/* 
 * Credentials.
 *
 * A subset of this structure is used in kvm(3) (src/lib/libkvm/kvm_proc.c)
 * and should be synchronized with this structure when the update is
 * relevant.
 */
struct kauth_cred {
	/*
	 * Ensure that the first part of the credential resides in its own
	 * cache line.  Due to sharing there aren't many kauth_creds in a
	 * typical system, but the reference counts change very often.
	 * Keeping it separate from the rest of the data prevents false
	 * sharing between CPUs.
	 */
	u_int cr_refcnt;		/* reference count */
#if COHERENCY_UNIT > 4
	uint8_t cr_pad[COHERENCY_UNIT - 4];
#endif
	uid_t cr_uid;			/* user id */
	uid_t cr_euid;			/* effective user id */
	uid_t cr_svuid;			/* saved effective user id */
	gid_t cr_gid;			/* group id */
	gid_t cr_egid;			/* effective group id */
	gid_t cr_svgid;			/* saved effective group id */
	u_int cr_ngroups;		/* number of groups */
	gid_t cr_groups[NGROUPS];	/* group memberships */
	specificdata_reference cr_sd;	/* specific data */
};
#endif

/*
 * Possible return values for a listener.
 */
#define	KAUTH_RESULT_ALLOW	0	/* allow access */
#define	KAUTH_RESULT_DENY	1	/* deny access */
#define	KAUTH_RESULT_DEFER	2	/* let others decide */

/*
 * Scopes.
 */
#define	KAUTH_SCOPE_GENERIC	"org.netbsd.kauth.generic"
#define	KAUTH_SCOPE_SYSTEM	"org.netbsd.kauth.system"
#define	KAUTH_SCOPE_PROCESS	"org.netbsd.kauth.process"
#define	KAUTH_SCOPE_NETWORK	"org.netbsd.kauth.network"
#define	KAUTH_SCOPE_MACHDEP	"org.netbsd.kauth.machdep"
#define	KAUTH_SCOPE_DEVICE	"org.netbsd.kauth.device"
#define	KAUTH_SCOPE_CRED	"org.netbsd.kauth.cred"
#define	KAUTH_SCOPE_VNODE	"org.netbsd.kauth.vnode"

/*
 * Generic scope - actions.
 */
enum {
	KAUTH_GENERIC_UNUSED1=1,
	KAUTH_GENERIC_ISSUSER,
};

/*
 * System scope - actions.
 */
enum {
	KAUTH_SYSTEM_ACCOUNTING=1,
	KAUTH_SYSTEM_CHROOT,
	KAUTH_SYSTEM_CHSYSFLAGS,
	KAUTH_SYSTEM_CPU,
	KAUTH_SYSTEM_DEBUG,
	KAUTH_SYSTEM_FILEHANDLE,
	KAUTH_SYSTEM_MKNOD,
	KAUTH_SYSTEM_MOUNT,
	KAUTH_SYSTEM_PSET,
	KAUTH_SYSTEM_REBOOT,
	KAUTH_SYSTEM_SETIDCORE,
	KAUTH_SYSTEM_SWAPCTL,
	KAUTH_SYSTEM_SYSCTL,
	KAUTH_SYSTEM_TIME,
	KAUTH_SYSTEM_MODULE,
	KAUTH_SYSTEM_FS_RESERVEDSPACE,
	KAUTH_SYSTEM_FS_QUOTA,
	KAUTH_SYSTEM_SEMAPHORE,
	KAUTH_SYSTEM_SYSVIPC,
	KAUTH_SYSTEM_MQUEUE,
	KAUTH_SYSTEM_VERIEXEC,
	KAUTH_SYSTEM_DEVMAPPER,
	KAUTH_SYSTEM_MAP_VA_ZERO,
	KAUTH_SYSTEM_LFS,
	KAUTH_SYSTEM_FS_EXTATTR,
	KAUTH_SYSTEM_FS_SNAPSHOT,
	KAUTH_SYSTEM_INTR,
};

/*
 * System scope - sub-actions.
 */
enum kauth_system_req {
	KAUTH_REQ_SYSTEM_CHROOT_CHROOT=1,
	KAUTH_REQ_SYSTEM_CHROOT_FCHROOT,
	KAUTH_REQ_SYSTEM_CPU_SETSTATE,
	KAUTH_REQ_SYSTEM_DEBUG_IPKDB,
	KAUTH_REQ_SYSTEM_MOUNT_GET,
	KAUTH_REQ_SYSTEM_MOUNT_NEW,
	KAUTH_REQ_SYSTEM_MOUNT_UNMOUNT,
	KAUTH_REQ_SYSTEM_MOUNT_UPDATE,
	KAUTH_REQ_SYSTEM_PSET_ASSIGN,
	KAUTH_REQ_SYSTEM_PSET_BIND,
	KAUTH_REQ_SYSTEM_PSET_CREATE,
	KAUTH_REQ_SYSTEM_PSET_DESTROY,
	KAUTH_REQ_SYSTEM_SYSCTL_ADD,
	KAUTH_REQ_SYSTEM_SYSCTL_DELETE,
	KAUTH_REQ_SYSTEM_SYSCTL_DESC,
	KAUTH_REQ_SYSTEM_SYSCTL_MODIFY,
	KAUTH_REQ_SYSTEM_SYSCTL_PRVT,
	KAUTH_REQ_SYSTEM_TIME_ADJTIME,
	KAUTH_REQ_SYSTEM_TIME_NTPADJTIME,
	KAUTH_REQ_SYSTEM_TIME_RTCOFFSET,
	KAUTH_REQ_SYSTEM_TIME_SYSTEM,
	KAUTH_REQ_SYSTEM_TIME_TIMECOUNTERS,
	KAUTH_REQ_SYSTEM_FS_QUOTA_GET,
	KAUTH_REQ_SYSTEM_FS_QUOTA_MANAGE,
	KAUTH_REQ_SYSTEM_FS_QUOTA_NOLIMIT,
	KAUTH_REQ_SYSTEM_FS_QUOTA_ONOFF,
	KAUTH_REQ_SYSTEM_SYSVIPC_BYPASS,
	KAUTH_REQ_SYSTEM_SYSVIPC_SHM_LOCK,
	KAUTH_REQ_SYSTEM_SYSVIPC_SHM_UNLOCK,
	KAUTH_REQ_SYSTEM_SYSVIPC_MSGQ_OVERSIZE,
	KAUTH_REQ_SYSTEM_VERIEXEC_ACCESS,
	KAUTH_REQ_SYSTEM_VERIEXEC_MODIFY,
	KAUTH_REQ_SYSTEM_LFS_MARKV,
	KAUTH_REQ_SYSTEM_LFS_BMAPV,
	KAUTH_REQ_SYSTEM_LFS_SEGCLEAN,
	KAUTH_REQ_SYSTEM_LFS_SEGWAIT,
	KAUTH_REQ_SYSTEM_LFS_FCNTL,
	KAUTH_REQ_SYSTEM_MOUNT_UMAP,
	KAUTH_REQ_SYSTEM_MOUNT_DEVICE,
	KAUTH_REQ_SYSTEM_INTR_AFFINITY,
};

/*
 * Process scope - actions.
 */
enum {
	KAUTH_PROCESS_CANSEE=1,
	KAUTH_PROCESS_CORENAME,
	KAUTH_PROCESS_FORK,
	KAUTH_PROCESS_KEVENT_FILTER,
	KAUTH_PROCESS_KTRACE,
	KAUTH_PROCESS_NICE,
	KAUTH_PROCESS_PROCFS,
	KAUTH_PROCESS_PTRACE,
	KAUTH_PROCESS_RLIMIT,
	KAUTH_PROCESS_SCHEDULER_GETAFFINITY,
	KAUTH_PROCESS_SCHEDULER_SETAFFINITY,
	KAUTH_PROCESS_SCHEDULER_GETPARAM,
	KAUTH_PROCESS_SCHEDULER_SETPARAM,
	KAUTH_PROCESS_SETID,
	KAUTH_PROCESS_SIGNAL,
	KAUTH_PROCESS_STOPFLAG
};

/*
 * Process scope - sub-actions.
 */
enum kauth_process_req {
	KAUTH_REQ_PROCESS_CANSEE_ARGS=1,
	KAUTH_REQ_PROCESS_CANSEE_ENTRY,
	KAUTH_REQ_PROCESS_CANSEE_ENV,
	KAUTH_REQ_PROCESS_CANSEE_OPENFILES,
	KAUTH_REQ_PROCESS_CORENAME_GET,
	KAUTH_REQ_PROCESS_CORENAME_SET,
	KAUTH_REQ_PROCESS_KTRACE_PERSISTENT,
	KAUTH_REQ_PROCESS_PROCFS_CTL,
	KAUTH_REQ_PROCESS_PROCFS_READ,
	KAUTH_REQ_PROCESS_PROCFS_RW,
	KAUTH_REQ_PROCESS_PROCFS_WRITE,
	KAUTH_REQ_PROCESS_RLIMIT_GET,
	KAUTH_REQ_PROCESS_RLIMIT_SET,
	KAUTH_REQ_PROCESS_RLIMIT_BYPASS,
};

/*
 * Network scope - actions.
 */
enum {
	KAUTH_NETWORK_ALTQ=1,
	KAUTH_NETWORK_BIND,
	KAUTH_NETWORK_FIREWALL,
	KAUTH_NETWORK_INTERFACE,
	KAUTH_NETWORK_FORWSRCRT,
	KAUTH_NETWORK_NFS,
	KAUTH_NETWORK_ROUTE,
	KAUTH_NETWORK_SOCKET,
	KAUTH_NETWORK_INTERFACE_PPP,
	KAUTH_NETWORK_INTERFACE_SLIP,
	KAUTH_NETWORK_INTERFACE_STRIP,
	KAUTH_NETWORK_INTERFACE_TUN,
	KAUTH_NETWORK_INTERFACE_BRIDGE,
	KAUTH_NETWORK_IPSEC,
	KAUTH_NETWORK_INTERFACE_PVC,
	KAUTH_NETWORK_IPV6,
	KAUTH_NETWORK_SMB,
};

/*
 * Network scope - sub-actions.
 */
enum kauth_network_req {
	KAUTH_REQ_NETWORK_ALTQ_AFMAP=1,
	KAUTH_REQ_NETWORK_ALTQ_BLUE,
	KAUTH_REQ_NETWORK_ALTQ_CBQ,
	KAUTH_REQ_NETWORK_ALTQ_CDNR,
	KAUTH_REQ_NETWORK_ALTQ_CONF,
	KAUTH_REQ_NETWORK_ALTQ_FIFOQ,
	KAUTH_REQ_NETWORK_ALTQ_HFSC,
	KAUTH_REQ_NETWORK_ALTQ_JOBS,
	KAUTH_REQ_NETWORK_ALTQ_PRIQ,
	KAUTH_REQ_NETWORK_ALTQ_RED,
	KAUTH_REQ_NETWORK_ALTQ_RIO,
	KAUTH_REQ_NETWORK_ALTQ_WFQ,
	KAUTH_REQ_NETWORK_BIND_PORT,
	KAUTH_REQ_NETWORK_BIND_PRIVPORT,
	KAUTH_REQ_NETWORK_FIREWALL_FW,
	KAUTH_REQ_NETWORK_FIREWALL_NAT,
	KAUTH_REQ_NETWORK_INTERFACE_GET,
	KAUTH_REQ_NETWORK_INTERFACE_GETPRIV,
	KAUTH_REQ_NETWORK_INTERFACE_SET,
	KAUTH_REQ_NETWORK_INTERFACE_SETPRIV,
	KAUTH_REQ_NETWORK_NFS_EXPORT,
	KAUTH_REQ_NETWORK_NFS_SVC,
	KAUTH_REQ_NETWORK_SOCKET_OPEN,
	KAUTH_REQ_NETWORK_SOCKET_RAWSOCK,
	KAUTH_REQ_NETWORK_SOCKET_CANSEE,
	KAUTH_REQ_NETWORK_SOCKET_DROP,
	KAUTH_REQ_NETWORK_SOCKET_SETPRIV,
	KAUTH_REQ_NETWORK_INTERFACE_PPP_ADD,
	KAUTH_REQ_NETWORK_INTERFACE_SLIP_ADD,
	KAUTH_REQ_NETWORK_INTERFACE_STRIP_ADD,
	KAUTH_REQ_NETWORK_INTERFACE_TUN_ADD,
	KAUTH_REQ_NETWORK_IPV6_HOPBYHOP,
	KAUTH_REQ_NETWORK_INTERFACE_BRIDGE_GETPRIV,
	KAUTH_REQ_NETWORK_INTERFACE_BRIDGE_SETPRIV,
	KAUTH_REQ_NETWORK_IPSEC_BYPASS,
	KAUTH_REQ_NETWORK_IPV6_JOIN_MULTICAST,
	KAUTH_REQ_NETWORK_INTERFACE_PVC_ADD,
	KAUTH_REQ_NETWORK_SMB_SHARE_ACCESS,
	KAUTH_REQ_NETWORK_SMB_SHARE_CREATE,
	KAUTH_REQ_NETWORK_SMB_VC_ACCESS,
	KAUTH_REQ_NETWORK_SMB_VC_CREATE,
	KAUTH_REQ_NETWORK_INTERFACE_FIRMWARE,
};

/*
 * Machdep scope - actions.
 */
enum {
	KAUTH_MACHDEP_CACHEFLUSH=1,
	KAUTH_MACHDEP_CPU_UCODE_APPLY,
	KAUTH_MACHDEP_IOPERM_GET,
	KAUTH_MACHDEP_IOPERM_SET,
	KAUTH_MACHDEP_IOPL,
	KAUTH_MACHDEP_LDT_GET,
	KAUTH_MACHDEP_LDT_SET,
	KAUTH_MACHDEP_MTRR_GET,
	KAUTH_MACHDEP_MTRR_SET,
	KAUTH_MACHDEP_NVRAM,
	KAUTH_MACHDEP_UNMANAGEDMEM,
	KAUTH_MACHDEP_PXG,
};

/*
 * Device scope - actions.
 */
enum {
	KAUTH_DEVICE_TTY_OPEN=1,
	KAUTH_DEVICE_TTY_PRIVSET,
	KAUTH_DEVICE_TTY_STI,
	KAUTH_DEVICE_RAWIO_SPEC,
	KAUTH_DEVICE_RAWIO_PASSTHRU,
	KAUTH_DEVICE_BLUETOOTH_SETPRIV,
	KAUTH_DEVICE_RND_ADDDATA,
	KAUTH_DEVICE_RND_ADDDATA_ESTIMATE,
	KAUTH_DEVICE_RND_GETPRIV,
	KAUTH_DEVICE_RND_SETPRIV,
	KAUTH_DEVICE_BLUETOOTH_BCSP,
	KAUTH_DEVICE_BLUETOOTH_BTUART,
	KAUTH_DEVICE_GPIO_PINSET,
	KAUTH_DEVICE_BLUETOOTH_SEND,
	KAUTH_DEVICE_BLUETOOTH_RECV,
	KAUTH_DEVICE_TTY_VIRTUAL,
	KAUTH_DEVICE_WSCONS_KEYBOARD_BELL,
	KAUTH_DEVICE_WSCONS_KEYBOARD_KEYREPEAT,
};

/*
 * Device scope - sub-actions.
 */
enum kauth_device_req {
	KAUTH_REQ_DEVICE_RAWIO_SPEC_READ=1,
	KAUTH_REQ_DEVICE_RAWIO_SPEC_WRITE,
	KAUTH_REQ_DEVICE_RAWIO_SPEC_RW,
	KAUTH_REQ_DEVICE_BLUETOOTH_BCSP_ADD,
	KAUTH_REQ_DEVICE_BLUETOOTH_BTUART_ADD,
};

/*
 * Credentials scope - actions.
 */
enum {
	KAUTH_CRED_INIT=1,
	KAUTH_CRED_FORK,
	KAUTH_CRED_COPY,
	KAUTH_CRED_FREE,
	KAUTH_CRED_CHROOT
};

/*
 * Vnode scope - action bits.
 */
#define	KAUTH_VNODE_READ_DATA		(1U << 0)
#define	KAUTH_VNODE_LIST_DIRECTORY	KAUTH_VNODE_READ_DATA
#define	KAUTH_VNODE_WRITE_DATA		(1U << 1)
#define	KAUTH_VNODE_ADD_FILE		KAUTH_VNODE_WRITE_DATA
#define	KAUTH_VNODE_EXECUTE		(1U << 2)
#define	KAUTH_VNODE_SEARCH		KAUTH_VNODE_EXECUTE
#define	KAUTH_VNODE_DELETE		(1U << 3)
#define	KAUTH_VNODE_APPEND_DATA		(1U << 4)
#define	KAUTH_VNODE_ADD_SUBDIRECTORY	KAUTH_VNODE_APPEND_DATA
#define	KAUTH_VNODE_READ_TIMES		(1U << 5)
#define	KAUTH_VNODE_WRITE_TIMES		(1U << 6)
#define	KAUTH_VNODE_READ_FLAGS		(1U << 7)
#define	KAUTH_VNODE_WRITE_FLAGS		(1U << 8)
#define	KAUTH_VNODE_READ_SYSFLAGS	(1U << 9)
#define	KAUTH_VNODE_WRITE_SYSFLAGS	(1U << 10)
#define	KAUTH_VNODE_RENAME		(1U << 11)
#define	KAUTH_VNODE_CHANGE_OWNERSHIP	(1U << 12)
#define	KAUTH_VNODE_READ_SECURITY	(1U << 13)
#define	KAUTH_VNODE_WRITE_SECURITY	(1U << 14)
#define	KAUTH_VNODE_READ_ATTRIBUTES	(1U << 15)
#define	KAUTH_VNODE_WRITE_ATTRIBUTES	(1U << 16)
#define	KAUTH_VNODE_READ_EXTATTRIBUTES	(1U << 17)
#define	KAUTH_VNODE_WRITE_EXTATTRIBUTES	(1U << 18)
#define	KAUTH_VNODE_RETAIN_SUID		(1U << 19)
#define	KAUTH_VNODE_RETAIN_SGID		(1U << 20)
#define	KAUTH_VNODE_REVOKE		(1U << 21)

#define	KAUTH_VNODE_IS_EXEC		(1U << 29)
#define	KAUTH_VNODE_HAS_SYSFLAGS	(1U << 30)
#define	KAUTH_VNODE_ACCESS		(1U << 31)

/*
 * This is a special fs_decision indication that can be used by file-systems
 * that don't support decision-before-action to tell kauth(9) it can only
 * short-circuit the operation beforehand.
 */
#define	KAUTH_VNODE_REMOTEFS		(-1)

/*
 * Device scope, passthru request - identifiers.
 */
#define	KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_READ		0x00000001
#define	KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_WRITE		0x00000002
#define	KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_READCONF	0x00000004
#define	KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_WRITECONF	0x00000008
#define	KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_ALL		0x0000000F

#define NOCRED ((kauth_cred_t)-1)	/* no credential available */
#define FSCRED ((kauth_cred_t)-2)	/* filesystem credential */

/* Macro to help passing arguments to authorization wrappers. */
#define	KAUTH_ARG(arg)	((void *)(unsigned long)(arg))

/*
 * A file-system object is determined to be able to execute if it's a
 * directory or if the execute bit is present in any of the
 * owner/group/other modes.
 *
 * This helper macro is intended to be used in order to implement a
 * policy that maintains the semantics of "a privileged user can enter
 * directory, and can execute any file, but only if the file is actually
 * executable."
 */
#define	FS_OBJECT_CAN_EXEC(vtype, mode)	(((vtype) == VDIR) ||		\
					 ((mode) &			\
					  (S_IXUSR|S_IXGRP|S_IXOTH)))

/*
 * Prototypes.
 */
void kauth_init(void);
kauth_scope_t kauth_register_scope(const char *, kauth_scope_callback_t, void *);
void kauth_deregister_scope(kauth_scope_t);
kauth_listener_t kauth_listen_scope(const char *, kauth_scope_callback_t, void *);
void kauth_unlisten_scope(kauth_listener_t);
int kauth_authorize_action(kauth_scope_t, kauth_cred_t, kauth_action_t, void *,
    void *, void *, void *);

/* Authorization wrappers. */
int kauth_authorize_generic(kauth_cred_t, kauth_action_t, void *);
int kauth_authorize_system(kauth_cred_t, kauth_action_t, enum kauth_system_req,
    void *, void *, void *);
int kauth_authorize_process(kauth_cred_t, kauth_action_t, struct proc *,
    void *, void *, void *);
int kauth_authorize_network(kauth_cred_t, kauth_action_t,
    enum kauth_network_req, void *, void *, void *);
int kauth_authorize_machdep(kauth_cred_t, kauth_action_t,
    void *, void *, void *, void *);
int kauth_authorize_device(kauth_cred_t, kauth_action_t,
    void *, void *, void *, void *);
int kauth_authorize_device_tty(kauth_cred_t, kauth_action_t, struct tty *);
int kauth_authorize_device_spec(kauth_cred_t, enum kauth_device_req,
    struct vnode *);
int kauth_authorize_device_passthru(kauth_cred_t, dev_t, u_long, void *);
int kauth_authorize_vnode(kauth_cred_t, kauth_action_t, struct vnode *,
    struct vnode *, int);

/* Kauth credentials management routines. */
kauth_cred_t kauth_cred_alloc(void);
void kauth_cred_free(kauth_cred_t);
void kauth_cred_clone(kauth_cred_t, kauth_cred_t);
kauth_cred_t kauth_cred_dup(kauth_cred_t);
kauth_cred_t kauth_cred_copy(kauth_cred_t);

uid_t kauth_cred_getuid(kauth_cred_t);
uid_t kauth_cred_geteuid(kauth_cred_t);
uid_t kauth_cred_getsvuid(kauth_cred_t);
gid_t kauth_cred_getgid(kauth_cred_t);
gid_t kauth_cred_getegid(kauth_cred_t);
gid_t kauth_cred_getsvgid(kauth_cred_t);
int kauth_cred_ismember_gid(kauth_cred_t, gid_t, int *);
u_int kauth_cred_ngroups(kauth_cred_t);
gid_t kauth_cred_group(kauth_cred_t, u_int);

void kauth_cred_setuid(kauth_cred_t, uid_t);
void kauth_cred_seteuid(kauth_cred_t, uid_t);
void kauth_cred_setsvuid(kauth_cred_t, uid_t);
void kauth_cred_setgid(kauth_cred_t, gid_t);
void kauth_cred_setegid(kauth_cred_t, gid_t);
void kauth_cred_setsvgid(kauth_cred_t, gid_t);

void kauth_cred_hold(kauth_cred_t);
u_int kauth_cred_getrefcnt(kauth_cred_t);

int kauth_cred_setgroups(kauth_cred_t, const gid_t *, size_t, uid_t,
    enum uio_seg);
int kauth_cred_getgroups(kauth_cred_t, gid_t *, size_t, enum uio_seg);

/* This is for sys_setgroups() */
int kauth_proc_setgroups(struct lwp *, kauth_cred_t);

int kauth_register_key(secmodel_t, kauth_key_t *);
int kauth_deregister_key(kauth_key_t);
void kauth_cred_setdata(kauth_cred_t, kauth_key_t, void *);
void *kauth_cred_getdata(kauth_cred_t, kauth_key_t);

int kauth_cred_uidmatch(kauth_cred_t, kauth_cred_t);
void kauth_uucred_to_cred(kauth_cred_t, const struct uucred *);
void kauth_cred_to_uucred(struct uucred *, const kauth_cred_t);
int kauth_cred_uucmp(kauth_cred_t, const struct uucred *);
void kauth_cred_toucred(kauth_cred_t, struct ki_ucred *);
void kauth_cred_topcred(kauth_cred_t, struct ki_pcred *);

kauth_action_t kauth_mode_to_action(mode_t);
kauth_action_t kauth_extattr_action(mode_t);

#define KAUTH_ACCESS_ACTION(access_mode, vn_vtype, file_mode)	\
	(kauth_mode_to_action(access_mode) |			\
	(FS_OBJECT_CAN_EXEC(vn_vtype, file_mode) ? KAUTH_VNODE_IS_EXEC : 0))

kauth_cred_t kauth_cred_get(void);

void kauth_proc_fork(struct proc *, struct proc *);
void kauth_proc_chroot(kauth_cred_t cred, struct cwdinfo *cwdi);

#endif	/* !_SYS_KAUTH_H_ */
