/*	$NetBSD: arc4random.c,v 1.9 2005/12/24 21:11:16 perry Exp $	*/
/*	$OpenBSD: arc4random.c,v 1.6 2001/06/05 05:05:38 pvalchev Exp $	*/

/*
 * Arc4 random number generator for OpenBSD.
 * Copyright 1996 David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project by leaving this copyright notice intact.
 */

/*
 * This code is derived from section 17.1 of Applied Cryptography,
 * second edition, which describes a stream cipher allegedly
 * compatible with RSA Labs "RC4" cipher (the actual description of
 * which is a trade secret).  The same algorithm is used as a stream
 * cipher called "arcfour" in Tatu Ylonen's ssh package.
 *
 * Here the stream cipher has been modified always to include the time
 * when initializing the state.  That makes it impossible to
 * regenerate the same random sequence twice, so this can't be used
 * for encryption, but will generate good random numbers.
 *
 * RC4 is a registered trademark of RSA Laboratories.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: arc4random.c,v 1.9 2005/12/24 21:11:16 perry Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/sysctl.h>

#ifdef __weak_alias
__weak_alias(arc4random,_arc4random)
#endif

struct arc4_stream {
	u_int8_t i;
	u_int8_t j;
	u_int8_t s[256];
};

static int rs_initialized;
static struct arc4_stream rs;

static inline void arc4_init(struct arc4_stream *);
static inline void arc4_addrandom(struct arc4_stream *, u_char *, int);
static void arc4_stir(struct arc4_stream *);
static inline u_int8_t arc4_getbyte(struct arc4_stream *);
static inline u_int32_t arc4_getword(struct arc4_stream *);

static inline void
arc4_init(as)
	struct arc4_stream *as;
{
	int     n;

	for (n = 0; n < 256; n++)
		as->s[n] = n;
	as->i = 0;
	as->j = 0;
}

static inline void
arc4_addrandom(as, dat, datlen)
	struct arc4_stream *as;
	u_char *dat;
	int     datlen;
{
	int     n;
	u_int8_t si;

	as->i--;
	for (n = 0; n < 256; n++) {
		as->i = (as->i + 1);
		si = as->s[as->i];
		as->j = (as->j + si + dat[n % datlen]);
		as->s[as->i] = as->s[as->j];
		as->s[as->j] = si;
	}
	as->j = as->i;
}

static void
arc4_stir(as)
	struct arc4_stream *as;
{
	int     fd;
	struct {
		struct timeval tv;
		u_int rnd[(128 - sizeof(struct timeval)) / sizeof(u_int)];
	}       rdat;
	int	n;

	gettimeofday(&rdat.tv, NULL);
	fd = open("/dev/urandom", O_RDONLY);
	if (fd != -1) {
		read(fd, rdat.rnd, sizeof(rdat.rnd));
		close(fd);
	}
#ifdef KERN_URND
	else {
		int mib[2];
		u_int i;
		size_t len;

		/* Device could not be opened, we might be chrooted, take
		 * randomness from sysctl. */

		mib[0] = CTL_KERN;
		mib[1] = KERN_URND;

		for (i = 0; i < sizeof(rdat.rnd) / sizeof(u_int); i++) {
			len = sizeof(u_int);
			if (sysctl(mib, 2, &rdat.rnd[i], &len, NULL, 0) == -1)
				break;
		}
	}
#endif
	/* fd < 0 or failed sysctl ?  Ah, what the heck. We'll just take
	 * whatever was on the stack... */

	arc4_addrandom(as, (void *) &rdat, sizeof(rdat));

	/*
	 * Throw away the first N words of output, as suggested in the
	 * paper "Weaknesses in the Key Scheduling Algorithm of RC4"
	 * by Fluher, Mantin, and Shamir.  (N = 256 in our case.)
	 */
	for (n = 0; n < 256 * 4; n++)
		arc4_getbyte(as);
}

static inline u_int8_t
arc4_getbyte(as)
	struct arc4_stream *as;
{
	u_int8_t si, sj;

	as->i = (as->i + 1);
	si = as->s[as->i];
	as->j = (as->j + si);
	sj = as->s[as->j];
	as->s[as->i] = sj;
	as->s[as->j] = si;
	return (as->s[(si + sj) & 0xff]);
}

static inline u_int32_t
arc4_getword(as)
	struct arc4_stream *as;
{
	u_int32_t val;
	val = arc4_getbyte(as) << 24;
	val |= arc4_getbyte(as) << 16;
	val |= arc4_getbyte(as) << 8;
	val |= arc4_getbyte(as);
	return val;
}

void
arc4random_stir()
{
	if (!rs_initialized) {
		arc4_init(&rs);
		rs_initialized = 1;
	}
	arc4_stir(&rs);
}

void
arc4random_addrandom(dat, datlen)
	u_char *dat;
	int     datlen;
{
	if (!rs_initialized)
		arc4random_stir();
	arc4_addrandom(&rs, dat, datlen);
}

u_int32_t
arc4random()
{
	if (!rs_initialized)
		arc4random_stir();
	return arc4_getword(&rs);
}

#if 0
/*-------- Test code for i386 --------*/
#include <stdio.h>
#include <machine/pctr.h>
int
main(int argc, char **argv)
{
	const int iter = 1000000;
	int     i;
	pctrval v;

	v = rdtsc();
	for (i = 0; i < iter; i++)
		arc4random();
	v = rdtsc() - v;
	v /= iter;

	printf("%qd cycles\n", v);
}
#endif
