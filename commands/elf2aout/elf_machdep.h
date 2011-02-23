/*	$NetBSD: elf_machdep.h,v 1.9 2001/12/09 23:05:57 thorpej Exp $	*/

#define	ELF32_MACHDEP_ENDIANNESS	ELFDATA2LSB
#define	ELF32_MACHDEP_ID_CASES						\
		case EM_386:						\
		case EM_486:						\
			break;

#define	ELF64_MACHDEP_ENDIANNESS	XXX	/* break compilation */
#define	ELF64_MACHDEP_ID_CASES						\
		/* no 64-bit ELF machine types supported */

#define	ELF32_MACHDEP_ID		EM_386

#define ARCH_ELFSIZE		32	/* MD native binary size */

/* i386 relocations */
#define	R_386_NONE	0
#define	R_386_32	1
#define	R_386_PC32	2
#define	R_386_GOT32	3
#define	R_386_PLT32	4
#define	R_386_COPY	5
#define	R_386_GLOB_DAT	6
#define	R_386_JMP_SLOT	7
#define	R_386_RELATIVE	8
#define	R_386_GOTOFF	9
#define	R_386_GOTPC	10
/* The following relocations are GNU extensions. */
#define	R_386_16	20
#define	R_386_PC16	21
#define	R_386_8		22
#define	R_386_PC8	23

#define	R_TYPE(name)	__CONCAT(R_386_,name)
