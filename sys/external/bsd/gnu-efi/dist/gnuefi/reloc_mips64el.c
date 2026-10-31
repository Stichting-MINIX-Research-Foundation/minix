/*	$NetBSD: reloc_mips64el.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

/* reloc_mips64el.c - position independent MIPS64 ELF shared object relocator
   Copyright (C) 2014 Linaro Ltd. <ard.biesheuvel@linaro.org>
   Copyright (C) 1999 Hewlett-Packard Co.
	Contributed by David Mosberger <davidm@hpl.hp.com>.
   Copyright (C) 2017 Lemote Co.
	Contributed by Heiher <r@hev.cc>

    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials
      provided with the distribution.
    * Neither the name of Hewlett-Packard Co. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
    CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
    BE LIABLE FOR ANYDIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
    OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
    THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/

#include <efi.h>
#include <efilib.h>

#include <elf.h>

EFI_STATUS _relocate (long ldbase, Elf64_Dyn *dyn,
		      EFI_HANDLE image EFI_UNUSED,
		      EFI_SYSTEM_TABLE *systab EFI_UNUSED)
{
	long relsz = 0, relent = 0, gotsz = 0;
	Elf64_Rel *rel = 0;
	unsigned long *addr = 0;
	int i;

	for (i = 0; dyn[i].d_tag != DT_NULL; ++i) {
		switch (dyn[i].d_tag) {
			case DT_REL:
				rel = (Elf64_Rel*)
					((unsigned long)dyn[i].d_un.d_ptr
					 + ldbase);
				break;

			case DT_RELSZ:
				relsz = dyn[i].d_un.d_val;
				break;

			case DT_RELENT:
				relent = dyn[i].d_un.d_val;
				break;

			case DT_PLTGOT:
				addr = (unsigned long *)
					((unsigned long)dyn[i].d_un.d_ptr
					 + ldbase);
				break;

			case DT_MIPS_LOCAL_GOTNO:
				gotsz = dyn[i].d_un.d_val;
				break;

			default:
				break;
		}
	}

	if ((!rel && relent == 0) && (!addr && gotsz == 0))
		return EFI_SUCCESS;

	if ((!rel && relent != 0) || (!addr && gotsz != 0))
		return EFI_LOAD_ERROR;

	while (gotsz > 0) {
		*addr += ldbase;
		addr += 1;
		gotsz --;
	}

	while (relsz > 0) {
		/* apply the relocs */
		switch (ELF64_R_TYPE (swap_uint64 (rel->r_info))) {
			case R_MIPS_NONE:
				break;

			case (R_MIPS_64 << 8) | R_MIPS_REL32:
				addr = (unsigned long *)
					(ldbase + rel->r_offset);
				*addr += ldbase;
				break;

			default:
				break;
		}
		rel = (Elf64_Rel*) ((char *) rel + relent);
		relsz -= relent;
	}
	return EFI_SUCCESS;
}
