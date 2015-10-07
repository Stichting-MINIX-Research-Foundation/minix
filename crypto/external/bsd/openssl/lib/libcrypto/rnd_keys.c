/*	$NetBSD: rnd_keys.c,v 1.1 2009/07/19 23:30:44 christos Exp $	*/

#include "des_locl.h"
#include <sys/time.h>
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>

#include <sha1.h>

void
des_set_random_generator_seed(des_cblock *seed)
{

	des_random_seed(seed);
}

/*
 * Generate a sequence of random des keys
 * using the random block sequence, fixup
 * parity and skip weak keys.
 */
int
des_new_random_key(des_cblock *key)
{
	int urandom;

 again:
	urandom = open("/dev/urandom", O_RDONLY);

	if (urandom < 0)
		des_random_key(key);
	else {
		if (read(urandom, key,
		    sizeof(des_cblock)) != sizeof(des_cblock)) {
			close(urandom);
			des_random_key(key);
		} else
			close(urandom);
	}

	/* random key must have odd parity and not be weak */
	des_set_odd_parity(key);
	if (des_is_weak_key(key))
		goto again;

	return (0);
}

/*
 * des_init_random_number_generator:
 *
 * This routine takes a secret key possibly shared by a number of servers
 * and uses it to generate a random number stream that is not shared by
 * any of the other servers.  It does this by using the current process id,
 * host id, and the current time to the nearest second.  The resulting
 * stream seed is not useful information for cracking the secret key.
 * Moreover, this routine keeps no copy of the secret key.
 */
void
des_init_random_number_generator(des_cblock *seed)
{
	u_int64_t seed_q;
	des_cblock seed_new;
	SHA1_CTX sha;

	u_char results[20];
	char hname[64], accum[512];

	struct timeval when;

	SHA1Init(&sha);

	gethostname(hname, sizeof(hname - 1));
	gettimeofday(&when, NULL);

	memcpy(&seed_q, seed, sizeof(seed_q));

	snprintf(accum, sizeof(accum), "%ld%ld%d%s%d%lld",
	    when.tv_sec, when.tv_usec, getpid(), hname, getuid(),
	    (long long) seed_q);

	SHA1Update(&sha, (u_char *) accum, strlen(accum));

	memset(accum, 0, sizeof(accum));

	SHA1Final(results, &sha);

	memcpy(seed_new, results, sizeof(seed_new));
	des_random_seed(&seed_new);

	memset(seed_new, 0, sizeof(seed_new));
	memset(results, 0, sizeof(results));
}
