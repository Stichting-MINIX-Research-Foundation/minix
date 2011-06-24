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

#define SECTOR_SIZE 512

static int __elfN(check_header)(const Elf_Ehdr *hdr);

int read_header_elf(
  const char *exec_hdr,		/* executable header */
  vir_bytes *text_vaddr,	/* text virtual address */
  phys_bytes *text_paddr,	/* text physical address */
  vir_bytes *text_filebytes,	/* text segment size (in the file) */
  vir_bytes *text_membytes,	/* text segment size (in memory) */
  vir_bytes *data_vaddr,	/* data virtual address */
  phys_bytes *data_paddr,	/* data physical address */
  vir_bytes *data_filebytes,	/* data segment size (in the file) */
  vir_bytes *data_membytes,	/* data segment size (in memory) */
  vir_bytes *pc,		/* program entry point (initial PC) */
  off_t *text_offset,		/* file offset to text segment */
  off_t *data_offset		/* file offset to data segment */
)
{
  const Elf_Ehdr *hdr = NULL;
  const Elf_Phdr *phdr = NULL;
  unsigned long seg_filebytes, seg_membytes;
  int i = 0;

  *text_vaddr = *text_paddr = 0;
  *text_filebytes = *text_membytes = 0;
  *data_vaddr = *data_paddr = 0;
  *data_filebytes = *data_membytes = 0;
  *pc = *text_offset = *data_offset = 0;

  hdr = (const Elf_Ehdr *)exec_hdr;
  if (__elfN(check_header)(hdr) != OK || (hdr->e_type != ET_EXEC))
  {
     return ENOEXEC;
  }

  if ((hdr->e_phoff > SECTOR_SIZE) ||
      (hdr->e_phoff + hdr->e_phentsize * hdr->e_phnum) > SECTOR_SIZE) {
     return ENOEXEC;
  }

  phdr = (const Elf_Phdr *)(exec_hdr + hdr->e_phoff);
  if (
#ifdef __NBSD_LIBC
      rounddown((uintptr_t)phdr, sizeof(Elf_Addr)) != (uintptr_t)phdr
#else
      !_minix_aligned(hdr->e_phoff, Elf_Addr)
#endif
     ) {
     return ENOEXEC;
  }

#if ELF_DEBUG
  printf("Program header file offset (phoff): %ld\n", hdr->e_phoff);
  printf("Section header file offset (shoff): %ld\n", hdr->e_shoff);
  printf("Program header entry size (phentsize): %d\n", hdr->e_phentsize);
  printf("Program header entry num (phnum): %d\n", hdr->e_phnum);
  printf("Section header entry size (shentsize): %d\n", hdr->e_shentsize);
  printf("Section header entry num (shnum): %d\n", hdr->e_shnum);
  printf("Section name strings index (shstrndx): %d\n", hdr->e_shstrndx);
  printf("Entry Point: 0x%lx\n", hdr->e_entry);
#endif

  for (i = 0; i < hdr->e_phnum; i++) {
      switch (phdr[i].p_type) {
      case PT_LOAD:
	  if (phdr[i].p_memsz == 0)
	      break;
	  seg_filebytes = phdr[i].p_filesz;
	  seg_membytes = round_page(phdr[i].p_memsz + phdr[i].p_vaddr -
				    trunc_page(phdr[i].p_vaddr));

	  if (hdr->e_entry >= phdr[i].p_vaddr &&
	      hdr->e_entry < (phdr[i].p_vaddr + phdr[i].p_memsz)) {
	      *text_vaddr = phdr[i].p_vaddr;
	      *text_paddr = phdr[i].p_paddr;
	      *text_filebytes = seg_filebytes;
	      *text_membytes = seg_membytes;
	      *pc = (vir_bytes)hdr->e_entry;
	      *text_offset = phdr[i].p_offset;
	  } else {
	      *data_vaddr = phdr[i].p_vaddr;
	      *data_paddr = phdr[i].p_paddr;
	      *data_filebytes = seg_filebytes;
	      *data_membytes = seg_membytes;
	      *data_offset = phdr[i].p_offset;
	  }
	  break;
      default:
	  break;
      }
  }

#if ELF_DEBUG
  printf("Text vaddr:     0x%lx\n", *text_vaddr);
  printf("Text paddr:     0x%lx\n", *text_paddr);
  printf("Text filebytes: 0x%lx\n", *text_filebytes);
  printf("Text membytes:  0x%lx\n", *text_membytes);
  printf("Data vaddr:     0x%lx\n", *data_vaddr);
  printf("Data paddr:     0x%lx\n", *data_paddr);
  printf("Data filebyte:  0x%lx\n", *data_filebytes);
  printf("Data membytes:  0x%lx\n", *data_membytes);
  printf("PC:             0x%lx\n", *pc);
  printf("Text offset:    0x%lx\n", *text_offset);
  printf("Data offset:    0x%lx\n", *data_offset);
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
