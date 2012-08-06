#define _SYSTEM 1

#include <minix/type.h>
#include <minix/const.h>
#include <a.out.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <libexec.h>

int read_header_aout(
  const char *exec_hdr,		/* executable header */
  size_t exec_len,		/* executable file size */
  int *sep_id,			/* true iff sep I&D */
  vir_bytes *text_bytes,	/* place to return text size */
  vir_bytes *data_bytes,	/* place to return initialized data size */
  vir_bytes *bss_bytes,		/* place to return bss size */
  phys_bytes *tot_bytes,	/* place to return total size */
  vir_bytes *pc,		/* program entry point (initial PC) */
  int *hdrlenp
)
{
/* Read the header and extract the text, data, bss and total sizes from it. */
  struct exec *hdr;		/* a.out header is read in here */

  /* Read the header and check the magic number.  The standard MINIX header
   * is defined in <a.out.h>.  It consists of 8 chars followed by 6 longs.
   * Then come 4 more longs that are not used here.
   *	Byte 0: magic number 0x01
   *	Byte 1: magic number 0x03
   *	Byte 2: normal = 0x10 (not checked, 0 is OK), separate I/D = 0x20
   *	Byte 3: CPU type, Intel 16 bit = 0x04, Intel 32 bit = 0x10,
   *            Motorola = 0x0B, Sun SPARC = 0x17
   *	Byte 4: Header length = 0x20
   *	Bytes 5-7 are not used.
   *
   *	Now come the 6 longs
   *	Bytes  8-11: size of text segments in bytes
   *	Bytes 12-15: size of initialized data segment in bytes
   *	Bytes 16-19: size of bss in bytes
   *	Bytes 20-23: program entry point
   *	Bytes 24-27: total memory allocated to program (text, data + stack)
   *	Bytes 28-31: size of symbol table in bytes
   * The longs are represented in a machine dependent order,
   * little-endian on the 8088, big-endian on the 68000.
   * The header is followed directly by the text and data segments, and the
   * symbol table (if any). The sizes are given in the header. Only the
   * text and data segments are copied into memory by exec. The header is
   * used here only. The symbol table is for the benefit of a debugger and
   * is ignored here.
   */

  assert(exec_hdr != NULL);

  hdr = (struct exec *)exec_hdr;
  if (exec_len < A_MINHDR) return(ENOEXEC);

  /* Check magic number, cpu type, and flags. */
  if (BADMAG(*hdr)) return(ENOEXEC);
#if defined(__i386__)
  if (hdr->a_cpu != A_I80386) return(ENOEXEC);
#endif
  if ((hdr->a_flags & ~(A_NSYM | A_EXEC | A_SEP)) != 0) return(ENOEXEC);

  *sep_id = !!(hdr->a_flags & A_SEP);	    /* separate I & D or not */

  /* Get text and data sizes. */
  *text_bytes = (vir_bytes) hdr->a_text;	/* text size in bytes */
  *data_bytes = (vir_bytes) hdr->a_data;	/* data size in bytes */
  *bss_bytes  = (vir_bytes) hdr->a_bss;	/* bss size in bytes */
  *tot_bytes  = hdr->a_total;		/* total bytes to allocate for prog */
  if (*tot_bytes == 0) return(ENOEXEC);

  if (!*sep_id) {
	/* If I & D space is not separated, it is all considered data. Text=0*/
	*data_bytes += *text_bytes;
	*text_bytes = 0;
  }
  *pc = hdr->a_entry;	/* initial address to start execution */
  *hdrlenp = hdr->a_hdrlen & BYTE;		/* header length */

  return(OK);
}
