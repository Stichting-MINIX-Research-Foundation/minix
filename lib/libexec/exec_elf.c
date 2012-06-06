#define _SYSTEM 1

#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <minix/syslib.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <libexec.h>
#include <string.h>
#include <machine/elf.h>
#include <machine/vmparam.h>
#include <machine/memory.h>

/* For verbose logging */
#define ELF_DEBUG 0

/* Support only 32-bit ELF objects */
#define __ELF_WORD_SIZE 32

#define SECTOR_SIZE 512

static int check_header(Elf_Ehdr *hdr);

static int elf_sane(Elf_Ehdr *hdr)
{
  if (check_header(hdr) != OK) {
     return 0;
  }

  if((hdr->e_type != ET_EXEC) && (hdr->e_type != ET_DYN)) {
     return 0;
  }

  if ((hdr->e_phoff > SECTOR_SIZE) ||
      (hdr->e_phoff + hdr->e_phentsize * hdr->e_phnum) > SECTOR_SIZE) {
#if ELF_DEBUG
	printf("peculiar phoff\n");
#endif
     return 0;
  }

  return 1;
}

static int elf_ph_sane(Elf_Phdr *phdr)
{
  if (rounddown((uintptr_t)phdr, sizeof(Elf_Addr)) != (uintptr_t)phdr) {
     return 0;
  }
  return 1;
}

static int elf_unpack(char *exec_hdr,
	int hdr_len, Elf_Ehdr **hdr, Elf_Phdr **phdr)
{
  *hdr = (Elf_Ehdr *) exec_hdr;
  if(!elf_sane(*hdr)) {
#if ELF_DEBUG
	printf("elf_sane failed\n");
#endif
  	return ENOEXEC;
  }
  *phdr = (Elf_Phdr *)(exec_hdr + (*hdr)->e_phoff);
  if(!elf_ph_sane(*phdr)) {
#if ELF_DEBUG
	printf("elf_ph_sane failed\n");
#endif
  	return ENOEXEC;
  }
#if 0
  if((int)((*phdr) + (*hdr)->e_phnum) >= hdr_len) return ENOEXEC;
#endif
  return OK;
}

int read_header_elf(
  char *exec_hdr,                /* executable header */
  int hdr_len,                 /* significant bytes in exec_hdr */
  vir_bytes *text_vaddr,       /* text virtual address */
  phys_bytes *text_paddr,      /* text physical address */
  vir_bytes *text_filebytes,   /* text segment size (in the file) */
  vir_bytes *text_membytes,    /* text segment size (in memory) */
  vir_bytes *data_vaddr,       /* data virtual address */
  phys_bytes *data_paddr,      /* data physical address */
  vir_bytes *data_filebytes,   /* data segment size (in the file) */
  vir_bytes *data_membytes,    /* data segment size (in memory) */
  vir_bytes *pc,               /* program entry point (initial PC) */
  off_t *text_offset,          /* file offset to text segment */
  off_t *data_offset           /* file offset to data segment */
)
{
  Elf_Ehdr *hdr = NULL;
  Elf_Phdr *phdr = NULL;
  unsigned long seg_filebytes, seg_membytes;
  int e, i = 0;

  *text_vaddr = *text_paddr = 0;
  *text_filebytes = *text_membytes = 0;
  *data_vaddr = *data_paddr = 0;
  *data_filebytes = *data_membytes = 0;
  *pc = *text_offset = *data_offset = 0;

  if((e=elf_unpack(exec_hdr, hdr_len, &hdr, &phdr)) != OK) {
#if ELF_DEBUG
       printf("elf_unpack failed\n");
#endif
       return e;
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

#define IS_ELF(ehdr)	((ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
			 (ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
			 (ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
			 (ehdr).e_ident[EI_MAG3] == ELFMAG3)

static int check_header(Elf_Ehdr *hdr)
{
  if (!IS_ELF(*hdr) ||
      hdr->e_ident[EI_DATA] != ELF_TARG_DATA ||
      hdr->e_ident[EI_VERSION] != EV_CURRENT ||
      hdr->e_phentsize != sizeof(Elf_Phdr) ||
      hdr->e_version != ELF_TARG_VER)
      return ENOEXEC;

  return OK;
}

/* Return >0 if there is an ELF interpreter (i.e. it is a dynamically linked
 * executable) and we could extract it successfully.
 * Return 0 if there isn't one.
 * Return <0 on error.
 */
int elf_has_interpreter(char *exec_hdr,		/* executable header */
		int hdr_len, char *interp, int maxsz)
{
  Elf_Ehdr *hdr = NULL;
  Elf_Phdr *phdr = NULL;
  int e, i;

  if((e=elf_unpack(exec_hdr, hdr_len, &hdr, &phdr)) != OK) return e;

  for (i = 0; i < hdr->e_phnum; i++) {
      switch (phdr[i].p_type) {
      case PT_INTERP:
      	  if(!interp) return 1;
      	  if(phdr[i].p_filesz >= maxsz)
	  	return -1;
	  if(phdr[i].p_offset + phdr[i].p_filesz >= hdr_len)
	  	return -1;
	  memcpy(interp, exec_hdr + phdr[i].p_offset, phdr[i].p_filesz);
	  interp[phdr[i].p_filesz] = '\0';
	  return 1;
      default:
	  continue;
      }
  }
  return 0;
}

int libexec_load_elf(struct exec_info *execi)
{
	Elf_Ehdr *hdr = NULL;
	Elf_Phdr *phdr = NULL;
	int e, i = 0;
	int first = 1;
	vir_bytes startv, stacklow;

	assert(execi != NULL);
	assert(execi->hdr != NULL);

	if((e=elf_unpack(execi->hdr, execi->hdr_len, &hdr, &phdr)) != OK) {
		printf("libexec_load_elf: elf_unpack failed\n");
		return e;
	 }

	/* this function can load the dynamic linker, but that
	 * shouldn't require an interpreter itself.
	 */
	i = elf_has_interpreter(execi->hdr, execi->hdr_len, NULL, 0);
	if(i > 0) {
	      return ENOEXEC;
	}

	execi->stack_size = roundup(execi->stack_size, PAGE_SIZE);
	execi->stack_high = VM_STACKTOP;
	assert(!(VM_STACKTOP % PAGE_SIZE));
	stacklow = execi->stack_high - execi->stack_size;

	assert(execi->copymem);
	assert(execi->clearmem);
	assert(execi->allocmem_prealloc);
	assert(execi->allocmem_ondemand);

	if(execi->clearproc) execi->clearproc(execi);

	for (i = 0; i < hdr->e_phnum; i++) {
		vir_bytes seg_membytes, page_offset, vaddr;
		vir_bytes chunk, vfileend, vmemend;
		Elf_Phdr *ph = &phdr[i];
		if (ph->p_type != PT_LOAD || ph->p_memsz == 0) continue;
		vaddr =  ph->p_vaddr;
		seg_membytes = ph->p_memsz;
		page_offset = vaddr % PAGE_SIZE;
		vaddr -= page_offset;
		seg_membytes += page_offset;
		seg_membytes = roundup(seg_membytes, PAGE_SIZE);
		if(first || startv > vaddr) startv = vaddr;
		first = 0;

		/* make us some memory */
		if(execi->allocmem_prealloc(execi, vaddr, seg_membytes) != OK) {
			if(execi->clearproc) execi->clearproc(execi);
			return ENOMEM;
		}

#if ELF_DEBUG
		printf("mmapped 0x%lx-0x%lx\n", vaddr, vaddr+seg_membytes);
#endif

		/* Copy executable section into it */
		if(execi->copymem(execi, ph->p_offset, ph->p_vaddr, ph->p_filesz) != OK) {
			if(execi->clearproc) execi->clearproc(execi);
			return ENOMEM;
		}

#if ELF_DEBUG
		printf("copied 0x%lx-0x%lx\n", ph->p_vaddr, ph->p_vaddr+ph->p_filesz);
#endif

		/* Clear remaining bits */
		vfileend  = ph->p_vaddr + ph->p_filesz;
		vmemend = vaddr + seg_membytes;
		if((chunk = ph->p_vaddr - vaddr) > 0) {
#if ELF_DEBUG
			printf("start clearing 0x%lx-0x%lx\n", vaddr, vaddr+chunk);
#endif
			execi->clearmem(execi, vaddr, chunk);
		}
		if((chunk = vmemend - vfileend) > 0) {
#if ELF_DEBUG
			printf("end clearing 0x%lx-0x%lx\n", vfileend, vaddr+chunk);
#endif
			execi->clearmem(execi, vfileend, chunk);
		}
	}

	/* Make it a stack */
	if(execi->allocmem_ondemand(execi, stacklow, execi->stack_size) != OK) {
		if(execi->clearproc) execi->clearproc(execi);
		return ENOMEM;
	}

	/* record entry point and lowest load vaddr for caller */
	execi->pc = hdr->e_entry;
	execi->load_base = startv;

	return OK;
}

