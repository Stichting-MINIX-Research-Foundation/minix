/*	$NetBSD: smbios.c,v 1.4 2021/09/16 22:19:11 andvar Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * Copyright (c) 1999, by UCHIYAMA Yasushi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997-2001 Michael Shalayeff
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smbios.c,v 1.4 2021/09/16 22:19:11 andvar Exp $");

#include <sys/param.h>

#include "efiboot.h"
#include "smbios.h"

struct smbios_entry smbios_entry;

static void
smbios2_init(uint8_t *p)
{
	const struct smbhdr *sh = (const struct smbhdr *)p;

	smbios_entry.addr = (void *)(uintptr_t)sh->addr;
	smbios_entry.len = sh->size;
	smbios_entry.rev = 0;
	smbios_entry.mjr = sh->majrev;
	smbios_entry.min = sh->minrev;
	smbios_entry.doc = 0;
	smbios_entry.count = sh->count;
}

static void
smbios3_init(uint8_t *p)
{
	const struct smb3hdr *sh = (const struct smb3hdr *)p;

	smbios_entry.addr = (void *)(uintptr_t)sh->addr;
	smbios_entry.len = sh->size;
	smbios_entry.rev = sh->eprev;
	smbios_entry.mjr = sh->majrev;
	smbios_entry.min = sh->minrev;
	smbios_entry.doc = sh->docrev;
	smbios_entry.count = UINT16_MAX;
}

void
smbios_init(uint8_t *p)
{
	if (memcmp(p, "_SM3_", 5) == 0) {
		smbios3_init(p);
	} else if (memcmp(p, "_SM_", 4) == 0) {
		smbios2_init(p);
	}
}

/*
 * smbios_find_table() takes a caller supplied smbios struct type and
 * a pointer to a handle (struct smbtable) returning one if the structure
 * is successfully located and zero otherwise. Callers should take care
 * to initilize the cookie field of the smbtable structure to zero before
 * the first invocation of this function.
 * Multiple tables of the same type can be located by repeadtly calling
 * smbios_find_table with the same arguments.
 */
int
smbios_find_table(uint8_t type, struct smbtable *st)
{
	uint8_t *va, *end;
	struct smbtblhdr *hdr;
	int ret = 0, tcount = 1;

	if (smbios_entry.addr == 0) {
		return 0;
	}

	va = smbios_entry.addr;
	end = va + smbios_entry.len;

	/*
	 * The cookie field of the smtable structure is used to locate
	 * multiple instances of a table of an arbitrary type. Following the
	 * successful location of a table, the type is encoded as bits 0:7 of
	 * the cookie value, the offset in terms of the number of structures
	 * preceding that referenced by the handle is encoded in bits 15:31.
	 */
	if ((st->cookie & 0xfff) == type && st->cookie >> 16) {
		if ((uint8_t *)st->hdr >= va && (uint8_t *)st->hdr < end) {
			hdr = st->hdr;
			if (hdr->type == type) {
				va = (uint8_t *)hdr + hdr->size;
				for (; va + 1 < end; va++)
					if (*va == 0 && *(va + 1) == 0)
						break;
				va+= 2;
				tcount = st->cookie >> 16;
			}
		}
	}
	for (; va + sizeof(struct smbtblhdr) < end && tcount <=
	    smbios_entry.count; tcount++) {
		hdr = (struct smbtblhdr *)va;
		if (hdr->type == type) {
			ret = 1;
			st->hdr = hdr;
			st->tblhdr = va + sizeof(struct smbtblhdr);
			st->cookie = (tcount + 1) << 16 | type;
			break;
		}
		if (hdr->type == SMBIOS_TYPE_EOT)
			break;
		va+= hdr->size;
		for (; va + 1 < end; va++)
			if (*va == 0 && *(va + 1) == 0)
				break;
		va+=2;
	}

	return ret;
}

char *
smbios_get_string(struct smbtable *st, uint8_t indx, char *dest, size_t len)
{
	uint8_t *va, *end;
	char *ret = NULL;
	int i;

	if (smbios_entry.addr == 0) {
		return NULL;
	}

	va = (uint8_t *)st->hdr + st->hdr->size;
	end = smbios_entry.addr + smbios_entry.len;
	for (i = 1; va < end && i < indx && *va; i++)
		while (*va++)
			;
	if (i == indx) {
		if (va + len < end) {
			ret = dest;
			memcpy(ret, va, len);
			ret[len - 1] = '\0';
		}
	}

	return ret;
}
