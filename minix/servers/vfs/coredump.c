#include "fs.h"
#include <fcntl.h>
#include <string.h>
#include <minix/vm.h>
#include <sys/mman.h>
#include <sys/exec_elf.h>

/* Include ELF headers */
#include <sys/elf_core.h>
#include <machine/elf.h>

static void fill_elf_header(Elf32_Ehdr *elf_header, int phnum);
static void fill_prog_header(Elf32_Phdr *prog_header, Elf32_Word
	p_type, Elf32_Off p_offset, Elf32_Addr p_vaddr, Elf32_Word p_flags,
	Elf32_Word p_filesz, Elf32_Word p_memsz);
static int get_memory_regions(Elf32_Phdr phdrs[]);
static void fill_note_segment_and_entries_hdrs(Elf32_Phdr phdrs[],
	Elf32_Nhdr nhdrs[]);
static void adjust_offsets(Elf32_Phdr phdrs[], int phnum);
static void dump_elf_header(struct filp *f, Elf32_Ehdr elf_header);
static void dump_notes(struct filp *f, Elf32_Nhdr nhdrs[], int csig,
	char *proc_name);
static void dump_program_headers(struct filp *f, Elf_Phdr phdrs[], int
	phnum);
static void dump_segments(struct filp *f, Elf32_Phdr phdrs[], int
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
  elf_header->e_ident[EI_OSABI] = ELFOSABI_FREEBSD;
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
  const char *note_name = ELF_NOTE_MINIX_ELFCORE_NAME "\0";
  int name_len, mei_len, gregs_len;

  /* Size of notes in the core file is rather fixed:
   * sizeof(minix_elfcore_info_t) +
   * 2 * sizeof(Elf_Nhdr) + the size of the padded name of the note
   * - i.e. "MINIX-CORE\0" padded to 4-byte alignment => 2 * 8 bytes
   */

  name_len = strlen(note_name) + 1;
  mei_len = sizeof(minix_elfcore_info_t);
  gregs_len = sizeof(gregset_t);

  /* Make sure to also count the padding bytes */
  filesize = PAD_LEN(mei_len) + PAD_LEN(gregs_len) +
	2 * sizeof(Elf_Nhdr) + 2 * PAD_LEN(name_len);
  fill_prog_header(&phdrs[0], PT_NOTE, 0, 0, PF_R, filesize, 0);

  /* First note entry header */
  nhdrs[0].n_namesz = name_len;
  nhdrs[0].n_descsz = sizeof(minix_elfcore_info_t);
  nhdrs[0].n_type = NT_MINIX_ELFCORE_INFO;

  /* Second note entry header */
  nhdrs[1].n_namesz = name_len;
  nhdrs[1].n_descsz = sizeof(gregset_t);
  nhdrs[1].n_type = NT_MINIX_ELFCORE_GREGS;
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
  char *note_name = ELF_NOTE_MINIX_ELFCORE_NAME "\0";
  char pad[4];
  minix_elfcore_info_t mei;
  int mei_len = sizeof(minix_elfcore_info_t);
  int gregs_len = sizeof(gregset_t);
  struct stackframe_s regs;

  /* Dump first note entry */
  mei.mei_version = MINIX_ELFCORE_VERSION;
  mei.mei_meisize = mei_len;
  mei.mei_signo = csig;
  mei.mei_pid = fp->fp_pid;
  memcpy(mei.mei_command, proc_name, sizeof(mei.mei_command));

  write_buf(f, (char *) &nhdrs[0], sizeof(Elf_Nhdr));
  write_buf(f, note_name, nhdrs[0].n_namesz);
  write_buf(f, pad, PAD_LEN(nhdrs[0].n_namesz) - nhdrs[0].n_namesz);
  write_buf(f, (char *) &mei, mei_len);
  write_buf(f, pad, PAD_LEN(mei_len) - mei_len);

  /* Get registers */
  if (sys_getregs(&regs, fp->fp_endpoint) != OK)
	printf("VFS: Could not read registers\n");

  if (sizeof(regs) != gregs_len)
	printf("VFS: Wrong core register structure size\n");

  /* Dump second note entry - the general registers */
  write_buf(f, (char *) &nhdrs[1], sizeof(Elf_Nhdr));

  write_buf(f, note_name, nhdrs[1].n_namesz);
  write_buf(f, pad, PAD_LEN(nhdrs[1].n_namesz) - nhdrs[1].n_namesz);
  write_buf(f, (char *) &regs, gregs_len);
  write_buf(f, pad, PAD_LEN(gregs_len) - gregs_len);
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
			continue;
		}

		write_buf(f, (char *) buf, (off + CLICK_SIZE <= (off_t) len) ?
					CLICK_SIZE : (len - off));
	}
  }
}
