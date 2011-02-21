/*
 * SVID compatible ipc.h file
 */

#ifndef _SYS_IPC_H_
#define _SYS_IPC_H_

#include <sys/featuretest.h>
#include <sys/types.h>

/* Data structure used to pass permission information to IPC operations. */
struct ipc_perm
{
	key_t key;			/* Key. */
	uid_t uid;			/* Owner's user ID. */
	gid_t  gid;			/* Owner's group ID. */
	uid_t cuid;			/* Creator's user ID. */
	gid_t  cgid;			/* Creator's group ID. */
	unsigned short int mode;	/* Reader/write permission. */
	unsigned short int __seq;	/* Sequence number. */
};

/* X/Open required constants (same values as system 5) */
#define	IPC_CREAT	001000	/* create entry if key does not exist */
#define	IPC_EXCL	002000	/* fail if key exists */
#define	IPC_NOWAIT	004000	/* error if request must wait */

#define	IPC_PRIVATE	(key_t)0 /* private key */

#define	IPC_RMID	0	/* remove identifier */
#define	IPC_SET		1	/* set options */
#define	IPC_STAT	2	/* get options */

#ifdef __minix
#define IPC_INFO	3	/* See ipcs. */
#endif /* !__minix */

/*
 * Macro to convert between ipc ids and array indices or sequence ids.
 */
#if defined(_NETBSD_SOURCE)
#define	IXSEQ_TO_IPCID(ix,perm)	(((perm._seq) << 16) | (ix & 0xffff))
#endif

#include <sys/cdefs.h>

__BEGIN_DECLS
key_t	ftok(const char *, int);
__END_DECLS

#endif /* !_SYS_IPC_H_ */
