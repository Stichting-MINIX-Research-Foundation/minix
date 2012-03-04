/* ELF functions-only symbol table extractor by David van Moolenbroek.
 * Replaces generic a.out version by Philip Homburg.
 * Temporary solution until libc's nlist(3) becomes usable.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libelf.h>
#include <nlist.h>

static int get_syms(Elf *elf, Elf_Scn *scn, Elf32_Shdr *shdr,
	struct nlist **nlistp)
{
	Elf_Data *data;
	Elf32_Sym *sym;
	unsigned long nsyms;
	struct nlist *nlp;
	char *name;
	size_t size;
	int i;

	if ((data = elf_getdata(scn, NULL)) == NULL) {
		errno = ENOEXEC;
		return -1;
	}

	nsyms = data->d_size / sizeof(Elf32_Sym);

	size = sizeof(struct nlist) * nsyms;
	if ((*nlistp = nlp = malloc(size)) == NULL)
		return -1;
	memset(nlp, 0, size);

	sym = (Elf32_Sym *) data->d_buf;
	for (i = 0; i < nsyms; i++, sym++, nlp++) {
		nlp->n_value = sym->st_value;

		/* The caller only cares about N_TEXT. Properly setting other
		 * types would involve crosschecking with section types.
		 */
		switch (ELF32_ST_TYPE(sym->st_info)) {
		case STT_FUNC:   nlp->n_type = N_TEXT; break;
		default:         nlp->n_type = N_UNDF; break;
		}

		name = elf_strptr(elf, shdr->sh_link, (size_t) sym->st_name);
		if ((nlp->n_name = strdup(name ? name : "")) == NULL) {
			free(*nlistp);
			return -1;
		}
	}

	return nsyms;
}

int read_nlist(const char *filename, struct nlist **nlist_table)
{
	/* Read in the symbol table of 'filename'. Return the resulting symbols
	 * as an array, with 'nlist_table' set to point to it, and the number
	 * of elements as the return value. The caller has no way to free the
	 * results. Return 0 if the executable contains no symbols. Upon error,
	 * return -1 and set errno to an appropriate value.
	 */
	Elf *elf;
	Elf_Scn *scn;
	Elf32_Shdr *shdr;
	int res, fd;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		errno = EINVAL;
		return -1;
	}

	if ((fd = open(filename, O_RDONLY)) < 0)
		return -1;

	elf = elf_begin(fd, ELF_C_READ, NULL);
	if (elf == NULL) {
		errno = ENOEXEC;
		close(fd);
		return -1;
	}

	scn = NULL;
	res = 0;
	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		if ((shdr = elf32_getshdr(scn)) == NULL)
			continue;

		if (shdr->sh_type != SHT_SYMTAB)
			continue;

		res = get_syms(elf, scn, shdr, nlist_table);
		break;
	}

	elf_end(elf);
	close(fd);

	return res;
}
