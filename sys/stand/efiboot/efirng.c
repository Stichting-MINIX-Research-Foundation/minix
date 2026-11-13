/*	$NetBSD: efirng.c,v 1.4 2022/08/14 11:26:41 jmcneill Exp $	*/

/*-
 * Copyright (c) 2020 The NetBSD Foundation, Inc.
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
 * UEFI Forum, Inc.: UEFI Specification, Version 2.8 Errata A, February
 * 2020, Sec. 37.5 EFI Random Number Generator Protocol, pp. 2158--2162
 * https://uefi.org/sites/default/files/resources/UEFI_Spec_2_8_A_Feb14.pdf
 */

#include "efirng.h"

#include "efiboot.h"

static EFI_GUID RngProtocolGuid = EFI_RNG_PROTOCOL_GUID;
static EFI_GUID RngAlgorithmRawGuid = EFI_RNG_ALGORITHM_RAW;
static EFI_RNG_PROTOCOL *rng;

#ifndef EFIBOOT_DEBUG
#define	DPRINT(...)	__nothing
#else
#define	DPRINT		Print
#endif

static const struct {
	EFI_GUID guid;
	const CHAR16 *name;
} algname[] = {
	{EFI_RNG_ALGORITHM_SP800_90_HASH_256_GUID,
	 L"NIST SP800-90 Hash_DRBG SHA-256"},
	{EFI_RNG_ALGORITHM_SP800_90_HMAC_256_GUID,
	 L"NIST SP800-90 HMAC_DRBG SHA-256"},
	{EFI_RNG_ALGORITHM_SP800_90_CTR_256_GUID,
	 L"NIST SP800-90 CTR_DRBG AES-256"},
	{EFI_RNG_ALGORITHM_X9_31_3DES_GUID, L"ANSI X9.31 3DES"},
	{EFI_RNG_ALGORITHM_X9_31_AES_GUID, L"ANSI X9.31 AES"},
	{EFI_RNG_ALGORITHM_RAW, L"raw"},
};

void
efi_rng_probe(void)
{
	EFI_STATUS status;

	/* Get the RNG protocol.  */
	status = LibLocateProtocol(&RngProtocolGuid, (void **)&rng);
	if (EFI_ERROR(status)) {
		DPRINT(L"efirng: protocol: %r\n", status);
		rng = NULL;
		return;
	}
}

void
efi_rng_show(void)
{
	EFI_RNG_ALGORITHM alglist[10];
	UINTN i, j, alglistsz = sizeof(alglist);
	EFI_STATUS status;

	if (!efi_rng_available())
		return;

	command_printtab("RNG", "");

	/* Query the list of supported algorithms.  */
	status = uefi_call_wrapper(rng->GetInfo, 3, rng, &alglistsz, alglist);
	if (EFI_ERROR(status)) {
		Print(L"GetInfo: %r\n", status);
		return;
	}

	/* Print the list of supported algorithms.  */
	for (i = 0; i < alglistsz/sizeof(alglist[0]); i++) {
		const CHAR16 *name = L"[unknown]";
		for (j = 0; j < __arraycount(algname); j++) {
			if (memcmp(&alglist[i], &algname[j].guid,
				sizeof(EFI_GUID)) == 0) {
				name = algname[j].name;
				break;
			}
		}
		Print(L"%s (%g)\n", name, &alglist[i]);
	}
}

int
efi_rng_available(void)
{

	return rng != NULL;
}

int
efi_rng(void *buf, UINTN len)
{
	EFI_STATUS status;

	if (!efi_rng_available())
		return EIO;

	status = uefi_call_wrapper(rng->GetRNG, 4, rng, &RngAlgorithmRawGuid,
	    len, buf);
	if (status == EFI_UNSUPPORTED) {
		/*
		 * Fall back to any supported RNG `algorithm' even
		 * though we would prefer raw samples.
		 */
		status = uefi_call_wrapper(rng->GetRNG, 4, rng, NULL, len, buf);
	}
	if (EFI_ERROR(status)) {
		DPRINT(L"efirng: GetRNG: %r\n", status);
		return EIO;
	}

	return 0;
}
