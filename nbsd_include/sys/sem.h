/*	$NetBSD: sem.h,v 1.29 2009/01/19 19:39:41 christos Exp $	*/

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
 * SVID compatible sem.h file
 *
 * Author: Daniel Boulet
 */

#ifndef _SYS_SEM_H_
#define _SYS_SEM_H_

#include <sys/featuretest.h>

#include <sys/ipc.h>

struct semid_ds {
	struct ipc_perm sem_perm;             /* operation permission struct */
	time_t 		sem_otime;            /* last semop() time */
	unsigned long int __unused1;
	time_t 		sem_ctime;            /* last time changed by semctl() */
	unsigned long int __unused2;
	unsigned long int sem_nsems;          /* number of semaphores in set */
	unsigned long int __unused3;
	unsigned long int __unused4;
};

/*
 * semop's sops parameter structure
 */
struct sembuf {
	unsigned short	sem_num;	/* semaphore # */
	short		sem_op;		/* semaphore operation */
	short		sem_flg;	/* operation flags */
};
#define SEM_UNDO	0x1000		/* undo changes on process exit */

/*
 * commands for semctl
 */
#define GETPID          11              /* get sempid */
#define GETVAL          12              /* get semval */
#define GETALL          13              /* get all semval's */
#define GETNCNT         14              /* get semncnt */
#define GETZCNT         15              /* get semzcnt */
#define SETVAL          16              /* set semval */
#define SETALL          17              /* set all semval's */

#ifdef __USE_MISC

/* ipcs ctl cmds */
# define SEM_STAT 18
# define SEM_INFO 19

/*
 * semaphore info struct
 */
struct seminfo {
	int32_t	semmap;		/* # of entries in semaphore map */
	int32_t	semmni;		/* # of semaphore identifiers */
	int32_t	semmns;		/* # of semaphores in system */
	int32_t	semmnu;		/* # of undo structures in system */
	int32_t	semmsl;		/* max # of semaphores per id */
	int32_t	semopm;		/* max # of operations per semop call */
	int32_t	semume;		/* max # of undo entries per process */
	int32_t	semusz;		/* size in bytes of undo structure */
	int32_t	semvmx;		/* semaphore maximum value */
	int32_t	semaem;		/* adjust on exit max value */
};

#endif /* __USE_MISC */

/*
 * Configuration parameters
 */
#define SEMMNI	128
#define SEMMSL	250
#define SEMMNS	(SEMMSL*SEMMNI)

#define SEMOPM	32
#define SEMVMX	32767


#include <sys/cdefs.h>

__BEGIN_DECLS
int	semctl(int, int, int, ...);
int	semget(key_t, int, int);
int	semop(int, struct sembuf *, size_t);
__END_DECLS

#endif /* !_SYS_SEM_H_ */
