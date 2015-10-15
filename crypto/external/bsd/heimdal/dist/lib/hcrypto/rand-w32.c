/*	$NetBSD: rand-w32.c,v 1.1.1.2 2014/04/24 12:45:30 pettai Exp $	*/

/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

#include <config.h>
#include <krb5/roken.h>

#include <wincrypt.h>

#include <stdio.h>
#include <stdlib.h>
#include <rand.h>
#include <heim_threads.h>

#include "randi.h"

volatile static HCRYPTPROV g_cryptprovider = 0;

static HCRYPTPROV
_hc_CryptProvider(void)
{
    BOOL rv;
    HCRYPTPROV cryptprovider = 0;

    if (g_cryptprovider != 0)
	return g_cryptprovider;

    rv = CryptAcquireContext(&cryptprovider, NULL,
			      MS_ENHANCED_PROV, PROV_RSA_FULL,
			      CRYPT_VERIFYCONTEXT);

    if (GetLastError() == NTE_BAD_KEYSET) {
        rv = CryptAcquireContext(&cryptprovider, NULL,
                                 MS_ENHANCED_PROV, PROV_RSA_FULL,
                                 CRYPT_NEWKEYSET);
    }

    if (rv) {
        /* try the default provider */
        rv = CryptAcquireContext(&cryptprovider, NULL, 0, PROV_RSA_FULL,
                                 CRYPT_VERIFYCONTEXT);

        if (GetLastError() == NTE_BAD_KEYSET) {
            rv = CryptAcquireContext(&cryptprovider, NULL,
                                     MS_ENHANCED_PROV, PROV_RSA_FULL,
                                     CRYPT_NEWKEYSET);
        }
    }

    if (rv) {
        /* try just a default random number generator */
        rv = CryptAcquireContext(&cryptprovider, NULL, 0, PROV_RNG,
                                 CRYPT_VERIFYCONTEXT);
    }

    if (rv &&
        InterlockedCompareExchangePointer((PVOID *) &g_cryptprovider,
					  (PVOID) cryptprovider, 0) != 0) {

        CryptReleaseContext(cryptprovider, 0);
        cryptprovider = g_cryptprovider;
    }

    return cryptprovider;
}

/*
 *
 */


static void
w32crypto_seed(const void *indata, int size)
{
}


static int
w32crypto_bytes(unsigned char *outdata, int size)
{
    if (CryptGenRandom(_hc_CryptProvider(), size, outdata))
	return 1;
    return 0;
}

static void
w32crypto_cleanup(void)
{
    HCRYPTPROV cryptprovider;

    if (InterlockedCompareExchangePointer((PVOID *) &cryptprovider,
					  0, (PVOID) g_cryptprovider) == 0) {
        CryptReleaseContext(cryptprovider, 0);
    }
}

static void
w32crypto_add(const void *indata, int size, double entropi)
{
}

static int
w32crypto_status(void)
{
    if (_hc_CryptProvider() == 0)
	return 0;
    return 1;
}

const RAND_METHOD hc_rand_w32crypto_method = {
    w32crypto_seed,
    w32crypto_bytes,
    w32crypto_cleanup,
    w32crypto_add,
    w32crypto_bytes,
    w32crypto_status
};

const RAND_METHOD *
RAND_w32crypto_method(void)
{
    return &hc_rand_w32crypto_method;
}
