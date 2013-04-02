/*
 * Copyright (c) 1999,2000,2004 Damien Miller <djm@mindrot.org>
 * Copyright (c) 2004 Darren Tucker <dtucker at zip.com.au>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"
#include "ntpd.h"

/* RCSID("$Id: bsd-arc4random.c,v 1.14 2005/05/29 14:01:08 dtucker Exp $"); */

#ifndef HAVE_ARC4RANDOM

#ifdef HAVE_OPENSSL
# include <openssl/rand.h>
# include <openssl/rc4.h>
# include <openssl/err.h>
#else

#include <errno.h>
#include <sys/types.h>
#include <sys/un.h>

enum entropy_dev_type {
	none,
	dev,	/* regular /dev/random type device special */
	egd	/* EGD/PRNGD socket */
};

struct entropy_dev_struct {
	const char *device;
	enum entropy_dev_type type;
} entropy_dev[] = {
	{ "/dev/urandom",	dev },
	{ "/dev/arandom",	dev },
	{ "/dev/random",	dev },
	{ "/var/run/egd-pool",	egd },
	{ "/dev/egd-pool",	egd },
	{ "/etc/egd-pool",	egd },
	{ NULL,			none }
};

/*
 * arcfour routines from "nanocrypt" also by Damien Miller, included with
 * permission under the license above.
 * Converted to OpenSSL API by Darren Tucker.
 */

static int randfd = -1;
static const char *randdev = NULL;
static enum entropy_dev_type randtype = none;

typedef struct {
	unsigned int s[256];
	int i;
	int j;
} RC4_KEY;

static void
RC4_set_key(RC4_KEY *r, int len, unsigned char *key)
{
	int t;
	
	for(r->i = 0; r->i < 256; r->i++)
		r->s[r->i] = r->i;

	r->j = 0;
	for(r->i = 0; r->i < 256; r->i++)
	{
		r->j = (r->j + r->s[r->i] + key[r->i % len]) % 256;
		t = r->s[r->i];
		r->s[r->i] = r->s[r->j];
		r->s[r->j] = t;
	}
	r->i = r->j = 0;
}

static void
RC4(RC4_KEY *r, unsigned long len, const unsigned char *plaintext,
    unsigned char *ciphertext )
{
	int t;
	unsigned long c;

	c = 0;
	while(c < len)
	{
		r->i = (r->i + 1) % 256;
		r->j = (r->j + r->s[r->i]) % 256;
		t = r->s[r->i];
		r->s[r->i] = r->s[r->j];
		r->s[r->j] = t;

		t = (r->s[r->i] + r->s[r->j]) % 256;

		ciphertext[c] = plaintext[c] ^ r->s[t];
		c++;
	}
}

static int
RAND_status(void)
{
	int i;
	socklen_t len;
	struct sockaddr_un sa;

	if (randfd >= 0 && randtype != none)
		return 1;

	/* search for an entropy source */
	for (i = 0; entropy_dev[i].device != NULL && randtype == none; i++ ) {
		randdev = entropy_dev[i].device;
		switch (entropy_dev[i].type) {
		case dev:
			randfd = open(randdev, O_RDONLY);
			if (randfd >= 0) /* success */
				randtype = entropy_dev[i].type;
			break;
		case egd:
			if ((randfd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1) {
				log_warn("socket");
				break;
			}
			memset((void *)&sa, 0, sizeof(sa));
			sa.sun_family = AF_UNIX;
			strlcpy(sa.sun_path, randdev, sizeof(sa.sun_path));
			len = sizeof(sa);
			if (connect(randfd, (struct sockaddr*)&sa, len) == -1) {
				close(randfd);
				randfd = -1;
			} else {
				randtype = entropy_dev[i].type;
			}
			break;
		default:
			fatal("RAND_status internal error");
		}
	}

	if (randfd >= 0 && randtype != none)
		return 1;
	return(0);
}

static int
RAND_bytes(unsigned char *buf, size_t len)
{
	if (randtype == egd) {
		char egdcmd[2];

		if (len > 255)
			fatal("requested more than 255 bytes from egd");
		egdcmd[0] = 0x02;	/* blocking read */
		egdcmd[1] = (unsigned char)len;
		if (atomicio(vwrite, randfd, egdcmd, 2) == 0)
			fatal("write to egd socket");
	}
	if (atomicio(read, randfd, buf, len) != len)
		return 0;
	return 1;
}
#endif

/* Size of key to use */
#define SEED_SIZE 20

/* Number of bytes to reseed after */
#define REKEY_BYTES	(1 << 24)

static int rc4_ready = 0;
static RC4_KEY rc4;

void
seed_rng(void)
{
	if (RAND_status() != 1)
		fatal("PRNG is not seeded");
}

unsigned int arc4random(void)
{
	unsigned int r = 0;
	static int first_time = 1;

	if (rc4_ready <= 0) {
		if (first_time)
			seed_rng();
		first_time = 0;
		arc4random_stir();
	}

	RC4(&rc4, sizeof(r), (unsigned char *)&r, (unsigned char *)&r);

	rc4_ready -= sizeof(r);
	
	return(r);
}

void arc4random_stir(void)
{
	unsigned char rand_buf[SEED_SIZE];
	int i;

	memset(&rc4, 0, sizeof(rc4));
	if (RAND_bytes(rand_buf, sizeof(rand_buf)) <= 0)
		fatal("Couldn't obtain random bytes");
	RC4_set_key(&rc4, sizeof(rand_buf), rand_buf);

	/*
	 * Discard early keystream, as per recommendations in:
	 * http://www.wisdom.weizmann.ac.il/~itsik/RC4/Papers/Rc4_ksa.ps
	 */
	for(i = 0; i <= 256; i += sizeof(rand_buf))
		RC4(&rc4, sizeof(rand_buf), rand_buf, rand_buf);

	RC4(&rc4, sizeof(rand_buf), rand_buf, rand_buf);
	memset(rand_buf, 0, sizeof(rand_buf));

	rc4_ready = REKEY_BYTES;
}
#endif /* !HAVE_ARC4RANDOM */
