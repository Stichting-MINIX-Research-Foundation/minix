#ifndef _SYS_IPC_H
#define _SYS_IPC_H

/* For gid_t, uid_t */
#include <sys/types.h>

/* Mode bits for `msgget', `semget', and `shmget'. */
/* Create key if key does not exist. */
#define IPC_CREAT	01000
/* Fail if key exists. */
#define IPC_EXCL	02000
/* Return error on wait. */
#define IPC_NOWAIT	04000

/* Control commands for `msgctl', `semctl', and `shmctl'. */
/* Remove identifier. */
#define IPC_RMID	0
/* Set `ipc_perm' options. */
#define IPC_SET		1
/* Get `ipc_perm' options. */
#define IPC_STAT	2
#define IPC_INFO	3	/* See ipcs. */

/* Special key values. */
/* Private key. */
#define IPC_PRIVATE	((key_t) 0)

/* Data structure used to pass permission information to IPC operations. */
struct ipc_perm
{
	key_t key;	/* Key. */
	uid_t uid;		/* Owner's user ID. */
	gid_t gid;		/* Owner's group ID. */
	uid_t cuid;		/* Creator's user ID. */
	gid_t cgid;		/* Creator's group ID. */
	unsigned short int mode;	/* Reader/write permission. */
	unsigned short int __seq;	/* Sequence number. */
};

_PROTOTYPE( key_t ftok, (const char *__path, int __id));

#endif /* _SYS_IPC_H */
