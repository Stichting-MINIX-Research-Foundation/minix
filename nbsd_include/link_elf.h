/*	$NetBSD: link_elf.h,v 1.10 2010/10/16 10:27:06 skrll Exp $	*/

#ifndef _LINK_ELF_H_
#define	_LINK_ELF_H_

#include <sys/types.h>
#include <sys/exec_elf.h>

typedef struct link_map {
	caddr_t		 l_addr;	/* Base Address of library */
#ifdef __mips__
	caddr_t		 l_offs;	/* Load Offset of library */
#endif
	const char	*l_name;	/* Absolute Path to Library */
	void		*l_ld;		/* Pointer to .dynamic in memory */
	struct link_map	*l_next;	/* linked list of of mapped libs */
	struct link_map *l_prev;
} Link_map;

/*
 * This only exists for GDB.
 */
struct r_debug {
	int r_version;			/* not used */
	struct link_map *r_map;		/* list of loaded images */
	void (*r_brk)(void);		/* pointer to break point */
	enum {
		RT_CONSISTENT,		/* things are stable */
		RT_ADD,			/* adding a shared library */
		RT_DELETE		/* removing a shared library */
	} r_state;
};

struct dl_phdr_info
{
	Elf_Addr dlpi_addr;			/* module relocation base */
	const char *dlpi_name;			/* module name */
	const Elf_Phdr *dlpi_phdr;		/* pointer to module's phdr */
	Elf_Half dlpi_phnum;			/* number of entries in phdr */
	unsigned long long int dlpi_adds;	/* total # of loads */
	unsigned long long int dlpi_subs;	/* total # of unloads */
	size_t dlpi_tls_modid;
	void *dlpi_tls_data;
};

__BEGIN_DECLS

int dl_iterate_phdr(int (*)(struct dl_phdr_info *, size_t, void *),
    void *);

__END_DECLS

#endif	/* _LINK_ELF_H_ */
