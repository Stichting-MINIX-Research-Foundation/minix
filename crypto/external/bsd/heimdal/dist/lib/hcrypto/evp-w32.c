/*	$NetBSD: evp-w32.c,v 1.2 2017/01/28 21:31:47 christos Exp $	*/

/*
 * Copyright (c) 2015, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Windows fallback provider: decides whether to use hcrypto or
 * wincng depending on whether bcrypt.dll is available (i.e. it
 * is runtime compatible back to XP, but will use the native
 * crypto APIs from Vista onwards).
 */

#include <config.h>
#include <krb5/roken.h>

#include <assert.h>

#include <evp.h>
#include <evp-w32.h>
#include <evp-hcrypto.h>

#include <evp-wincng.h>

static LONG wincng_available = -1;

static __inline int
wincng_check_availability(void)
{
    if (wincng_available == -1) {
	char szBCryptDllPath[MAX_PATH];
	UINT cbBCryptDllPath;

	cbBCryptDllPath = GetSystemDirectory(szBCryptDllPath,
					     sizeof(szBCryptDllPath));
	if (cbBCryptDllPath > 0 &&
	    cbBCryptDllPath < sizeof(szBCryptDllPath) &&
	    strncat_s(szBCryptDllPath,
		      sizeof(szBCryptDllPath), "\\bcrypt.dll", 11) == 0) {
	    HANDLE hBCryptDll = LoadLibrary(szBCryptDllPath);

	    InterlockedCompareExchangeRelease(&wincng_available,
					      !!hBCryptDll, -1);
	    if (hBCryptDll)
		FreeLibrary(hBCryptDll);
	}
    }

    return wincng_available == 1;
}

BOOL WINAPI
_hc_w32crypto_DllMain(HINSTANCE hinstDLL,
		      DWORD fdwReason,
		      LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_DETACH) {
	/*
	 * Don't bother cleaning up on process exit, only on
	 * FreeLibrary() (in which case lpvReserved will be NULL).
	 */
	if (lpvReserved == NULL)
	    _hc_wincng_cleanup();
    }

    return TRUE;
}

#define EVP_W32CRYPTO_PROVIDER(type, name)		    \
							    \
    const type *hc_EVP_w32crypto_ ##name (void)		    \
    {							    \
	if (wincng_check_availability())		    \
	    return hc_EVP_wincng_ ##name ();		    \
	else if (HCRYPTO_FALLBACK)			    \
	    return hc_EVP_hcrypto_ ##name ();		    \
	else						    \
	    return NULL;				    \
    }

#define EVP_W32CRYPTO_PROVIDER_CNG_UNAVAILABLE(type, name)  \
							    \
    const type *hc_EVP_w32crypto_ ##name (void)		    \
    {							    \
	return hc_EVP_hcrypto_ ##name ();		    \
    }

EVP_W32CRYPTO_PROVIDER(EVP_MD, md2)
EVP_W32CRYPTO_PROVIDER(EVP_MD, md4)
EVP_W32CRYPTO_PROVIDER(EVP_MD, md5)
EVP_W32CRYPTO_PROVIDER(EVP_MD, sha1)
EVP_W32CRYPTO_PROVIDER(EVP_MD, sha256)
EVP_W32CRYPTO_PROVIDER(EVP_MD, sha384)
EVP_W32CRYPTO_PROVIDER(EVP_MD, sha512)

EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, rc2_cbc)
EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, rc2_40_cbc)
EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, rc2_64_cbc)

EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, rc4)
EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, rc4_40)

EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, des_cbc)
EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, des_ede3_cbc)

EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, aes_128_cbc)
EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, aes_192_cbc)
EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, aes_256_cbc)

EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, aes_128_cfb8)
EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, aes_192_cfb8)
EVP_W32CRYPTO_PROVIDER(EVP_CIPHER, aes_256_cfb8)

EVP_W32CRYPTO_PROVIDER_CNG_UNAVAILABLE(EVP_CIPHER, camellia_128_cbc)
EVP_W32CRYPTO_PROVIDER_CNG_UNAVAILABLE(EVP_CIPHER, camellia_192_cbc)
EVP_W32CRYPTO_PROVIDER_CNG_UNAVAILABLE(EVP_CIPHER, camellia_256_cbc)
