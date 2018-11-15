/*	$NetBSD: crypto-rand.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "krb5_locl.h"

#undef HEIMDAL_WARN_UNUSED_RESULT_ATTRIBUTE
#define HEIMDAL_WARN_UNUSED_RESULT_ATTRIBUTE

#define ENTROPY_NEEDED 128

static HEIMDAL_MUTEX crypto_mutex = HEIMDAL_MUTEX_INITIALIZER;

static int
seed_something(void)
{
#ifndef NO_RANDFILE
    char buf[1024], seedfile[256];

    /* If there is a seed file, load it. But such a file cannot be trusted,
       so use 0 for the entropy estimate */
    if (RAND_file_name(seedfile, sizeof(seedfile))) {
	int fd;
	fd = open(seedfile, O_RDONLY | O_BINARY | O_CLOEXEC);
	if (fd >= 0) {
	    ssize_t ret;
	    rk_cloexec(fd);
	    ret = read(fd, buf, sizeof(buf));
	    if (ret > 0)
		RAND_add(buf, ret, 0.0);
	    close(fd);
	} else
	    seedfile[0] = '\0';
    } else
	seedfile[0] = '\0';
#endif

    /* Calling RAND_status() will try to use /dev/urandom if it exists so
       we do not have to deal with it. */
    if (RAND_status() != 1) {
	/* TODO: Once a Windows CryptoAPI RAND method is defined, we
	   can use that and failover to another method. */
    }

    if (RAND_status() == 1)	{
#ifndef NO_RANDFILE
	/* Update the seed file */
	if (seedfile[0])
	    RAND_write_file(seedfile);
#endif

	return 0;
    } else
	return -1;
}

/**
 * Fill buffer buf with len bytes of PRNG randomness that is ok to use
 * for key generation, padding and public diclosing the randomness w/o
 * disclosing the randomness source.
 *
 * This function can fail, and callers must check the return value.
 *
 * @param buf a buffer to fill with randomness
 * @param len length of memory that buf points to.
 *
 * @return return 0 on success or HEIM_ERR_RANDOM_OFFLINE if the
 * funcation failed to initialize the randomness source.
 *
 * @ingroup krb5_crypto
 */

HEIMDAL_WARN_UNUSED_RESULT_ATTRIBUTE
KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_generate_random(void *buf, size_t len)
{
    static int rng_initialized = 0;
    int ret;

    HEIMDAL_MUTEX_lock(&crypto_mutex);
    if (!rng_initialized) {
	if (seed_something()) {
            HEIMDAL_MUTEX_unlock(&crypto_mutex);
	    return HEIM_ERR_RANDOM_OFFLINE;
        }
	rng_initialized = 1;
    }
    if (RAND_bytes(buf, len) <= 0)
	ret = HEIM_ERR_RANDOM_OFFLINE;
    else
	ret = 0;
    HEIMDAL_MUTEX_unlock(&crypto_mutex);

    return ret;
}

/**
 * Fill buffer buf with len bytes of PRNG randomness that is ok to use
 * for key generation, padding and public diclosing the randomness w/o
 * disclosing the randomness source.
 *
 * This function can NOT fail, instead it will abort() and program will crash.
 *
 * If this function is called after a successful krb5_init_context(),
 * the chance of it failing is low due to that krb5_init_context()
 * pulls out some random, and quite commonly the randomness sources
 * will not fail once it have started to produce good output,
 * /dev/urandom behavies that way.
 *
 * @param buf a buffer to fill with randomness
 * @param len length of memory that buf points to.
 *
 * @ingroup krb5_crypto
 */


KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_generate_random_block(void *buf, size_t len)
{
    int ret = krb5_generate_random(buf, len);
    if (ret)
	krb5_abortx(NULL, "Failed to generate random block");
}
