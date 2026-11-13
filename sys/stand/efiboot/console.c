/* $NetBSD: console.c,v 1.2 2018/09/15 16:44:15 jmcneill Exp $ */

/*-
 * Copyright (c) 2018 Jared McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "efiboot.h"

static EFI_INPUT_KEY key_cur;
static int key_pending;

int
getchar(void)
{
	EFI_STATUS status;
	EFI_INPUT_KEY key;

	if (key_pending) {
		key = key_cur;
		key_pending = 0;
	} else {
		status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
		while (status == EFI_NOT_READY) {
			if (ST->ConIn->WaitForKey != NULL)
				WaitForSingleEvent(ST->ConIn->WaitForKey, 0);
			status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
		}
	}

	return key.UnicodeChar;
}

void
putchar(int c)
{
	CHAR16 buf[2] = { c, '\0' };
	if (c == '\n')
		putchar('\r');
	uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, buf);
}

int
ischar(void)
{
	EFI_INPUT_KEY key;
	EFI_STATUS status;

	if (ST->ConIn->WaitForKey == NULL) {
		if (key_pending)
			return 1;
		status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key);
		if (status == EFI_SUCCESS) {
			key_cur = key;
			key_pending = 1;
		}
		return key_pending;
	} else {
		status = uefi_call_wrapper(BS->CheckEvent, 1, ST->ConIn->WaitForKey);
		return status == EFI_SUCCESS;
	}
}
