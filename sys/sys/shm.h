/*	$NetBSD: shm.h,v 1.48 2009/01/19 19:39:41 christos Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994 Adam Glass
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Adam Glass.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * As defined+described in "X/Open System Interfaces and Headers"
 *                         Issue 4, p. XXX
 */

#ifndef _SYS_SHM_H_
#define _SYS_SHM_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>

#include <sys/ipc.h>

#define	SHM_RDONLY	010000	/* Attach read-only (else read-write) */
#define	SHM_RND		020000	/* Round attach address to SHMLBA */

/* Segment low boundry address multiple */
#define	SHMLBA		getpagesize()
#define SHMMNI	4096
#define SHMSEG	32		/* max shared segs per process */

typedef unsigned int	shmatt_t;

struct shmid_ds {
	struct ipc_perm	shm_perm;	/* operation permission structure */
	size_t		shm_segsz;	/* size of segment in bytes */
	time_t		shm_atime;	/* time of last shmat() */
	time_t		shm_dtime;	/* time of last shmdt() */
	time_t		shm_ctime;	/* time of last change by shmctl() */
	pid_t		shm_cpid;	/* process ID of creator */
	pid_t		shm_lpid;	/* process ID of last shm operation */
	shmatt_t	shm_nattch;	/* number of current attaches */
};


/* shm_mode upper byte flags */
#define SHM_DEST 01000			/* segment will be destroyed on last detach */
#define SHM_LOCKED 02000		/* segment will not be swapped */

/* ipcs ctl commands */
#define SHM_STAT	13
#define SHM_INFO	14


#if defined(_NETBSD_SOURCE) || defined(__minix)
/*
 * Permission definitions used in shmflag arguments to shmat(2) and shmget(2).
 * Provided for source compatibility only; do not use in new code!
 */
#define	SHM_R		0400
#define	SHM_W		0200

/*
 * System 5 style catch-all structure for shared memory constants that
 * might be of interest to user programs.  Do we really want/need this?
 */
struct shminfo {
	unsigned long int shmmax;	/* max shared memory segment size (bytes) */
	unsigned long int shmmin;	/* min shared memory segment size (bytes) */
	unsigned long int shmmni;	/* max number of shared memory identifiers */
	unsigned long int shmseg;	/* max shared memory segments per process */
	unsigned long int shmall;	/* max amount of shared memory (pages) */
};

#ifdef __minix
struct shm_info
{
	int used_ids;
	unsigned long int shm_tot;  /* total allocated shm */
	unsigned long int shm_rss;  /* total resident shm */
	unsigned long int shm_swp;  /* total swapped shm */
	unsigned long int swap_attempts;
	unsigned long int swap_successes;
};
#endif /* __minix */

#endif /* _NETBSD_SOURCE */

__BEGIN_DECLS
void	*shmat(int, const void *, int);
int	shmctl(int, int, struct shmid_ds *) __RENAME(__shmctl50);
int	shmdt(const void *);
int	shmget(key_t, size_t, int);
__END_DECLS

#endif /* !_SYS_SHM_H_ */
