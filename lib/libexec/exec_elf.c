#define _SYSTEM 1

#include <minix/type.h>
#include <minix/const.h>
#include <sys/param.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <libexec.h>

/* For verbose logging */
#define ELF_DEBUG 0

/* Support only 32-bit ELF objects */
#define __ELF_WORD_SIZE 32

static int __elfN(check_header)(const Elf_Ehdr *hdr);

int read_header_elf(
  const char *exec_hdr,		/* executable header */
  vir_bytes *text_addr,		/* text virtual address */
  vir_bytes *text_filebytes,	/* text segment size (in the file) */
  vir_bytes *text_membytes,	/* text segment size (in memory) */
  vir_bytes *data_addr,		/* data virtual address */
  vir_bytes *data_filebytes,	/* data segment size (in the file) */
  vir_bytes *data_membytes,	/* data segment size (in memory) */
  phys_bytes *tot_bytes,	/* total size */
  vir_bytes *pc,		/* program entry point (initial PC) */
  off_t *text_offset,		/* file offset to text segment */
  off_t *data_offset		/* file offset to data segment */
)
{
  const Elf_Ehdr *hdr = NULL;
  const Elf_Phdr *phdr = NULL;
  unsigned long seg_filebytes, seg_membytes, seg_addr;
  int i = 0;

  assert(exec_hdr != NULL);

  *text_addr = *text_filebytes = *text_membytes = 0;
  *data_addr = *data_filebytes = *data_membytes = 0;
  *tot_bytes = *pc = *text_offset = *data_offset = 0;

  hdr = (const Elf_Ehdr *)exec_hdr;
  if (__elfN(check_header)(hdr) != OK || (hdr->e_type != ET_EXEC))
  {
     return ENOEXEC;
  }

  if ((hdr->e_phoff > PAGE_SIZE) ||
      (hdr->e_phoff + hdr->e_phentsize * hdr->e_phnum) > PAGE_SIZE) {
     return ENOEXEC;
  }

  phdr = (const Elf_Phdr *)(exec_hdr + hdr->e_phoff);
  if (!aligned(phdr, Elf_Addr)) {
     return ENOEXEC;
  }

#if ELF_DEBUG
  printf("Program header file offset (phoff): %d\n", hdr->e_phoff);
  printf("Section header file offset (shoff): %d\n", hdr->e_shoff);
  printf("Program header entry size (phentsize): %d\n", hdr->e_phentsize);
  printf("Program header entry num (phnum): %d\n", hdr->e_phnum);
  printf("Section header entry size (shentsize): %d\n", hdr->e_shentsize);
  printf("Section header entry num (shnum): %d\n", hdr->e_shnum);
  printf("Section name strings index (shstrndx): %d\n", hdr->e_shstrndx);
  printf("Entry Point: 0x%x\n", hdr->e_entry);
#endif

  for (i = 0; i < hdr->e_phnum; i++) {
      switch (phdr[i].p_type) {
      case PT_LOAD:
	  if (phdr[i].p_memsz == 0)
	      break;
	  seg_addr = phdr[i].p_vaddr;
	  seg_filebytes = phdr[i].p_filesz;
	  seg_membytes = round_page(phdr[i].p_memsz + phdr[i].p_vaddr -
				    trunc_page(phdr[i].p_vaddr));

	  if (hdr->e_entry >= phdr[i].p_vaddr &&
	      hdr->e_entry < (phdr[i].p_vaddr + phdr[i].p_memsz)) {
	      *text_addr = seg_addr;
	      *text_filebytes = seg_filebytes;
	      *text_membytes = seg_membytes;
	      *pc = (vir_bytes)hdr->e_entry;
	      *text_offset = phdr[i].p_offset;
	  } else {
	      *data_addr = seg_addr;
	      *data_filebytes = seg_filebytes;
	      *data_membytes = seg_membytes;
	      *data_offset = phdr[i].p_offset;
	  }
	  break;
      default:
	  break;
      }
  }

  *tot_bytes = 0; /* Use default stack size */

#if ELF_DEBUG
  printf("Text addr:      0x%x\n", *text_addr);
  printf("Text filebytes: 0x%x\n", *text_filebytes);
  printf("Text membytes:  0x%x\n", *text_membytes);
  printf("Data addr:      0x%x\n", *data_addr);
  printf("Data filebyte:  0x%x\n", *data_filebytes);
  printf("Data membytes:  0x%x\n", *data_membytes);
  printf("Tot bytes:      0x%x\n", *tot_bytes);
  printf("PC:             0x%x\n", *pc);
  printf("Text offset:    0x%x\n", *text_offset);
  printf("Data offset:    0x%x\n", *data_offset);
#endif

  return OK;
}

static int __elfN(check_header)(const Elf_Ehdr *hdr)
{
  if (!IS_ELF(*hdr) ||
      hdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
      hdr->e_ident[EI_VERSION] != EV_CURRENT ||
      hdr->e_phentsize != sizeof(Elf_Phdr) ||
      hdr->e_version != ELF_TARG_VER)
      return ENOEXEC;

  return OK;
}
