/*
 * This file mainly provides a definition of the structures used
 * to describe the notes section of an ELF file.
 *
 * It is the interface between MINIX core dump and GDB
 */

#ifndef _SYS_ELF_CORE_H_
#define _SYS_ELF_CORE_H_

#include <sys/param.h>
#include <machine/stackframe.h>

/* MINIX core file format:
 *
 * The core file is in ELF format. Besides the regular program segments,
 * it has a NOTE segment which contains two NOTE entries
 * - one containing a minix_elfcore_info_t and the other one containing
 * the general purpose registers (a stackframe_s structure)
 */

typedef struct stackframe_s gregset_t; /* General purpose registers */

/*
 * A lot more things should be added to these structures.  At present,
 * they contain the absolute bare minimum required to allow GDB to work
 * with MINIX ELF core dumps.
 */

#define ELF_NOTE_MINIX_ELFCORE_NAME	"MINIX-CORE"
#define NT_MINIX_ELFCORE_INFO		1
#define NT_MINIX_ELFCORE_GREGS		2

#define MINIX_ELFCORE_VERSION		1  /* Current version of minix_elfcore_info_t */
#define RESERVED_SIZE                   5

typedef struct minix_elfcore_info {
  /* Version 1 fields */
  uint32_t	mei_version;		/* Version number of struct */
  uint32_t	mei_meisize;		/* sizeof(minix_elfcore_info_t) */
  uint32_t	mei_signo;		/* killing signal */
  int32_t	mei_pid;		/* Process ID */
  int8_t        mei_command[MAXCOMLEN]; /* Command */
  uint32_t      flags;                  /* Flags */
  int32_t       reserved[RESERVED_SIZE];/* Reserved space*/
  /* Put below version 2 fields */
} minix_elfcore_info_t;

#endif /* _SYS_ELF_CORE_H_ */

