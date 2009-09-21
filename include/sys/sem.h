#ifndef _SYS_SEM_H
#define _SYS_SEM_H

#include <sys/types.h>
#include <sys/ipc.h>

#define SEMMNI	128
#define SEMMSL	250
#define SEMMNS	(SEMMSL*SEMMNI)

#define SEMOPM	32
#define SEMVMX	32767

/* Flags for `semop'.  */
#define SEM_UNDO        0x1000          /* undo the operation on exit */

/* Commands for `semctl'.  */
#define GETPID          11              /* get sempid */
#define GETVAL          12              /* get semval */
#define GETALL          13              /* get all semval's */
#define GETNCNT         14              /* get semncnt */
#define GETZCNT         15              /* get semzcnt */
#define SETVAL          16              /* set semval */
#define SETALL          17              /* set all semval's */


/* Data structure describing a set of semaphores.  */
struct semid_ds
{
	struct ipc_perm sem_perm;             /* operation permission struct */
	time_t sem_otime;                   /* last semop() time */
	unsigned long int __unused1;
	time_t sem_ctime;                   /* last time changed by semctl() */
	unsigned long int __unused2;
	unsigned long int sem_nsems;          /* number of semaphores in set */
	unsigned long int __unused3;
	unsigned long int __unused4;
};

/* Structure used for argument to `semop' to describe operations.  */
struct sembuf
{
	unsigned short int sem_num;   /* semaphore number */
	short int sem_op;             /* semaphore operation */
	short int sem_flg;            /* operation flag */
};


/* Semaphore control operation.  */
_PROTOTYPE( int semctl, (int __semid, int __semnum, int __cmd, ...));

/* Get semaphore.  */
_PROTOTYPE( int semget, (key_t __key, int __nsems, int __semflg));

/* Operate on semaphore.  */
_PROTOTYPE( int semop, (int __semid, struct sembuf *__sops, size_t __nsops));


#ifdef __USE_MISC

/* ipcs ctl cmds */
# define SEM_STAT 18
# define SEM_INFO 19

struct  seminfo
{
	int semmap;
	int semmni;
	int semmns;
	int semmnu;
	int semmsl;
	int semopm;
	int semume;
	int semusz;
	int semvmx;
	int semaem;
};

#endif /* __USE_MISC */

#endif /* _SYS_SEM_H */
