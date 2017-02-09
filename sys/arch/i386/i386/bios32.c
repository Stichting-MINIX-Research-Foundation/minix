/*	$NetBSD: bios32.c,v 1.29 2012/06/15 23:01:16 joerg Exp $	*/

/*-
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

/*
 * Basic interface to BIOS32 services.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bios32.c,v 1.29 2012/06/15 23:01:16 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h> 
#include <sys/malloc.h>

#include <dev/isa/isareg.h>
#include <machine/isa_machdep.h>

#include <machine/segments.h>
#include <machine/bios32.h>
#include <x86/smbiosvar.h>

#include <uvm/uvm.h>

#include "ipmi.h"
#include "opt_xen.h"

#define	BIOS32_START	0xe0000
#define	BIOS32_SIZE	0x20000
#define	BIOS32_END	(BIOS32_START + BIOS32_SIZE - 0x10)

struct bios32_entry bios32_entry;
struct smbios_entry smbios_entry;

/*
 * Initialize the BIOS32 interface.
 */
void
bios32_init(void)
{
	paddr_t entry = 0;
	char *p;
	unsigned char cksum;
	int i;

	for (p = (char *)ISA_HOLE_VADDR(BIOS32_START);
	     p < (char *)ISA_HOLE_VADDR(BIOS32_END);
	     p += 16) {
		if (*(int *)p != BIOS32_MAKESIG('_', '3', '2', '_'))
			continue;

		cksum = 0;
		for (i = 0; i < 16; i++)
			cksum += *(unsigned char *)(p + i);
		if (cksum != 0)
			continue;

		if (*(p + 9) != 1)
			continue;

		entry = *(uint32_t *)(p + 4);

		aprint_debug("BIOS32 rev. %d found at 0x%lx\n",
		    *(p + 8), (u_long)entry);

		if (entry < BIOS32_START ||
		    entry >= BIOS32_END) {
			aprint_error("BIOS32 entry point outside "
			    "allowable range\n");
			entry = 0;
		}
		break;
	}

	if (entry != 0) {
		bios32_entry.offset = (void *)ISA_HOLE_VADDR(entry);
		bios32_entry.segment = GSEL(GCODE_SEL, SEL_KPL);
	}
	/* see if we have SMBIOS extensions */
	for (p = ISA_HOLE_VADDR(SMBIOS_START);
	    p < (char *)ISA_HOLE_VADDR(SMBIOS_END); p+= 16) {
		struct smbhdr * sh = (struct smbhdr *)p;
		uint8_t chksum;
		vaddr_t eva;
		paddr_t pa, end;

		if (sh->sig != BIOS32_MAKESIG('_', 'S', 'M', '_'))
			continue;
		i = sh->len;
		for (chksum = 0; i--; chksum += p[i])
			;
		if (chksum != 0)
			continue;
		p += 0x10;
		if (p[0] != '_' && p[1] != 'D' && p[2] != 'M' &&
		    p[3] != 'I' && p[4] != '_')
			continue;
		for (chksum = 0, i = 0xf; i--;)
			chksum += p[i];
		if (chksum != 0)
			continue;

		pa = trunc_page(sh->addr);
		end = round_page(sh->addr + sh->size);
		eva = uvm_km_alloc(kernel_map, end - pa, 0, UVM_KMF_VAONLY);
		if (eva == 0)
			break;

		smbios_entry.addr = (uint8_t *)(eva +
		    (sh->addr & PGOFSET));
		smbios_entry.len = sh->size;
		smbios_entry.mjr = sh->majrev;
		smbios_entry.min = sh->minrev;
		smbios_entry.count = sh->count;

    		for (; pa < end; pa+= NBPG, eva+= NBPG)
#ifdef XEN
			pmap_kenter_ma(eva, pa, VM_PROT_READ, 0);
#else
			pmap_kenter_pa(eva, pa, VM_PROT_READ, 0);
#endif

		aprint_debug("SMBIOS rev. %d.%d @ 0x%lx (%d entries)\n",
		    sh->majrev, sh->minrev, (u_long)sh->addr,
		    sh->count);

		break;
	}
	pmap_update(pmap_kernel());
}

/*
 * Call BIOS32 to locate the specified BIOS32 service, and fill
 * in the entry point information.
 */
int
bios32_service(uint32_t service, bios32_entry_t e, bios32_entry_info_t ei)
{
	uint32_t eax, ebx, ecx, edx;
	paddr_t entry;

	if (bios32_entry.offset == 0)
		return (0);	/* BIOS32 not present */

	__asm volatile("lcall *(%%edi)"
		: "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
		: "0" (service), "1" (0), "D" (&bios32_entry));

	if ((eax & 0xff) != 0)
		return (0);	/* service not found */

	entry = ebx + edx;

	if (entry < BIOS32_START || entry >= BIOS32_END) {
		aprint_error("BIOS32: entry point for service %c%c%c%c is "
		    "outside allowable range\n",
		    service & 0xff,
		    (service >> 8) & 0xff,
		    (service >> 16) & 0xff,
		    (service >> 24) & 0xff);
		return (0);
	}

	e->offset = (void *)ISA_HOLE_VADDR(entry);
	e->segment = GSEL(GCODE_SEL, SEL_KPL);

	ei->bei_base = ebx;
	ei->bei_size = ecx;
	ei->bei_entry = entry;

	return (1);
}

/*
 * smbios_find_table() takes a caller supplied smbios struct type and
 * a pointer to a handle (struct smbtable) returning one if the structure
 * is sucessfully located and zero otherwise. Callers should take care
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

	va = smbios_entry.addr;
	end = va + smbios_entry.len;

	/*
	 * The cookie field of the smtable structure is used to locate
	 * multiple instances of a table of an arbitrary type. Following the
	 * sucessful location of a table, the type is encoded as bits 0:7 of
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
