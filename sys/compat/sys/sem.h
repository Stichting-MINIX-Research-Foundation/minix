/*	$NetBSD: sem.h,v 1.6 2009/01/19 19:39:41 christos Exp $	*/

/*
 * SVID compatible sem.h file
 *
 * Author: Daniel Boulet
 */

#ifndef _COMPAT_SYS_SEM_H_
#define _COMPAT_SYS_SEM_H_

#include <compat/sys/ipc.h>

struct semid_ds14 {
	struct ipc_perm14 sem_perm;	/* operation permission struct */
	struct __sem	*sem_base;	/* pointer to first semaphore in set */
	unsigned short	sem_nsems;	/* number of sems in set */
	int32_t		sem_otime;	/* last operation time */
	long		sem_pad1;	/* SVABI/386 says I need this here */
	int32_t		sem_ctime;	/* last change time */
    					/* Times measured in secs since */
    					/* 00:00:00 GMT, Jan. 1, 1970 */
	long		sem_pad2;	/* SVABI/386 says I need this here */
	long		sem_pad3[4];	/* SVABI/386 says I need this here */
};

struct semid_ds13 {
	struct ipc_perm	sem_perm;	/* operation permission structure */
	unsigned short	sem_nsems;	/* number of semaphores in set */
	int32_t		sem_otime;	/* last semop() time */
	int32_t		sem_ctime;	/* last time changed by semctl() */

	/*
	 * These members are private and used only in the internal
	 * implementation of this interface.
	 */
	struct __sem	*_sem_base;	/* pointer to first semaphore in set */
};

/* Warning: 64-bit structure padding is needed here */
struct semid_ds_sysctl50 {
	struct	ipc_perm_sysctl sem_perm;
	int16_t	sem_nsems;
	int16_t	pad2;
	int32_t	pad3;
	int32_t	sem_otime;
	int32_t	sem_ctime;
};

struct sem_sysctl_info50 {
	struct	seminfo seminfo;
	struct	semid_ds_sysctl50 semids[1];
};

__BEGIN_DECLS
static __inline void	__semid_ds14_to_native(const struct semid_ds14 *, struct semid_ds *);
static __inline void	__native_to_semid_ds14(const struct semid_ds *, struct semid_ds14 *);
static __inline void	__semid_ds13_to_native(const struct semid_ds13 *, struct semid_ds *);
static __inline void	__native_to_semid_ds13(const struct semid_ds *, struct semid_ds13 *);

static __inline void
__semid_ds13_to_native(const struct semid_ds13  *osembuf, struct semid_ds *sembuf)
{

	sembuf->sem_perm = osembuf->sem_perm;

#define	CVT(x)	sembuf->x = osembuf->x
	CVT(sem_nsems);
	CVT(sem_otime);
	CVT(sem_ctime);
#undef CVT
}

static __inline void
__native_to_semid_ds13(const struct semid_ds *sembuf, struct semid_ds13 *osembuf)
{

	osembuf->sem_perm = sembuf->sem_perm;

#define	CVT(x)	osembuf->x = sembuf->x
#define	CVTI(x)	osembuf->x = (int)sembuf->x
	CVT(sem_nsems);
	CVTI(sem_otime);
	CVTI(sem_ctime);
#undef CVT
#undef CVTI
}

static __inline void
__semid_ds14_to_native(const struct semid_ds14  *osembuf, struct semid_ds *sembuf)
{

	__ipc_perm14_to_native(&osembuf->sem_perm, &sembuf->sem_perm);

#define	CVT(x)	sembuf->x = osembuf->x
	CVT(sem_nsems);
	CVT(sem_otime);
	CVT(sem_ctime);
#undef CVT
}

static __inline void
__native_to_semid_ds14(const struct semid_ds *sembuf, struct semid_ds14 *osembuf)
{

	__native_to_ipc_perm14(&sembuf->sem_perm, &osembuf->sem_perm);

#define	CVT(x)	osembuf->x = sembuf->x
#define	CVTI(x)	osembuf->x = (int)sembuf->x
	CVT(sem_nsems);
	CVTI(sem_otime);
	CVTI(sem_ctime);
#undef CVT
#undef CVTI
}

int	semctl(int, int, int, ...);
int	__semctl(int, int, int, union __semun *);
int	__semctl13(int, int, int, ...);
int	__semctl14(int, int, int, ...);
int	__semctl50(int, int, int, ...);
int	____semctl50(int, int, int, ...);
__END_DECLS

#endif /* !_COMPAT_SYS_SEM_H_ */
