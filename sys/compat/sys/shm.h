/*	$NetBSD: shm.h,v 1.7 2009/04/01 21:15:23 christos Exp $	*/

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

#ifndef _COMPAT_SYS_SHM_H_
#define _COMPAT_SYS_SHM_H_

#include <compat/sys/ipc.h>

struct shmid_ds14 {
	struct ipc_perm14 shm_perm;	/* operation permission structure */
	int		shm_segsz;	/* size of segment in bytes */
	pid_t		shm_lpid;	/* process ID of last shm op */
	pid_t		shm_cpid;	/* process ID of creator */
	short		shm_nattch;	/* number of current attaches */
	int32_t		shm_atime;	/* time of last shmat() */
	int32_t		shm_dtime;	/* time of last shmdt() */
	int32_t		shm_ctime;	/* time of last change by shmctl() */
	void		*shm_internal;	/* sysv stupidity */
};

struct shmid_ds13 {
	struct ipc_perm	shm_perm;	/* operation permission structure */
	size_t		shm_segsz;	/* size of segment in bytes */
	pid_t		shm_lpid;	/* process ID of last shm operation */
	pid_t		shm_cpid;	/* process ID of creator */
	shmatt_t	shm_nattch;	/* number of current attaches */
	int32_t		shm_atime;	/* time of last shmat() */
	int32_t		shm_dtime;	/* time of last shmdt() */
	int32_t		shm_ctime;	/* time of last change by shmctl() */

	/*
	 * These members are private and used only in the internal
	 * implementation of this interface.
	 */
	void		*_shm_internal;
};

/* Warning: 64-bit structure padding is needed here */
struct shmid_ds_sysctl50 {
	struct		ipc_perm_sysctl shm_perm;
	uint64_t	shm_segsz;
	pid_t		shm_lpid;
	pid_t		shm_cpid;
	int32_t		shm_atime;
	int32_t		shm_dtime;
	int32_t		shm_ctime;
	uint32_t	shm_nattch;
};
struct shm_sysctl_info50 {
	struct	shminfo shminfo;
	struct	shmid_ds_sysctl50 shmids[1];
};

__BEGIN_DECLS
static __inline void	__shmid_ds14_to_native(const struct shmid_ds14 *, struct shmid_ds *);
static __inline void	__native_to_shmid_ds14(const struct shmid_ds *, struct shmid_ds14 *);
static __inline void	__shmid_ds13_to_native(const struct shmid_ds13 *, struct shmid_ds *);
static __inline void	__native_to_shmid_ds13(const struct shmid_ds *, struct shmid_ds13 *);
static __inline void
__shmid_ds14_to_native(const struct shmid_ds14 *oshmbuf, struct shmid_ds *shmbuf)
{

	__ipc_perm14_to_native(&oshmbuf->shm_perm, &shmbuf->shm_perm);

#define	CVT(x)	shmbuf->x = oshmbuf->x
	CVT(shm_segsz);
	CVT(shm_lpid);
	CVT(shm_cpid);
	CVT(shm_nattch);
	CVT(shm_atime);
	CVT(shm_dtime);
	CVT(shm_ctime);
#undef CVT
}

static __inline void
__native_to_shmid_ds14(const struct shmid_ds *shmbuf, struct shmid_ds14 *oshmbuf)
{

	__native_to_ipc_perm14(&shmbuf->shm_perm, &oshmbuf->shm_perm);

#define	CVT(x)	oshmbuf->x = shmbuf->x
#define	CVTI(x)	oshmbuf->x = (int)shmbuf->x
	CVTI(shm_segsz);
	CVT(shm_lpid);
	CVT(shm_cpid);
	CVT(shm_nattch);
	CVTI(shm_atime);
	CVTI(shm_dtime);
	CVTI(shm_ctime);
#undef CVT
#undef CVTI
}

static __inline void
__shmid_ds13_to_native(const struct shmid_ds13 *oshmbuf, struct shmid_ds *shmbuf)
{

	shmbuf->shm_perm = oshmbuf->shm_perm;

#define	CVT(x)	shmbuf->x = oshmbuf->x
	CVT(shm_segsz);
	CVT(shm_lpid);
	CVT(shm_cpid);
	CVT(shm_nattch);
	CVT(shm_atime);
	CVT(shm_dtime);
	CVT(shm_ctime);
#undef CVT
}

static __inline void
__native_to_shmid_ds13(const struct shmid_ds *shmbuf, struct shmid_ds13 *oshmbuf)
{

	oshmbuf->shm_perm = shmbuf->shm_perm;

#define	CVT(x)	oshmbuf->x = shmbuf->x
#define	CVTI(x)	oshmbuf->x = (int)shmbuf->x
	CVT(shm_segsz);
	CVT(shm_lpid);
	CVT(shm_cpid);
	CVT(shm_nattch);
	CVTI(shm_atime);
	CVTI(shm_dtime);
	CVTI(shm_ctime);
#undef CVT
#undef CVTI
}

int	__shmctl13(int, int, struct shmid_ds13 *);
int	__shmctl14(int, int, struct shmid_ds14 *);
int	__shmctl50(int, int, struct shmid_ds *);
__END_DECLS

#endif /* !_COMPAT_SYS_SHM_H_ */
