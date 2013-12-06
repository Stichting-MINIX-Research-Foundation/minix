/* $NetBSD: crypt-sha1.c,v 1.8 2013/08/28 17:47:07 riastradh Exp $ */

/*
 * Copyright (c) 2004, Juniper Networks, Inc.
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
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: crypt-sha1.c,v 1.8 2013/08/28 17:47:07 riastradh Exp $");
#endif /* not lint */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <err.h>
#include "crypt.h"

/*
 * The default iterations - should take >0s on a fast CPU
 * but not be insane for a slow CPU.
 */
#ifndef CRYPT_SHA1_ITERATIONS
# define CRYPT_SHA1_ITERATIONS 24680
#endif
/*
 * Support a reasonably? long salt.
 */
#ifndef CRYPT_SHA1_SALT_LENGTH
# define CRYPT_SHA1_SALT_LENGTH 64
#endif

/*
 * This may be called from crypt_sha1 or gensalt.
 *
 * The value returned will be slightly less than <hint> which defaults
 * to 24680.  The goals are that the number of iterations should take
 * non-zero amount of time on a fast cpu while not taking insanely
 * long on a slow cpu.  The current default will take about 5 seconds
 * on a 100MHz sparc, and about 0.04 seconds on a 3GHz i386.
 * The number is varied to frustrate those attempting to generate a
 * dictionary of pre-computed hashes.
 */
unsigned int
__crypt_sha1_iterations (unsigned int hint)
{
    static int once = 1;

    /*
     * We treat CRYPT_SHA1_ITERATIONS as a hint.
     * Make it harder for someone to pre-compute hashes for a
     * dictionary attack by not using the same iteration count for
     * every entry.
     */

    if (once) {
	int pid = getpid();
	
	srandom(time(NULL) ^ (pid * pid));
	once = 0;
    }
    if (hint == 0)
	hint = CRYPT_SHA1_ITERATIONS;
    return hint - (random() % (hint / 4));
}

/*
 * UNIX password using hmac_sha1
 * This is PBKDF1 from RFC 2898, but using hmac_sha1.
 *
 * The format of the encrypted password is:
 * $<tag>$<iterations>$<salt>$<digest>
 *
 * where:
 * 	<tag>		is "sha1"
 *	<iterations>	is an unsigned int identifying how many rounds
 * 			have been applied to <digest>.  The number
 * 			should vary slightly for each password to make
 * 			it harder to generate a dictionary of
 * 			pre-computed hashes.  See crypt_sha1_iterations.
 * 	<salt>		up to 64 bytes of random data, 8 bytes is
 * 			currently considered more than enough.
 *	<digest>	the hashed password.
 *
 * NOTE:
 * To be FIPS 140 compliant, the password which is used as a hmac key,
 * should be between 10 and 20 characters to provide at least 80bits
 * strength, and avoid the need to hash it before using as the 
 * hmac key.
 */
char *
__crypt_sha1 (const char *pw, const char *salt)
{
    static const char *magic = SHA1_MAGIC;
    static unsigned char hmac_buf[SHA1_SIZE];
    static char passwd[(2 * sizeof(SHA1_MAGIC)) +
		       CRYPT_SHA1_SALT_LENGTH + SHA1_SIZE];
    const char *sp;
    char *ep;
    unsigned long ul;
    int sl;
    int pl;
    int dl;
    unsigned int iterations;
    unsigned int i;
    /* XXX silence -Wpointer-sign (would be nice to fix this some other way) */
    const unsigned char *pwu = (const unsigned char *)pw;

    /*
     * Salt format is
     * $<tag>$<iterations>$salt[$]
     * If it does not start with $ we use our default iterations.
     */

    /* If it starts with the magic string, then skip that */
    if (!strncmp(salt, magic, strlen(magic))) {
	salt += strlen(magic);
	/* and get the iteration count */
	iterations = strtoul(salt, &ep, 10);
	if (*ep != '$')
	    return NULL;		/* invalid input */
	salt = ep + 1;			/* skip over the '$' */
    } else {
	iterations = __crypt_sha1_iterations(0);
    }

    /* It stops at the next '$', max CRYPT_SHA1_ITERATIONS chars */
    for (sp = salt; *sp && *sp != '$' && sp < (salt + CRYPT_SHA1_ITERATIONS); sp++)
	continue;

    /* Get the length of the actual salt */
    sl = sp - salt;
    pl = strlen(pw);

    /*
     * Now get to work...
     * Prime the pump with <salt><magic><iterations>
     */
    dl = snprintf(passwd, sizeof (passwd), "%.*s%s%u", 
		  sl, salt, magic, iterations);
    /*
     * Then hmac using <pw> as key, and repeat...
     */
    __hmac_sha1((unsigned char *)passwd, dl, pwu, pl, hmac_buf);
    for (i = 1; i < iterations; i++) {
	__hmac_sha1(hmac_buf, SHA1_SIZE, pwu, pl, hmac_buf);
    }
    /* Now output... */
    pl = snprintf(passwd, sizeof(passwd), "%s%u$%.*s$",
		  magic, iterations, sl, salt);
    ep = passwd + pl;

    /* Every 3 bytes of hash gives 24 bits which is 4 base64 chars */
    for (i = 0; i < SHA1_SIZE - 3; i += 3) {
	ul = (hmac_buf[i+0] << 16) |
	    (hmac_buf[i+1] << 8) |
	    hmac_buf[i+2];
	__crypt_to64(ep, ul, 4); ep += 4;
    }
    /* Only 2 bytes left, so we pad with byte0 */
    ul = (hmac_buf[SHA1_SIZE - 2] << 16) |
	(hmac_buf[SHA1_SIZE - 1] << 8) |
	hmac_buf[0];
    __crypt_to64(ep, ul, 4); ep += 4;
    *ep = '\0';

    /* Don't leave anything around in vm they could use. */
    explicit_memset(hmac_buf, 0, sizeof hmac_buf);

    return passwd;
}	
