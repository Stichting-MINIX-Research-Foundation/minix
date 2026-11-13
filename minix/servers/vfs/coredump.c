#include "fs.h"
#include <fcntl.h>
#include <string.h>
#include <minix/vm.h>
#include <sys/mman.h>
#include <sys/exec_elf.h>
#include <sys/ptrace.h>

/* Include ELF headers */
#include <machine/elf.h>
#include <machine/reg.h>
#include <machine/stackframe.h>

/*
 * We write NetBSD-style core file notes: a "NetBSD-CORE" procinfo note,
 * plus a per-LWP "NetBSD-CORE@<lwpid>" register note whose contents are
 * in PT_GETREGS (struct reg) format, so that the native NetBSD toolchain
 * (gdb, objdump) can interpret the core file.  MINIX processes have a
 * single thread of control, presented as LWP 1.
 */
#define CORE_LWPID	1

static char core_name[] = ELF_NOTE_NETBSD_CORE_NAME;
static char core_lwp_name[] = ELF_NOTE_NETBSD_CORE_NAME "@1";

static void fill_elf_header(Elf_Ehdr *elf_header, int phnum);
static void fill_prog_header(Elf_Phdr *prog_header, Elf_Word
	p_type, Elf_Off p_offset, Elf_Addr p_vaddr, Elf_Word p_flags,
	Elf_Word p_filesz, Elf_Word p_memsz);
static int get_memory_regions(Elf_Phdr phdrs[]);
static void fill_note_segment_and_entries_hdrs(Elf_Phdr phdrs[],
	Elf_Nhdr nhdrs[]);
static void adjust_offsets(Elf_Phdr phdrs[], int phnum);
static void dump_elf_header(struct filp *f, Elf_Ehdr elf_header);
static void dump_notes(struct filp *f, Elf_Nhdr nhdrs[], int csig,
	char *proc_name);
static void dump_program_headers(struct filp *f, Elf_Phdr phdrs[], int
	phnum);
static void dump_segments(struct filp *f, Elf_Phdr phdrs[], int
	phnum);
static void write_buf(struct filp *f, char *buf, size_t size);

/*===========================================================================*
 *				write_elf_core_file			     *
 *===========================================================================*/
void write_elf_core_file(struct filp *f, int csig, char *proc_name)
{
/* First, fill in all the required headers, second, adjust the offsets,
 * third, dump everything into the core file
 */
#define MAX_REGIONS 100
#define NR_NOTE_ENTRIES 2
  Elf_Ehdr elf_header;
  Elf_Phdr phdrs[MAX_REGIONS + 1];
  Elf_Nhdr nhdrs[NR_NOTE_ENTRIES];
  int phnum;

  memset(phdrs, 0, sizeof(phdrs));

  /* Fill in the NOTE Program Header - at phdrs[0] - and
   * note entries' headers
   */
  fill_note_segment_and_entries_hdrs(phdrs, nhdrs);

  /* Get the memory segments and fill in the Program headers */
  phnum = get_memory_regions(phdrs) + 1;

  /* Fill in the ELF header */
  fill_elf_header(&elf_header, phnum);

  /* Adjust offsets in program headers - The layout in the ELF core file
   * is the following: the ELF Header, the Note Program Header,
   * the rest of Program Headers (memory segments), Note contents,
   * the program segments' contents
   */
  adjust_offsets(phdrs, phnum);

  /* Write ELF header */
  dump_elf_header(f, elf_header);

  /* Write Program headers (Including the NOTE) */
  dump_program_headers(f, phdrs, phnum);

  /* Write NOTE contents */
  dump_notes(f, nhdrs, csig, proc_name);

  /* Write segments' contents */
  dump_segments(f, phdrs, phnum);
}

/*===========================================================================*
 *				fill_elf_header        			     *
 *===========================================================================*/
static void fill_elf_header (Elf_Ehdr *elf_header, int phnum)
{
  memset((void *) elf_header, 0, sizeof(Elf_Ehdr));

  elf_header->e_ident[EI_MAG0] = ELFMAG0;
  elf_header->e_ident[EI_MAG1] = ELFMAG1;
  elf_header->e_ident[EI_MAG2] = ELFMAG2;
  elf_header->e_ident[EI_MAG3] = ELFMAG3;
  elf_header->e_ident[EI_CLASS] = ELF_TARG_CLASS;
  elf_header->e_ident[EI_DATA] = ELF_TARG_DATA;
  elf_header->e_ident[EI_VERSION] = EV_CURRENT;
  elf_header->e_ident[EI_OSABI] = ELFOSABI_SYSV;
  elf_header->e_type = ET_CORE;
  elf_header->e_machine = ELF_TARG_MACH;
  elf_header->e_version = EV_CURRENT;
  elf_header->e_ehsize = sizeof(Elf_Ehdr);
  elf_header->e_phoff = sizeof(Elf_Ehdr);
  elf_header->e_phentsize = sizeof(Elf_Phdr);
  elf_header->e_phnum = phnum;
}

/*===========================================================================*
 *				fill_prog_header        		     *
 *===========================================================================*/
static void fill_prog_header (Elf_Phdr *prog_header, Elf_Word p_type,
	Elf_Off p_offset, Elf_Addr p_vaddr, Elf_Word p_flags,
	Elf_Word p_filesz, Elf_Word p_memsz)
{

  memset((void *) prog_header, 0, sizeof(Elf_Phdr));

  prog_header->p_type = p_type;
  prog_header->p_offset = p_offset;
  prog_header->p_vaddr = p_vaddr;
  prog_header->p_flags = p_flags;
  prog_header->p_filesz = p_filesz;
  prog_header->p_memsz = p_memsz;

}

#define PADBYTES    4
#define PAD_LEN(x)  ((x + (PADBYTES - 1)) & ~(PADBYTES - 1))

/*===========================================================================*
 *			fill_note_segment_and_entries_hdrs     	     	     *
 *===========================================================================*/
static void fill_note_segment_and_entries_hdrs(Elf_Phdr phdrs[],
				Elf_Nhdr nhdrs[])
{
  int filesize;

  /* First note entry header: process info, "NetBSD-CORE" */
  nhdrs[0].n_namesz = sizeof(core_name);
  nhdrs[0].n_descsz = sizeof(struct netbsd_elfcore_procinfo);
  nhdrs[0].n_type = ELF_NOTE_NETBSD_CORE_PROCINFO;

  /* Second note entry header: registers of the (only) LWP, in ptrace(2)
   * PT_GETREGS format, "NetBSD-CORE@<lwpid>"
   */
  nhdrs[1].n_namesz = sizeof(core_lwp_name);
  nhdrs[1].n_descsz = sizeof(struct reg);
  nhdrs[1].n_type = PT_GETREGS;

  /* Note names and descriptors are written 4-byte aligned */
  filesize = 2 * sizeof(Elf_Nhdr) +
	PAD_LEN(nhdrs[0].n_namesz) + PAD_LEN(nhdrs[0].n_descsz) +
	PAD_LEN(nhdrs[1].n_namesz) + PAD_LEN(nhdrs[1].n_descsz);
  fill_prog_header(&phdrs[0], PT_NOTE, 0, 0, PF_R, filesize, 0);
}

/*===========================================================================*
 *				adjust_offset   			     *
 *===========================================================================*/
static void adjust_offsets(Elf_Phdr phdrs[], int phnum)
{
  int i;
  long offset = sizeof(Elf_Ehdr) + phnum * sizeof(Elf_Phdr);

  for (i = 0; i < phnum; i++) {
	phdrs[i].p_offset = offset;
	offset += phdrs[i].p_filesz;
  }
}

/*===========================================================================*
 *				write_buf       			     *
 *===========================================================================*/
static void write_buf(struct filp *f, char *buf, size_t size)
{
  /*
   * TODO: pass in the proper file descriptor number.  It really doesn't matter
   * what we pass in, because the write target is a regular file.  As such, the
   * write call will never be suspended, and suspension is the only case that
   * read_write() could use the file descriptor.  Still, passing in an invalid
   * value isn't exactly nice.
   */
  read_write(fp, WRITING, -1 /*fd*/, f, (vir_bytes)buf, size, VFS_PROC_NR);
}

/*===========================================================================*
 *				get_memory_regions			     *
 *===========================================================================*/
static int get_memory_regions(Elf_Phdr phdrs[])
{
  /* Print the virtual memory regions of a process. */

  /* The same as dump_regions from procfs/pid.c */
  struct vm_region_info vri[MAX_VRI_COUNT];
  vir_bytes next;
  int i, r, count;
  Elf_Word pflags;

  count = 0;
  next = 0;

  do {
	r = vm_info_region(fp->fp_endpoint, vri, MAX_VRI_COUNT, &next);
	if (r < 0) return r;
	if (r == 0) break;

	for (i = 0; i < r; i++) {
		pflags =  (vri[i].vri_prot & PROT_READ ? PF_R : 0)
			| (vri[i].vri_prot & PROT_WRITE ? PF_W : 0)
			| (vri[i].vri_prot & PROT_EXEC ? PF_X : 0);

		fill_prog_header (&phdrs[count + 1], PT_LOAD,
				0, vri[i].vri_addr, pflags,
				vri[i].vri_length, vri[i].vri_length);
		count++;

		if (count >= MAX_REGIONS) {
			printf("VFS: get_memory_regions Warning: "
				"Program has too many regions\n");
			return(count);
		}
	}
  } while (r == MAX_VRI_COUNT);

  return(count);
}

/*===========================================================================*
 *				dump_notes			             *
 *===========================================================================*/
static void dump_notes(struct filp *f, Elf_Nhdr nhdrs[], int csig,
			 char *proc_name)
{
  char pad[4];
  struct netbsd_elfcore_procinfo cpi;
  struct stackframe_s frame;
  struct reg regs;

  memset(pad, 0, sizeof(pad));

  /* Dump first note entry - process information */
  memset(&cpi, 0, sizeof(cpi));
  cpi.cpi_version = NETBSD_ELFCORE_PROCINFO_VERSION;
  cpi.cpi_cpisize = sizeof(cpi);
  cpi.cpi_signo = csig;
  cpi.cpi_pid = fp->fp_pid;
  /* Parent/group/session IDs are kept by PM, not VFS; leave them zero */
  cpi.cpi_ruid = fp->fp_realuid;
  cpi.cpi_euid = fp->fp_effuid;
  cpi.cpi_svuid = fp->fp_effuid;
  cpi.cpi_rgid = fp->fp_realgid;
  cpi.cpi_egid = fp->fp_effgid;
  cpi.cpi_svgid = fp->fp_effgid;
  cpi.cpi_nlwps = 1;
  cpi.cpi_siglwp = CORE_LWPID;
  strncpy((char *) cpi.cpi_name, proc_name, sizeof(cpi.cpi_name) - 1);

  write_buf(f, (char *) &nhdrs[0], sizeof(Elf_Nhdr));
  write_buf(f, core_name, nhdrs[0].n_namesz);
  write_buf(f, pad, PAD_LEN(nhdrs[0].n_namesz) - nhdrs[0].n_namesz);
  write_buf(f, (char *) &cpi, sizeof(cpi));
  write_buf(f, pad, PAD_LEN(sizeof(cpi)) - sizeof(cpi));

  /* Get registers and convert to ptrace(2) PT_GETREGS layout */
  memset(&frame, 0, sizeof(frame));
  if (sys_getregs(&frame, fp->fp_endpoint) != OK)
	printf("VFS: Could not read registers\n");

  memset(&regs, 0, sizeof(regs));
#if defined(__x86_64__)
  regs.regs[_REG_RDI] = frame.di;
  regs.regs[_REG_RSI] = frame.si;
  regs.regs[_REG_RDX] = frame.dx;
  regs.regs[_REG_RCX] = frame.cx;
  regs.regs[_REG_R8] = frame.r8;
  regs.regs[_REG_R9] = frame.r9;
  regs.regs[_REG_R10] = frame.r10;
  regs.regs[_REG_R11] = frame.r11;
  regs.regs[_REG_R12] = frame.r12;
  regs.regs[_REG_R13] = frame.r13;
  regs.regs[_REG_R14] = frame.r14;
  regs.regs[_REG_R15] = frame.r15;
  regs.regs[_REG_RBP] = frame.fp;
  regs.regs[_REG_RBX] = frame.bx;
  regs.regs[_REG_RAX] = frame.retreg;
  regs.regs[_REG_GS] = frame.gs;
  regs.regs[_REG_FS] = frame.fs;
  regs.regs[_REG_ES] = frame.es;
  regs.regs[_REG_DS] = frame.ds;
  regs.regs[_REG_RIP] = frame.pc;
  regs.regs[_REG_CS] = frame.cs;
  regs.regs[_REG_RFLAGS] = frame.psw;
  regs.regs[_REG_RSP] = frame.sp;
  regs.regs[_REG_SS] = frame.ss;
#elif defined(__i386__)
  /*
   * i386's struct reg has named members rather than a regs[] array
   * indexed by _REG_*, and its stackframe_s is a different shape: the
   * segment registers are 16-bit and there is no r8..r15.  The two
   * commented-out holes in the frame (st, retadr) have no counterpart
   * here either.
   */
  regs.r_edi = frame.di;
  regs.r_esi = frame.si;
  regs.r_ebp = frame.fp;
  regs.r_ebx = frame.bx;
  regs.r_edx = frame.dx;
  regs.r_ecx = frame.cx;
  regs.r_eax = frame.retreg;
  regs.r_eip = frame.pc;
  regs.r_cs = frame.cs;
  regs.r_eflags = frame.psw;
  regs.r_esp = frame.sp;
  regs.r_ss = frame.ss;
  regs.r_ds = frame.ds;
  regs.r_es = frame.es;
  regs.r_fs = frame.fs;
  regs.r_gs = frame.gs;
#else
#error "no register mapping for this architecture"
#endif

  /* Dump second note entry - the general registers */
  write_buf(f, (char *) &nhdrs[1], sizeof(Elf_Nhdr));
  write_buf(f, core_lwp_name, nhdrs[1].n_namesz);
  write_buf(f, pad, PAD_LEN(nhdrs[1].n_namesz) - nhdrs[1].n_namesz);
  write_buf(f, (char *) &regs, sizeof(regs));
  write_buf(f, pad, PAD_LEN(sizeof(regs)) - sizeof(regs));
}

/*===========================================================================*
 *				dump_elf_header			             *
 *===========================================================================*/
static void dump_elf_header(struct filp *f, Elf_Ehdr elf_header)
{
  write_buf(f, (char *) &elf_header, sizeof(Elf_Ehdr));
}

/*===========================================================================*
 *			  dump_program_headers			             *
 *===========================================================================*/
static void dump_program_headers(struct filp *f, Elf_Phdr phdrs[], int phnum)
{
  int i;

  for (i = 0; i < phnum; i++)
	write_buf(f, (char *) &phdrs[i], sizeof(Elf_Phdr));
}

/*===========================================================================*
 *			      dump_segments 			             *
 *===========================================================================*/
static void dump_segments(struct filp *f, Elf_Phdr phdrs[], int phnum)
{
  int i;
  vir_bytes len;
  off_t off, seg_off;
  int r;
  static u8_t buf[CLICK_SIZE];

  for (i = 1; i < phnum; i++) {
	len = phdrs[i].p_memsz;
	seg_off = phdrs[i].p_vaddr;

	if (len > LONG_MAX) {
		printf("VFS: segment too large to dump, truncating\n");
		len = LONG_MAX;
	}

	for (off = 0; off < (off_t) len; off += CLICK_SIZE) {
		vir_bytes p = (vir_bytes) (seg_off + off);
		r = sys_datacopy_try(fp->fp_endpoint, p,
			SELF, (vir_bytes) buf,
			(phys_bytes) CLICK_SIZE);

		if(r != OK) {
			/* memory didn't exist; write as zeroes */
			memset(buf, 0, sizeof(buf));
		}

		write_buf(f, (char *) buf, (off + CLICK_SIZE <= (off_t) len) ?
					CLICK_SIZE : (len - off));
	}
  }
}
