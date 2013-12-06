/*	$NetBSD: msg.h,v 1.4 2009/01/19 19:39:41 christos Exp $	*/

/*
 * SVID compatible msg.h file
 *
 * Author:  Daniel Boulet
 *
 * Copyright 1993 Daniel Boulet and RTMX Inc.
 *
 * This system call was implemented by Daniel Boulet under contract from RTMX.
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 */

#ifndef _COMPAT_SYS_MSG_H_
#define _COMPAT_SYS_MSG_H_

#include <compat/sys/ipc.h>
/*
 * Old message queue data structure used before NetBSD 1.5.
 */
struct msqid_ds14 {
	struct	ipc_perm14 msg_perm;	/* msg queue permission bits */
	struct	__msg *msg_first;	/* first message in the queue */
	struct	__msg *msg_last;	/* last message in the queue */
	u_long	msg_cbytes;	/* number of bytes in use on the queue */
	u_long	msg_qnum;	/* number of msgs in the queue */
	u_long	msg_qbytes;	/* max # of bytes on the queue */
	pid_t	msg_lspid;	/* pid of last msgsnd() */
	pid_t	msg_lrpid;	/* pid of last msgrcv() */
	int32_t	msg_stime;	/* time of last msgsnd() */
	long	msg_pad1;
	int32_t	msg_rtime;	/* time of last msgrcv() */
	long	msg_pad2;
	int32_t	msg_ctime;	/* time of last msgctl() */
	long	msg_pad3;
	long	msg_pad4[4];
};

struct msqid_ds13 {
	struct ipc_perm	msg_perm;	/* operation permission strucure */
	msgqnum_t	msg_qnum;	/* number of messages in the queue */
	msglen_t	msg_qbytes;	/* max # of bytes in the queue */
	pid_t		msg_lspid;	/* process ID of last msgsend() */
	pid_t		msg_lrpid;	/* process ID of last msgrcv() */
	int32_t		msg_stime;	/* time of last msgsend() */
	int32_t		msg_rtime;	/* time of last msgrcv() */
	int32_t		msg_ctime;	/* time of last change */

	/*
	 * These members are private and used only in the internal
	 * implementation of this interface.
	 */
	struct __msg	*_msg_first;	/* first message in the queue */
	struct __msg	*_msg_last;	/* last message in the queue */
	msglen_t	_msg_cbytes;	/* # of bytes currently in queue */
};

/* Warning: 64-bit structure padding is needed here */
struct msgid_ds_sysctl50 {
	struct		ipc_perm_sysctl msg_perm;
	uint64_t	msg_qnum;
	uint64_t	msg_qbytes;
	uint64_t	_msg_cbytes;
	pid_t		msg_lspid;
	pid_t		msg_lrpid;
	int32_t		msg_stime;
	int32_t		msg_rtime;
	int32_t		msg_ctime;
	int32_t		pad;
};
struct msg_sysctl_info50 {
	struct	msginfo msginfo;
	struct	msgid_ds_sysctl50 msgids[1];
};

__BEGIN_DECLS
static __inline void __msqid_ds14_to_native(const struct msqid_ds14 *, struct msqid_ds *);
static __inline void __native_to_msqid_ds14(const struct msqid_ds *, struct msqid_ds14 *);
static __inline void __msqid_ds13_to_native(const struct msqid_ds13 *, struct msqid_ds *);
static __inline void __native_to_msqid_ds13(const struct msqid_ds *, struct msqid_ds13 *);

static __inline void
__msqid_ds13_to_native(const struct msqid_ds13 *omsqbuf, struct msqid_ds *msqbuf)
{

	msqbuf->msg_perm = omsqbuf->msg_perm;

#define	CVT(x)	msqbuf->x = omsqbuf->x
	CVT(msg_qnum);
	CVT(msg_qbytes);
	CVT(msg_lspid);
	CVT(msg_lrpid);
	CVT(msg_stime);
	CVT(msg_rtime);
	CVT(msg_ctime);
#undef CVT
}

static __inline void
__native_to_msqid_ds13(const struct msqid_ds *msqbuf, struct msqid_ds13 *omsqbuf)
{

	omsqbuf->msg_perm = msqbuf->msg_perm;

#define	CVT(x)	omsqbuf->x = msqbuf->x
#define	CVTI(x)	omsqbuf->x = (int)msqbuf->x
	CVT(msg_qnum);
	CVT(msg_qbytes);
	CVT(msg_lspid);
	CVT(msg_lrpid);
	CVTI(msg_stime);
	CVTI(msg_rtime);
	CVTI(msg_ctime);
#undef CVT
#undef CVTI

	/*
	 * Not part of the API, but some programs might look at it.
	 */
	omsqbuf->_msg_cbytes = msqbuf->_msg_cbytes;
}

static __inline void
__msqid_ds14_to_native(const struct msqid_ds14 *omsqbuf, struct msqid_ds *msqbuf)
{

	__ipc_perm14_to_native(&omsqbuf->msg_perm, &msqbuf->msg_perm);

#define	CVT(x)	msqbuf->x = omsqbuf->x
	CVT(msg_qnum);
	CVT(msg_qbytes);
	CVT(msg_lspid);
	CVT(msg_lrpid);
	CVT(msg_stime);
	CVT(msg_rtime);
	CVT(msg_ctime);
#undef CVT
}

static __inline void
__native_to_msqid_ds14(const struct msqid_ds *msqbuf, struct msqid_ds14 *omsqbuf)
{

	__native_to_ipc_perm14(&msqbuf->msg_perm, &omsqbuf->msg_perm);

#define	CVT(x)	omsqbuf->x = msqbuf->x
#define	CVTI(x)	omsqbuf->x = (int)msqbuf->x
	CVT(msg_qnum);
	CVT(msg_qbytes);
	CVT(msg_lspid);
	CVT(msg_lrpid);
	CVTI(msg_stime);
	CVTI(msg_rtime);
	CVTI(msg_ctime);
#undef CVT
#undef CVTI
}

int	__msgctl13(int, int, struct msqid_ds13 *);
int	__msgctl14(int, int, struct msqid_ds14 *);
int	__msgctl50(int, int, struct msqid_ds *);
__END_DECLS

#endif /* !_COMPAT_SYS_MSG_H_ */
