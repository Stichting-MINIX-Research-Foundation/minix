/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#ifndef __OUT_H_INCLUDED
#define __OUT_H_INCLUDED
/*
 * output format for ACK assemblers
 */
#ifndef ushort
#define ushort	unsigned short
#endif /* ushort */

struct outhead {
	ushort 	oh_magic;	/* magic number */
	ushort 	oh_stamp;	/* version stamp */
	ushort	oh_flags;	/* several format flags */
	ushort	oh_nsect;	/* number of outsect structures */
	ushort	oh_nrelo;	/* number of outrelo structures */
	ushort	oh_nname;	/* number of outname structures */
	long	oh_nemit;	/* sum of all os_flen */
	long	oh_nchar;	/* size of string area */
};

#define O_MAGIC	0x0201		/* magic number of output file */
#define	O_STAMP	0		/* version stamp */
#define MAXSECT	64		/* Maximum number of sections */

#define	HF_LINK	0x0004		/* unresolved references left */
#define	HF_8086	0x0008		/* os_base specially encoded */

struct outsect {
	long 	os_base;	/* startaddress in machine */
	long	os_size;	/* section size in machine */
	long	os_foff;	/* startaddress in file */
	long	os_flen;	/* section size in file */
	long	os_lign;	/* section alignment */
};

struct outrelo {
	char	or_type;	/* type of reference */
	char	or_sect;	/* referencing section */
	ushort	or_nami;	/* referenced symbol index */
	long	or_addr;	/* referencing address */
};

struct outname {
	union {
	  char	*on_ptr;	/* symbol name (in core) */
	  long	on_off;		/* symbol name (in file) */
	}	on_u;
#define on_mptr	on_u.on_ptr
#define on_foff	on_u.on_off
	ushort	on_type;	/* symbol type */
	ushort	on_desc;	/* debug info */
	long	on_valu;	/* symbol value */
};

/*
 * relocation type bits
 */
#define RELSZ	0x07		/* relocation length */
#define RELO1	   1		/* 1 byte */
#define RELO2	   2		/* 2 bytes */
#define RELO4	   4		/* 4 bytes */
#define RELPC	0x08		/* pc relative */
#define RELBR	0x10		/* High order byte lowest address. */
#define RELWR	0x20		/* High order word lowest address. */

/*
 * section type bits and fields
 */
#define S_TYP	0x007F		/* undefined, absolute or relative */
#define S_EXT	0x0080		/* external flag */
#define S_ETC	0x7F00		/* for symbolic debug, bypassing 'as' */

/*
 * S_TYP field values
 */
#define S_UND	0x0000		/* undefined item */
#define S_ABS	0x0001		/* absolute item */
#define S_MIN	0x0002		/* first user section */
#define S_MAX	(S_TYP-1)	/* last user section */
#define S_CRS	S_TYP		/* on_valu is symbol index which contains value */

/*
 * S_ETC field values
 */
#define S_SCT	0x0100		/* section names */
#define S_LIN	0x0200		/* hll source line item */
#define S_FIL	0x0300		/* hll source file item */
#define S_MOD	0x0400		/* ass source file item */
#define S_COM	0x1000		/* Common name. */
#define S_STB	0xe000		/* entries with any of these bits set are
				   reserved for debuggers
				*/

/*
 * structure format strings
 */
#define SF_HEAD		"22222244"
#define SF_SECT		"44444"
#define SF_RELO		"1124"
#define SF_NAME		"4224"

/*
 * structure sizes (bytes in file; add digits in SF_*)
 */
#define SZ_HEAD		20
#define SZ_SECT		20
#define SZ_RELO		8
#define SZ_NAME		12

/*
 * file access macros
 */
#define BADMAGIC(x)	((x).oh_magic!=O_MAGIC)
#define OFF_SECT(x)	SZ_HEAD
#define OFF_EMIT(x)	(OFF_SECT(x) + ((long)(x).oh_nsect * SZ_SECT))
#define OFF_RELO(x)	(OFF_EMIT(x) + (x).oh_nemit)
#define OFF_NAME(x)	(OFF_RELO(x) + ((long)(x).oh_nrelo * SZ_RELO))
#define OFF_CHAR(x)	(OFF_NAME(x) + ((long)(x).oh_nname * SZ_NAME))

#endif /* __OUT_H_INCLUDED */
