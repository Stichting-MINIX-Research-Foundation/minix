#ifndef _LIBEXEC_H_
#define _LIBEXEC_H_ 1

#include <machine/elf.h>

/* a.out routines */
int read_header_aout(const char *exec_hdr, size_t exec_len, int *sep_id,
   vir_bytes *text_bytes, vir_bytes *data_bytes,
   vir_bytes *bss_bytes, phys_bytes *tot_bytes, vir_bytes *pc,
   int *hdrlenp);

/* ELF routines */
int read_header_elf(const char *exec_hdr,
   vir_bytes *text_vaddr, phys_bytes *text_paddr,
   vir_bytes *text_filebytes, vir_bytes *text_membytes,
   vir_bytes *data_vaddr, phys_bytes *data_paddr,
   vir_bytes *data_filebytes, vir_bytes *data_membytes,
   vir_bytes *pc, off_t *text_offset, off_t *data_offset);

#endif /* !_LIBEXEC_H_ */
