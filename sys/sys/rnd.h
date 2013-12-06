/*	$NetBSD: rnd.h,v 1.40 2013/08/29 01:04:49 tls Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Graff <explorer@flame.org>.  This code uses ideas and
 * algorithms from the Linux driver written by Ted Ts'o.
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

#ifndef _SYS_RND_H_
#define	_SYS_RND_H_

#ifndef _KERNEL
#include <sys/cdefs.h>
#endif /* !_KERNEL */

#include <sys/types.h>
#include <sys/sha1.h>

#ifdef _KERNEL
#include <sys/queue.h>
#endif

#ifdef _KERNEL
#include <sys/rngtest.h>
#endif

#define	RND_DEV_RANDOM	0	/* minor devices for random and kinda random */
#define	RND_DEV_URANDOM	1

/*
 * Exposed "size" of entropy pool, for convenience in load/save
 * from userspace.  Do not assume this is the same as the actual in-kernel
 * pool size!
 */
#define RND_SAVEWORDS	128
typedef struct {
	uint32_t entropy;
	uint8_t data[RND_SAVEWORDS * sizeof(uint32_t)];
	uint8_t digest[SHA1_DIGEST_LENGTH];
} rndsave_t;

/* Statistics exposed by RNDGETPOOLSTAT */
typedef struct
{
	uint32_t	poolsize;
	uint32_t 	threshold;
	uint32_t	maxentropy;

	uint32_t	added;
	uint32_t	curentropy;
	uint32_t	removed;
	uint32_t	discarded;
	uint32_t	generated;
} rndpoolstat_t;

/* Sanitized random source view for userspace */
typedef struct {
	char		name[16];	/* device name */
	uint32_t	total;		/* entropy from this source */
	uint32_t	type;		/* type */
	uint32_t	flags;		/* flags */
} rndsource_t;

/*
 * Flags to control the source.  Low byte is type, upper bits are flags.
 */
#define	RND_FLAG_NO_ESTIMATE	0x00000100	/* don't estimate entropy */
#define	RND_FLAG_NO_COLLECT	0x00000200	/* don't collect entropy */
#define RND_FLAG_FAST		0x00000400	/* process samples in bulk */
#define RND_FLAG_HASCB		0x00000800	/* has get callback */

#define	RND_TYPE_UNKNOWN	0	/* unknown source */
#define	RND_TYPE_DISK		1	/* source is physical disk */
#define	RND_TYPE_NET		2	/* source is a network device */
#define	RND_TYPE_TAPE		3	/* source is a tape drive */
#define	RND_TYPE_TTY		4	/* source is a tty device */
#define	RND_TYPE_RNG		5	/* source is a hardware RNG */
#define RND_TYPE_SKEW		6	/* source is skew between clocks */
#define RND_TYPE_ENV		7	/* source is temp or fan sensor */
#define RND_TYPE_VM		8	/* source is VM system events */
#define RND_TYPE_POWER		9	/* source is power events */
#define	RND_TYPE_MAX		9	/* last type id used */

#ifdef _KERNEL
/*
 * Size of entropy pool in 32-bit words.  This _MUST_ be a power of 2.  Don't
 * change this unless you really know what you are doing...
 */
#ifndef RND_POOLWORDS
#define RND_POOLWORDS	128
#endif
#define RND_POOLBITS	(RND_POOLWORDS * 32)

typedef struct krndsource {
	LIST_ENTRY(krndsource) list;	/* the linked list */
        char            name[16];       /* device name */
        uint32_t        last_time;      /* last time recorded */
        uint32_t        last_delta;     /* last delta value */
        uint32_t        last_delta2;    /* last delta2 value */
        uint32_t        total;          /* entropy from this source */
        uint32_t        type;           /* type */
        uint32_t        flags;          /* flags */
        void            *state;         /* state information */
        size_t          test_cnt;       /* how much test data accumulated? */
        rngtest_t	*test;          /* test data for RNG type sources */
	void		(*get)(size_t, void *);	/* pool wants N bytes (badly) */
	void		*getarg;	/* argument to get-function */
} krndsource_t;

static inline void
rndsource_setcb(struct krndsource *const rs, void *const cb, void *const arg)
{
	rs->get = cb;
	rs->getarg = arg;
}

typedef struct {
        uint32_t        cursor;         /* current add point in the pool */
        uint32_t        rotate;         /* how many bits to rotate by */
        rndpoolstat_t   stats;          /* current statistics */
        uint32_t        pool[RND_POOLWORDS]; /* random pool data */
} rndpool_t;

#define RND_ENABLED(rp) \
        (((rp)->flags & RND_FLAG_NO_COLLECT) == 0)

void		rndpool_init(rndpool_t *);
void		rndpool_init_global(void);
uint32_t	rndpool_get_entropy_count(rndpool_t *);
void		rndpool_get_stats(rndpool_t *, void *, int);
void		rndpool_increment_entropy_count(rndpool_t *, uint32_t);
uint32_t	*rndpool_get_pool(rndpool_t *);
uint32_t	rndpool_get_poolsize(void);
void		rndpool_add_data(rndpool_t *,
				 const void *const , uint32_t, uint32_t);
uint32_t	rndpool_extract_data(rndpool_t *, void *, uint32_t, uint32_t);
void		rnd_init(void);
void		rnd_init_softint(void);
void		_rnd_add_uint32(krndsource_t *, uint32_t);
void		rnd_add_data(krndsource_t *, const void *const, uint32_t,
		    uint32_t);
void		rnd_attach_source(krndsource_t *, const char *,
		    uint32_t, uint32_t);
void		rnd_detach_source(krndsource_t *);

void		rnd_getmore(size_t);

void		rnd_seed(void *, size_t);

static inline void
rnd_add_uint32(krndsource_t *kr, uint32_t val)
{
	if (__predict_true(kr) && RND_ENABLED(kr)) {
		_rnd_add_uint32(kr, val);
	} else {
		rnd_add_data(NULL, &val, sizeof(val), 0);
	}
}

extern int	rnd_empty;
extern int	rnd_full;
extern int	rnd_filled;
extern int	rnd_initial_entropy;

#endif /* _KERNEL */

#define	RND_MAXSTATCOUNT	10	/* 10 sources at once max */

/*
 * return "count" random entries, starting at "start"
 */
typedef struct {
	uint32_t	start;
	uint32_t	count;
	rndsource_t	source[RND_MAXSTATCOUNT];
} rndstat_t;

/*
 * return information on a specific source by name
 */
typedef struct {
	char		name[16];
	rndsource_t	source;
} rndstat_name_t;

/*
 * set/clear device flags.  If type is set to 0xff, the name is used
 * instead.  Otherwise, the flags set/cleared apply to all devices of
 * the specified type, and the name is ignored.
 */
typedef struct {
	char		name[16];	/* the name we are adjusting */
	uint32_t	type;		/* the type of device we want */
	uint32_t	flags;		/* flags to set or clear */
	uint32_t	mask;		/* mask for the flags we are setting */
} rndctl_t;

/*
 * Add entropy to the pool.  len is the data length, in bytes.
 * entropy is the number of bits of estimated entropy in the data.
 */
typedef struct {
	uint32_t	len;
	uint32_t	entropy;
	u_char		data[RND_SAVEWORDS * sizeof(uint32_t)];
} rnddata_t;

#define	RNDGETENTCNT	_IOR('R',  101, uint32_t) /* get entropy count */
#define	RNDGETSRCNUM	_IOWR('R', 102, rndstat_t) /* get rnd source info */
#define	RNDGETSRCNAME	_IOWR('R', 103, rndstat_name_t) /* get src by name */
#define	RNDCTL		_IOW('R',  104, rndctl_t)  /* set/clear source flags */
#define	RNDADDDATA	_IOW('R',  105, rnddata_t) /* add data to the pool */
#define	RNDGETPOOLSTAT	_IOR('R',  106, rndpoolstat_t) /* get statistics */

#endif /* !_SYS_RND_H_ */
