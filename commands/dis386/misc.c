/*
 * misc.c: interface to Bruce Evan's dis86 package.
 *
 * $Id: misc.c,v 1.1 1997/10/20 12:00:00 cwr Exp cwr $
 *
 * Heavily modified by C W Rose.
 */

/* Version settings */
#define MINIX
#undef OS2
#undef TEST

#include <sys/types.h>
#ifdef MINIX
#include <minix/config.h>
#include <minix/const.h>
#include <a.out.h>
#endif
#ifdef OS2
typedef unsigned char u8_t;
typedef unsigned int u16_t;
typedef unsigned long u32_t;
#include </local/minix/minix/config.h>
#include </local/minix/minix/const.h>
#include </local/minix/a.out.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "const.h"
#include "type.h"
#undef S_ABS				/* clash with a.out.h */
#include "out.h"			/* ACK compiler output header */
#include "var.h"			/* db header */
#include "dis386.h"			/* dis386 package header */

/* Standard defines */
#define FAILED -1
#define MAYBE 0
#define OK 1

/* Local defines */

#ifndef lint
static char *Version = "@(#) misc.c $Revision: 1.1 $ $Date: 1997/10/20 12:00:00 $";
#endif

/* Global variables */
PRIVATE bool_t forceupper;
PRIVATE bool_t someupper = TRUE;
PRIVATE count_t stringcount = 0;
PRIVATE char *string_ptr = (char *)0;	/* stringptr ambiguous at 8th char */
PRIVATE char *stringstart = (char *)0;

/* Externals */

/* Forward declarations */
#if 0
PUBLIC void closestring(void);				/* */
PUBLIC u8_pt get8(void);				/* */
PUBLIC u16_t get16(void);				/* */
PUBLIC u32_t get32(void);				/* */
PUBLIC void openstring(char *string, int len);		/* */
PUBLIC void outbyte(char_pt byte);			/* */
PUBLIC void outcolon(void);				/* */
PUBLIC void outcomma(void);				/* */
PUBLIC void outh4(u4_pt num);				/* */
PUBLIC void outh8(u8_pt num);				/* */
PUBLIC void outh16(u16_t num);				/* */
PUBLIC void outh32(u32_t num);				/* */
PUBLIC bool_pt outnl(void);				/* */
PUBLIC count_t outsegaddr(struct address_s *ap, offset_t addr);	/* */
PUBLIC count_t outsegreg(offset_t num);			/* */
PUBLIC void outspace(void);				/* */
PUBLIC void outstr(char *s);				/* */
PUBLIC void outtab(void);				/* */
PUBLIC void outustr(char *s);				/* */
PUBLIC count_t stringpos(void);				/* */
PUBLIC count_t stringtab(void);				/* */
PUBLIC void outrel(struct nlist *sp, offset_t off);	/* */
PUBLIC void outsym(struct nlist *sp, offset_t off);	/* */
PUBLIC struct nlist *findrval(offset_t value, int where);/* */
PUBLIC struct nlist *findsval(offset_t value, int where);/* */
PUBLIC int dasm(offset_t addr, offset_t count);		/* */
#endif

PRIVATE u8_pt peek8(struct address_s *ap);			/* */
PRIVATE u16_t peek16(struct address_s *ap);			/* */
PRIVATE u32_t peek32(struct address_s *ap);			/* */
PRIVATE struct nlist *find_arval(offset_t value, int where);	/* */
PRIVATE struct nlist *find_orval(offset_t value, int where);	/* */
PRIVATE struct nlist *find_asval(offset_t value, int where);	/* */
PRIVATE struct nlist *find_osval(offset_t value, int where);	/* */
PRIVATE int dis_one(void);					/* */


/*
 * Close string device.
 */
PUBLIC void closestring()
{
  stringcount = 0;
  stringstart = string_ptr = (char *)0;
}


/*
 * Get 8 bits from current instruction pointer and advance pointer.
 */
PUBLIC u8_pt get8()
{
  u8_pt temp;

  temp = peek8(&uptr);
  ++uptr.off;
  return temp;
}


/*
 * Get 16 bits from current instruction pointer and advance pointer.
 */
PUBLIC u16_pt get16()
{
  u16_pt temp;

  temp = peek16(&uptr);
  uptr.off += 2;
  return temp;
}


/*
 * Get 32 bits from current instruction pointer and advance pointer.
 */
PUBLIC u32_t get32()
{
  u32_t temp;

  temp = peek32(&uptr);
  uptr.off += 4;
  return temp;
}


/*
 * Open string device.
 */
PUBLIC void openstring(string, len)
char *string; int len;
{
  while (--len >= 0)
	string[len] = '\0';
  stringcount = 0;
  stringstart = string_ptr = string;
}


/*
 * Print char to currently open output devices.
 */
PUBLIC void outbyte(char_pt byte)
{
  /* convert to upper case if required */
  if (forceupper && byte >= 'a' && byte <= 'z')
	byte += 'A' - 'a';

  /* increment the output line character count, allowing for tab stops */
  if (string_ptr != NULL) {
	if ((*string_ptr++ = byte) == '\t')
		stringcount = 8 * (stringcount / 8 + 1);
	else
		++stringcount;
  }
  else
	(void) fputc(byte, stdout);
}


/*
 * Print colon.
 */
PUBLIC void outcolon()
{
  outbyte(':');
}


/*
 * Print comma.
 */
PUBLIC void outcomma()
{
  outbyte(',');
}


/*
 * Print 4 bits hex.
 */
PUBLIC void outh4(u4_pt num)
{
  static char hexdigits[] = "0123456789abcdef";

  forceupper = someupper;
  outbyte(hexdigits[num % 16]);
  forceupper = FALSE;
}


/*
 * Print 8 bits hex.
 */
PUBLIC void outh8(u8_pt num)
{
  outh4(num / 16);
  outh4(num);
}


/*
 * Print 16 bits hex.
 */
PUBLIC void outh16(u16_pt num)
{
  outh8(num / 256);
  outh8(num);
}


/*
 * Print 32 bits hex.
 */
PUBLIC void outh32(u32_t num)
{
  outh16((u16_t) (num >> 16));
  outh16((u16_t) num);
}


/*
 * Print newline.
 */
PUBLIC bool_pt outnl()
{
  /* bool_pt avoids change in type.h */
  outstr("\n");
  return OK;
}


/*
 * Print segmented address.
 */
PUBLIC count_t outsegaddr(struct address_s *ap, offset_t addr)
{
  count_t bytes_printed;

  bytes_printed = 2;

  if (ap->base == regs.csbase)
	outustr("cs");
  else if (ap->base == regs.dsbase)
	outustr("ds");
  else if (ap->base == regs.esbase)
	outustr("es");
  else if (processor >= 386 && ap->base == regs.fsbase)
	outustr("fs");
  else if (processor >= 386 && ap->base == regs.gsbase)
	outustr("gs");
  else if (ap->base == regs.ssbase)
	outustr("ss");
  else
	bytes_printed = outsegreg(ap->base);

  if (bytes_printed > 4)
	outbyte('+');
  else
	outcolon();
  bytes_printed++;

  if (ap->off >= 0x10000) {
	outh32(ap->off + addr);
	return bytes_printed + 8;
  }
  else {
	outh16((u16_pt) ap->off + addr);
	return bytes_printed + 4;
  }
}


/*
 * Print segment register.
 */
PUBLIC count_t outsegreg(offset_t num)
{
  if ((num % HCLICK_SIZE) != 0 || num >= 0x100000) {
	outh32(num);
	return 8;
  }
  outh16((u16_pt) (num / HCLICK_SIZE));
  return 4;
}


/*
 * Print space.
 */
PUBLIC void outspace()
{
  outbyte(' ');
}


/*
 * Print string.
 */
PUBLIC void outstr(char *s)
{
  while (*s)
	outbyte(*s++);
}

/*
 * Print tab.
 */
PUBLIC void outtab()
{
  outbyte('\t');
}


/*
 * Print string, perhaps converting case to upper.
 */
PUBLIC void outustr(char *s)
{
  forceupper = someupper;
  while (*s)
	outbyte(*s++);
  forceupper = FALSE;
}


/*
 * p e e k 8
 *
 * Get a byte from the process.
 *
 * Returns:	byte	Success
 *
 * Note: aborts on read error.
 */
PRIVATE u8_pt peek8(struct address_s *ap)
{
  unsigned int uj;

  /* with luck buffering should make this fairly quick */
  if (fseek(disfp, (long)(ap->off), SEEK_CUR) != 0) {
	fprintf(stderr, "Cannot seek forward in object file\n");
	exit(1);
  }
  uj = fgetc(disfp) & 0377;
  if (fseek(disfp, -(long)(ap->off + 1), SEEK_CUR) != 0) {
	fprintf(stderr, "Cannot seek backward in object file\n");
	exit(1);
  }

  return uj;
}


/*
 * p e e k 1 6
 *
 * Get a 16-bit short from the process.
 *
 * Returns:	2 bytes	Success
 *
 * Note: aborts on read error.
 */
PRIVATE u16_t peek16(struct address_s *ap)
{
  unsigned int uj;

  /* with luck buffering should make this fairly quick */
  if (fseek(disfp, (long)(ap->off), SEEK_CUR) != 0) {
	fprintf(stderr, "Cannot seek forward in object file\n");
	exit(1);
  }
  /* Intel has right to left byte ordering */
#if 1
  uj = fgetc(disfp) & 0377;
  uj |= (fgetc(disfp) & 0377) << 8;
#else
  uj = fgetc(disfp) & 0377;
  uj <<= 8;
  uj |= fgetc(disfp) & 0377;
#endif
  if (fseek(disfp, -(long)(ap->off + 2), SEEK_CUR) != 0) {
	fprintf(stderr, "Cannot seek backward in object file\n");
	exit(1);
  }

  return uj;
}


/*
 * p e e k 3 2
 *
 * Get a 32-bit int from the process.
 *
 * Returns:	4 bytes	Success
 *
 * Note: aborts on read error.
 */
PRIVATE u32_t peek32(struct address_s *ap)
{
  unsigned int uj;

  /* with luck buffering should make this fairly quick */
  if (fseek(disfp, (long)(ap->off), SEEK_CUR) != 0) {
	fprintf(stderr, "Cannot seek forward in object file\n");
	exit(1);
  }
#if 1
  /* Intel has right to left byte ordering */
  uj = fgetc(disfp) & 0377;
  uj |= (fgetc(disfp) & 0377) << 8;
  uj |= (fgetc(disfp) & 0377) << 16;
  uj |= (fgetc(disfp) & 0377) << 24;
#else
  uj = fgetc(disfp) & 0377;
  uj <<= 8;
  uj |= fgetc(disfp) & 0377;
  uj <<= 8;
  uj |= fgetc(disfp) & 0377;
  uj <<= 8;
  uj |= fgetc(disfp) & 0377;
#endif
  if (fseek(disfp, -(long)(ap->off + 4), SEEK_CUR) != 0) {
	fprintf(stderr, "Cannot seek backward in object file\n");
	exit(1);
  }

  return uj;
}


/*
 * Return current offset of string device.
 */
PUBLIC count_t stringpos()
{
  return string_ptr - stringstart;
}


/*
 * Return current "tab" spot of string device.
 */
PUBLIC count_t stringtab()
{
  return stringcount;
}

/******************** sym.c ***********************/

/*
 * f i n d r v a l
 *
 * Check if an address refers to a relocation structure,
 * and if so return the table entry.
 *
 * Returns:	Pointer to struct nlist		Success
 * 		Null pointer			Failure
 *
 * Note that the nlist interface must be maintained for use by unasm().
 */
PUBLIC struct nlist *findrval(offset_t value, int where)
{
  if (aoutfp != (FILE *)NULL)
	return (find_arval(value, where));
  else if (objfp != (FILE *)NULL)
	return (find_orval(value, where));
  else
	return (struct nlist *)NULL;
}


/*
 * f i n d _ a r v a l
 *
 * Check if an address refers to an a.out file relocation structure,
 * and if so return the table entry.
 *
 * Returns:	Pointer to struct nlist		Success
 * 		Null pointer			Failure
 *
 * Note that the nlist interface must be maintained for use by unasm().
 * ### Do any available ACK compilers have this feature?
 */
PRIVATE struct nlist *find_arval(offset_t value, int where)
{
  return (struct nlist *)NULL;
}


/*
 * f i n d _ o r v a l
 *
 * Check if an address refers to an object file relocation structure,
 * and if so return the table entry.
 *
 * Returns:	Pointer to struct nlist		Success
 * 		Null pointer			Failure
 *
 * Note that the nlist interface must be maintained for use by unasm().
 * The table entry is stored in a static buffer which is overwritten
 * on successive calls.
 */
PRIVATE struct nlist *find_orval(offset_t value, int where)
{
  char data[20];
  int j, k, status;
  long int lj;
  static struct nlist sym;

  /* we need to have an object file */
  if (objfp == (FILE *)NULL) return (struct nlist *)NULL;

  /* Sections in an object file usually have the order text, rom, data, bss.
   * The order is actually set out in the section data header.  Assume that
   * the first user section is text, and all else is data.
   */
  if (where != CSEG && where != DSEG)
	return(struct nlist *)NULL;

  /* check for a relocation entry */
  status = FAILED;
  for (j = 0 ; j < o_hdrbuf.oh_nrelo ; j++) {
	if (value == o_reltab[j].or_addr) {
		/* abandon non-matching section entries */
		if (where == CSEG && (o_reltab[j].or_sect & S_TYP) != S_MIN)
			continue;
		if (where == DSEG && ((o_reltab[j].or_sect & S_TYP) <= S_MIN ||
				(o_reltab[j].or_sect & S_TYP) > (S_MIN + 3)))
			continue;
		/* the address is an offset from the symbol or section base */
		if (o_reltab[j].or_nami < o_hdrbuf.oh_nname) {
			lj = o_symtab[o_reltab[j].or_nami].on_foff -
					(long)OFF_CHAR(o_hdrbuf);
			/* check that addressing isn't messed up */
			assert(lj >= 0 && lj < o_hdrbuf.oh_nchar);
			/* name size is defined by SZ_NAME */
			sprintf(data, "%-13s", o_strtab + lj);
			/* convert from rel table to executable symbol table format */
			for (k = 0 ; k < sizeof(sym.n_name) ; k++) {
				sym.n_name[k] = data[k];/* 8 characters */
			}
			sym.n_value = o_symtab[o_reltab[j].or_nami].on_valu;
							/* long */
#if 1
			sym.n_sclass = (where == CSEG) ? N_TEXT : N_DATA;
#else
			sym.n_sclass = (o_symtab[o_reltab[j].or_nami].on_type &
					S_TYP) - S_MIN;	/* unsigned char */
#endif
			sym.n_numaux = 0;		/* unsigned char */
			sym.n_type = 0;			/* unsigned short */
			status = OK;
			break;
		}
		/* the address is an absolute number relative to the pc */
		else if (o_reltab[j].or_nami == o_hdrbuf.oh_nname) {
			strcpy(data, "Absolute");
			/* convert from relocation data to executable symbol table format */
			for (k = 0 ; k < sizeof(sym.n_name) ; k++) {
				sym.n_name[k] = data[k];
			}
			sym.n_value = 0;
			sym.n_sclass = (where == CSEG) ? N_TEXT : N_DATA;
			sym.n_numaux = 0;
			sym.n_type = 0;
			status = OK;
			break;
		}
	}
  }
  return (status == OK ? &sym : (struct nlist *)NULL);
}


/*
 * f i n d s v a l
 *
 * Check if an address refers to a symbol,
 * and if so return the table entry.
 *
 * Returns:	Pointer to struct nlist		Success
 * 		Null pointer			Failure
 *
 * Note that the nlist interface must be maintained for use by unasm().
 */
PUBLIC struct nlist *findsval(offset_t value, int where)
{
  if (aoutfp != (FILE *)NULL)
	return (find_asval(value, where));
  else if (objfp != (FILE *)NULL)
	return (find_osval(value, where));
  else
	return (struct nlist *)NULL;
}


/*
 * f i n d _ a s v a l
 *
 * Check if an address refers to an a.out file symbol,
 * and if so return the table entry.
 *
 * Returns:	Pointer to struct nlist		Success
 * 		Null pointer			Failure
 *
 * Note that the nlist interface must be maintained for use by unasm().
 * The table entry is stored in a static buffer which is overwritten
 * on successive calls.
 */
PRIVATE struct nlist *find_asval(offset_t value, int where)
{
  int j, status;
  static struct nlist sym;

  /* Sections in an a.out file have the order text, data, bss
   * but this function is called only with CSEG and DSEG.
   */
  if (where != CSEG && where != DSEG)
	return(struct nlist *)NULL;

  /* do a linear search for a symbol, as the symbol tables are unsorted */
  status = FAILED;
  for (j = 0 ; j < (a_hdrbuf.a_syms / sizeof(struct nlist)) ; j++) {
	if (value == a_symtab[j].n_value &&
			((where == CSEG && (a_symtab[j].n_sclass & N_SECT) == N_TEXT) ||
			(where == DSEG && ((a_symtab[j].n_sclass & N_SECT) == N_DATA ||
			(a_symtab[j].n_sclass & N_SECT) == N_BSS)))) {
		(void) memcpy(&sym, &a_symtab[j], sizeof(struct nlist));
		status = OK;
		break;
	}
  }
  return (status == OK) ? &sym : (struct nlist *)NULL;
}


/*
 * f i n d _ o s v a l
 *
 * Check if an address refers to an object file symbol,
 * and if so return the table entry.
 *
 * Returns:	Pointer to struct nlist		Success
 * 		Null pointer			Failure
 *
 * Note that the nlist interface must be maintained for use by unasm().
 * The table entry is stored in a static buffer which is overwritten
 * on successive calls.
 */
PRIVATE struct nlist *find_osval(offset_t value, int where)
{
  int j, k, sec, status;
  long int lj;
  struct locname *np;
  static struct nlist sym;

  /* Sections in an object file usually have the order text, rom, data, bss.
   * The order is actually set out in the section data header.  Assume that
   * the first user section is text, and all else is data.
   */
  if (where != CSEG && where != DSEG)
	return(struct nlist *)NULL;

  /* do a linear search for a local symbol, as the tables are unsorted */
  status = FAILED;
  if (where == DSEG) {
	/* nb. hardcoded assumption of section order */
	for (sec = 1 ; status == FAILED && sec < 4 ; sec++) {
		for (np = locsym[sec] ; status == FAILED && np !=
					(struct locname *)NULL ; np = np->l_next) {
			if (np->l_value == value) {
				for (k = 0 ; k < sizeof(sym.n_name) ; k++) {
					sym.n_name[k] = np->l_name[k];/* 8 characters */
				}
				sym.n_value = value;		/* long */
				sym.n_sclass = N_DATA;		/* unsigned char */
				sym.n_numaux = 0;		/* unsigned char */
				sym.n_type = 0;			/* unsigned short */
				status = OK;
			}
		}
	}
  }

  /* do a linear search for a symbol, as the symbol tables are unsorted */
  for (j = 0 ; status == FAILED && j < o_hdrbuf.oh_nname ; j++) {
	if (value == o_symtab[j].on_valu) {
		/* abandon non-matching section entries */
		if (where == CSEG && (o_symtab[j].on_type & S_TYP) != S_MIN)
			continue;
		if (where == DSEG && ((o_symtab[j].on_type & S_TYP) <= S_MIN ||
				(o_symtab[j].on_type & S_TYP) > (S_MIN + 3)))
			continue;
#if 0
			((where == CSEG && sect == (o_symtab[j].on_type & S_TYP)) ||
			(where == DSEG && sect <= (o_symtab[j].on_type & S_TYP)))) {
#endif
		/* find the name in the object file symbol table */
		lj = o_symtab[j].on_foff - (long)OFF_CHAR(o_hdrbuf);
		/* check that the offset addressing isn't messed up */
		assert(lj >= 0 && lj < o_hdrbuf.oh_nchar);
		/* convert from object to executable symbol table format */
		for (k = 0 ; k < sizeof(sym.n_name) ; k++) {
			sym.n_name[k] = *(o_strtab + lj + k);
							/* 8 characters */
		}
		sym.n_value = o_symtab[j].on_valu;	/* long */
		sym.n_sclass = (where == CSEG) ? N_TEXT : N_DATA;
							/* unsigned char */
		sym.n_numaux = 0;			/* unsigned char */
		sym.n_type = 0;				/* unsigned short */
		status = OK;
	}
  }

  return (status == OK ? &sym : (struct nlist *)NULL);
}


/*
 * o u t r e l
 *
 * Output a symbol name from an nlist structure.
 *
 * Returns:	Nothing		Always
 *
 * Note that the nlist interface must be maintained for use by unasm().
 * The label may be a segment name, in which case the address is relative
 * to that segment and must be dereferenced further.
 */
PUBLIC void outrel(struct nlist *sp, offset_t off)
{
  char data[20];
  int j, k;
  struct nlist *spnew;

  /* get a local copy of the label */
  for (j = 0 ; j < 20 ; j++) {
	data[j] = sp->n_name[j];
	if (data[j] == ' ' || data[j] == '\0')
		break;
  }
  data[j] = '\0';
  data[8] = '\0';

  /* see if we have a section name */
  for (k = 0 ; k < 4 ; k++) {
	if (strcmp(data, o_secnam[k]) == 0) {
		/* look up the name in the appropriate section */
		if ((spnew = findsval(off, (k ? DSEG : CSEG))) != (struct nlist *)NULL) {
			/* get a local copy of the label */
			for (j = 0 ; j < 20 ; j++) {
				data[j] = spnew->n_name[j];
				if (data[j] == '\0') break;
			}
			data[8] = '\0';
		}
	}
  }

  /* output the result */
  for (j = 0 ; data[j] != 0 ; j++)
	outbyte(data[j]);
}


/*
 * o u t s y m
 *
 * Output a symbol name from an nlist structure.
 *
 * Returns:	Nothing		Always
 *
 * Note that the nlist interface must be maintained for use by unasm().
 */
PUBLIC void outsym(struct nlist *sp, offset_t off)
{
  char *s;
  char *send;

  /* output the symbol name */
  for (s = sp->n_name, send = s + sizeof sp->n_name; *s != 0 && s < send; ++s)
	outbyte(*s);

  /* if the required address is offset from the name, output that too */
  if ((off -= sp->n_value) != 0) {
	outbyte('+');
	if (off >= 0x10000)
		outh32(off);
	else if (off >= 0x100)
		outh16((u16_pt) off);
	else
		outh8((u8_pt) off);
  }
}


/*
 * d a s m
 *
 * Disassemble a stream of instructions.
 *
 * Returns:	OK	Success
 *		FAILED	Otherwise
 */
PUBLIC int dasm(offset_t addr, offset_t count)
{
#if (_WORD_SIZE == 4)
  bits32 = TRUE;		/* set mode */
#else
  bits32 = FALSE;
#endif
  processor = bits32 ? 386 : 0;
  uptr.off = 0;
  uptr.base = 0;

  while (uptr.off < count) {
	addrbase = addr;
	/* assume that the object file text segment is first */
	if (objfp != (FILE *)NULL && uptr.off >= o_sectab[0].os_flen)
		return FAILED;
	if (aoutfp != (FILE *)NULL && uptr.off >= (A_DATAPOS(a_hdrbuf) - 1))
		return FAILED;
	if (dis_one() == FAILED)
		return FAILED;
  }
  return OK;
}


/*
 * d i s _ o n e
 *
 * Disassemble a single instruction.
 *
 * Returns:	OK		Always
 *
 * File read failures are handled at a low level by simply
 * baling out of the program; the startup checks on file
 * readability should make this a rare occurrence.  Hence
 * there are no error returns from this routine.
 * The output is written into a static line buffer, which
 * is overwritten on successive calls.
 */
PRIVATE int dis_one()
{
  int idone, column, maxcol;
  static char line[81];
  struct address_s newuptr;
  struct address_s olduptr;
  struct nlist *sp;

  do {
	/* output a label */
	if ((sp = findsval(uptr.off + addrbase, CSEG)) != NULL
			&& sp->n_value == uptr.off + addrbase) {
		outsym(sp, uptr.off + addrbase);
		outbyte(':');
		(void) outnl();
	}

	/* park the current address */
	olduptr = uptr;

	/* initialise the string input */
	openstring(line, sizeof(line));

	/* output an instruction */
	idone = puti();

	/* terminate the line buffer */
	line[stringpos()] = 0;

	/* deinitialise the string input */
	closestring();

	/* park the new address, set by puti() */
	newuptr = uptr;

	/* get back the current address */
	uptr = olduptr;

	/* output the segment data */
	column = outsegaddr(&uptr, addrbase);
	outspace();
	outspace();
	column += 2;

	/* output the raw bytes of the current instruction */
	while (uptr.off != newuptr.off) {
	    outh8(get8());
	    column += 2;
	}

	/* format the disassembled output */
	maxcol = bits32 ? 24 : 16;
	while (column < maxcol) {
	    outtab();
	    column += 8;
	}
	outtab();

	/* display the collected buffer */
	outstr(line);
	(void) outnl();
  } while (!idone);			/* eat all prefixes */

  return OK;
}

/*
 * EOF
 */

