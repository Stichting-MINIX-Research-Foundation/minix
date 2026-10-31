/*	$NetBSD: bin_bfd.c,v 1.2 2017/07/25 06:43:03 mrg Exp $	*/

/*
 * Copyright (c) 1996, 2002 Christopher G. Demetriou
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * <<Id: LICENSE_GC,v 1.1 2001/10/01 23:24:05 cgd Exp>>
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: bin_bfd.c,v 1.2 2017/07/25 06:43:03 mrg Exp $");

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <bfd.h>
#include "bin.h"

void *
bin_open(int kfd, const char *kfile, const char *bfdname)
{
	bfd *abfd;
	bfd_init();
	if ((abfd = bfd_fdopenr(kfile, bfdname, kfd)) == NULL) {
		bfd_perror("open");
		exit(1);
	}
	if (!bfd_check_format(abfd, bfd_object)) {
		bfd_perror("check format");
		exit(1);
	}
	return abfd;
}

int
bin_find_md_root(void *bin, const char *mappedkfile, off_t size,
    unsigned long text_start,
    const char *root_name, const char *size_name, size_t *md_root_offset,
    size_t *md_root_size_offset, uint32_t *md_root_size, int verbose)
{
	bfd *abfd = bin;
	long i;
	long storage_needed;
	long number_of_symbols;
	asymbol **symbol_table = NULL;
	struct symbols {
		const char *name;
		size_t offset;
	} *s, symbols[3];

	symbols[0].offset = 0;
	symbols[1].offset = 0;
	symbols[0].name = root_name;
	symbols[1].name = size_name;
	symbols[2].name = NULL;

	storage_needed = bfd_get_symtab_upper_bound(abfd);
	if (storage_needed <= 0) {
		warnx("bfd storage needed error");
		return 1;
	}

	symbol_table = malloc(storage_needed);
	if (symbol_table == NULL) {
		warn("symbol table");
		return 1;
	}

	number_of_symbols = bfd_canonicalize_symtab(abfd, symbol_table);
	if (number_of_symbols <= 0) {
		warnx("can't canonicalize symbol table");
		free(symbol_table);
		return 1;
	}

	for (i = 0; i < number_of_symbols; i++) {
		for (s = symbols; s->name != NULL; s++) {
			const char *sym = symbol_table[i]->name;

			/*
			 * match symbol prefix '_' or ''.
			 */
			if (!strcmp(s->name, sym) ||
			    !strcmp(s->name + 1, sym)) {
				s->offset =
				    (size_t)(symbol_table[i]->section->filepos
				    + symbol_table[i]->value);
			}
		}
	}

	free(symbol_table);

	for (s = symbols; s->name != NULL; s++) {
		if (s->offset == 0) {
			warnx("missing offset for `%s'", s->name);
			return 1;
		}
	}

	*md_root_offset = symbols[0].offset;
	*md_root_size_offset = symbols[1].offset;
	*md_root_size = bfd_get_32(abfd, &mappedkfile[*md_root_size_offset]);

	return 0;
}

void
bin_put_32(void *bin, off_t size, char *buf)
{
	bfd_put_32((struct bfd *)bin, size, buf);
}

void
bin_close(void *bin)
{
	bfd_close_all_done((struct bfd *)bin);
}

const char **
bin_supported_targets(void)
{
	return bfd_target_list();
}
