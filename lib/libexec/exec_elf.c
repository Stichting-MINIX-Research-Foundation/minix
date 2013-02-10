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
#include <minix/syslib.h>

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
  	return ENOEXEC;
  }
  *phdr = (Elf_Phdr *)(exec_hdr + (*hdr)->e_phoff);
  if(!elf_ph_sane(*phdr)) {
  	return ENOEXEC;
  }
#if 0
  if((int)((*phdr) + (*hdr)->e_phnum) >= hdr_len) return ENOEXEC;
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

  if((e=elf_unpack(exec_hdr, hdr_len, &hdr, &phdr)) != OK) return 0;

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
	vir_bytes startv = 0, stacklow;

	assert(execi != NULL);
	assert(execi->hdr != NULL);

	if((e=elf_unpack(execi->hdr, execi->hdr_len, &hdr, &phdr)) != OK) {
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
	execi->stack_high = rounddown(execi->stack_high, PAGE_SIZE);
	stacklow = execi->stack_high - execi->stack_size;

	assert(execi->copymem);
	assert(execi->clearmem);
	assert(execi->allocmem_prealloc);
	assert(execi->allocmem_ondemand);

	for (i = 0; i < hdr->e_phnum; i++) {
		Elf_Phdr *ph = &phdr[i];
		off_t file_limit = ph->p_offset + ph->p_filesz;
		/* sanity check binary before wiping out the target process */
		if(execi->filesize < file_limit) {
			return ENOEXEC;
		}
	}

	if(execi->clearproc) execi->clearproc(execi);

	for (i = 0; i < hdr->e_phnum; i++) {
		vir_bytes seg_membytes, page_offset, p_vaddr, vaddr;
		vir_bytes chunk, vfileend, vmemend;
		Elf_Phdr *ph = &phdr[i];
		if (ph->p_type != PT_LOAD || ph->p_memsz == 0) continue;
		vaddr = p_vaddr = ph->p_vaddr + execi->load_offset;
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
		if(execi->copymem(execi, ph->p_offset, p_vaddr, ph->p_filesz) != OK) {
			if(execi->clearproc) execi->clearproc(execi);
			return ENOMEM;
		}

#if ELF_DEBUG
		printf("copied 0x%lx-0x%lx\n", p_vaddr, p_vaddr+ph->p_filesz);
#endif

		/* Clear remaining bits */
		vfileend  = p_vaddr + ph->p_filesz;
		vmemend = vaddr + seg_membytes;
		if((chunk = p_vaddr - vaddr) > 0) {
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

#if ELF_DEBUG
	printf("stack mmapped 0x%lx-0x%lx\n", stacklow, stacklow+execi->stack_size);
#endif

	/* record entry point and lowest load vaddr for caller */
	execi->pc = hdr->e_entry + execi->load_offset;
	execi->load_base = startv;

	return OK;
}

