/*
 * This file mainly provides a definition of the structures used
 * to describe the notes section of an ELF file. It doesn't have
 * anything to do with the /proc file system, even though MINIX
 * has one.
 *
 * The whole purpose of this file is for GDB and GDB only.
 */

#ifndef _SYS_PROCFS_H_
#define _SYS_PROCFS_H_

#include <sys/param.h>
#include <sys/elf_core.h>
#include <i386/stackframe.h>

/*
 *
 * These structures define an interface between core files and the debugger.
 * Never change or delete any elements. These structures are modeled from
 * the file with the same name from FreeBSD
 * 
 * A lot more things should be added to these structures.  At present,
 * they contain the absolute bare minimum required to allow GDB to work
 * with ELF core dumps.
 */

/*
 * The parenthsized numbers like (1) indicate the minimum version number
 * for which each element exists in the structure.
 */

#define PRSTATUS_VERSION	1	/* Current version of prstatus_t */

typedef struct prstatus {
    int		pr_version;	/* Version number of struct (1) */
    size_t	pr_statussz;	/* sizeof(prstatus_t) (1) */
    size_t	pr_gregsetsz;	/* sizeof(gregset_t) (1) */
    size_t	pr_fpregsetsz;	/* sizeof(fpregset_t) (1) */
    int		pr_osreldate;	/* Kernel version (1) */
    int		pr_cursig;	/* Current signal (1) */
    pid_t	pr_pid;		/* Process ID (1) */
    gregset_t	pr_reg;		/* General purpose registers (1) */
} prstatus_t;

#define PRARGSZ		80	/* Maximum argument bytes saved */

#ifndef MAXCOMLEN
# define MAXCOMLEN      16      /* Maximum command line arguments */
#endif

#define PRPSINFO_VERSION	1	/* Current version of prpsinfo_t */

typedef struct prpsinfo {
    int		pr_version;	/* Version number of struct (1) */
    size_t	pr_psinfosz;	/* sizeof(prpsinfo_t) (1) */
    char	pr_fname[MAXCOMLEN+1];	/* Command name, null terminated (1) */
    char	pr_psargs[PRARGSZ+1];	/* Arguments, null terminated (1) */
} prpsinfo_t;

#endif /* _SYS_PROCFS_H_ */

