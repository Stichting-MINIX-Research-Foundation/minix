#ifndef _LIBEXEC_H_
#define _LIBEXEC_H_ 1

#include <sys/exec_elf.h>

/* ELF routines */
int read_header_elf(const char *exec_hdr, int hdr_len,
   vir_bytes *text_vaddr, phys_bytes *text_paddr,
   vir_bytes *text_filebytes, vir_bytes *text_membytes,
   vir_bytes *data_vaddr, phys_bytes *data_paddr,
   vir_bytes *data_filebytes, vir_bytes *data_membytes,
   vir_bytes *pc, off_t *text_offset, off_t *data_offset);

int elf_has_interpreter(const char *exec_hdr, int hdr_len, char *interp, int maxsz);
int elf_phdr(const char *exec_hdr, int hdr_len, vir_bytes *phdr);

#endif /* !_LIBEXEC_H_ */
