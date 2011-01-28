/*
 * dis_o386: disassemble 386 object files.
 *
 * $Id: diso.c,v 1.1 1997/10/20 12:00:00 cwr Exp cwr $
 *
 * Written by C W Rose.
 */

/* Version settings */
#define MINIX
#undef OS2
#undef TEST

#ifdef MINIX
#include <sys/types.h>
#include <sys/stat.h>
#include <minix/config.h>
#include <minix/const.h>
#include <a.out.h>
#include <minix/ansi.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#undef S_ABS		/* used in a.out.h */
#include "out.h"	/* ACK compiler output header */
#undef EXTERN
#define EXTERN
#include "dis386.h"	/* dis386 header */
#endif

#ifdef OS2
#include <sys/stat.h>
#include <sys/types.h>

#include </local/minix/minix/config.h>
#include </local/minix/minix/const.h>
#include </local/minix/a.out.h>
#include </local/minix/ansi.h>

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#undef S_ABS		/* used in a.out.h */
#include "out.h"	/* ACK compiler output header */
#undef EXTERN
#define EXTERN
#include "dis386.h"	/* dis386 header */
#endif

/* Standard defines */
#define FALSE		0
#undef TRUE
#define TRUE		!FALSE
#define FAILED		-1
#define MAYBE		0
#define OK		1
#define SAME		0

/* Local defines */
#define L_BUFF_LEN	1024
#define BUFF_LEN	256
#define S_BUFF_LEN	20
#define L_BUFF_MAX	(L_BUFF_LEN-1)
#define BUFF_MAX	(BUFF_LEN-1)
#define S_BUFF_MAX	(S_BUFF_LEN-1)
#define PSEP		'\\'

#define AOUT		"a.out"	/* useful default file names */
#define CORE		"core"
#define OBJF		"test.o"
#define STAB		"symbol.tab"
#define LINE_LEN	16
#define SYMLEN		8

#define TEXT		0	/* section indices for locsym[] */
#define ROM		1
#define DATA		2
#define BSS		3

#ifndef lint
static char *Version = "@(#) dis_o386.c $Revision: 1.1 $ $Date: 1997/10/20 12:00:00 $";
#endif

/* Global variables */
int opt_C = FALSE;		/* core file name */
int opt_E = FALSE;		/* executable file name */
int opt_O = FALSE;		/* object file name */
int opt_S = FALSE;		/* symbol table name */
int opt_a = FALSE;		/* dump tables and disassemble segments */
int opt_b = FALSE;		/* dump straight binary */
int opt_d = FALSE;		/* dump the data segment */
int opt_f = FALSE;		/* first address of dump */
int opt_h = FALSE;		/* dump the header structure */
int opt_l = FALSE;		/* last address of dump */
int opt_m = FALSE;		/* dump the rom segment */
int opt_n = FALSE;		/* dump the symbol names */
int opt_r = FALSE;		/* dump the relocation structures */
int opt_s = FALSE;		/* dump the symbol table */
int opt_t = FALSE;		/* dump the text segment */
int opt_u = FALSE;		/* dump the bss segment */
int opt_x = FALSE;		/* debugging flag */

char progname[BUFF_LEN];	/* program name */
int dbglvl = 0;			/* debugging level */

/* Forward declarations */
/* _PROTOTYPE(void usage, (void)); */
unsigned long int atoaddr(char *sp);			/* Convert ascii hex/dec to ulong */
int dump_hex(FILE *fp, long s, long n);			/* Dump bytes in hex and ascii */
int dump_odata(FILE *fp, long s, long n, int sec);	/* Dump object file data section */
int dump_ohdr(struct outhead *ph);			/* Dump object file header */
int dump_orel(FILE *fp, long s, long n);		/* Dump object file relocation section */
int dump_osec(long b, long e, int sec, int flg);	/* Dump object file section */
int dump_oshdr(FILE *fp, long s, long n);		/* Dump object file section headers */
int dump_ostr(FILE *fp, long s, long n);		/* Dump object file string data */
int dump_osym(FILE *fp, long s, long n);		/* Dump object file symbol table data */
int find_osym(long val, int sec);			/* Find object file symbol index */
int gen_locsym(FILE *fp, int sec);			/* Generate local symbols */
int getstruct(FILE *fp, char *bp, char *s);		/* Get values from the input file */
int init_objf(FILE *fp);				/* Initialise object file tables */
void usage(void);					/* Usage message */


/*
 * a t o a d d r
 *
 * Convert ascii hex/dec to unsigned long.
 *
 * Returns:	Conversion result	Always
 */
unsigned long int atoaddr(char *sp)
{
  char c, *cp, buff[S_BUFF_LEN];
  int j;
  unsigned long int result = 0;

  /* flip to upper */
  for (j = 0 ; j < S_BUFF_MAX && *(sp + j) != '\0' ; j++)
	buff[j] = toupper(*(sp + j));
  buff[j] = '\0';

  /* lose leading whitespace */
  cp = buff;
  while isspace(*cp)
	cp++;

  /* check for hexadecimal entry */
  if (*cp == '0' && *(cp + 1) == 'X') {
	cp += 2;
	while (isxdigit(*cp)) {
		c = *cp++;
		j = (c < 'A') ? c - '0' : c - 'A' + 10;
		result = (result << 4) + (c < 'A' ? c - '0' : c - 'A' + 10);
	}
  }
  else
	result = atol(buff);

  return result;
}


/*
 * d u m p _ h e x
 *
 * Dump bytes in hex and ascii.
 *
 * Returns:	OK	Success
 *		FAILED	File read failure, invalid arguments
 */
int dump_hex(FILE *fp, long start, long count)
{
  char c, buff[S_BUFF_LEN];
  int j, k, status, quit, last;
  unsigned long int ulj;

  if (start < 0)
	return(FAILED);

  ulj = 0;
  quit = FALSE;
  status = OK;
  while (TRUE) {
	/* get 16 bytes of data */
	for (j = 0 ; j < 16 ; j++) {
		if ((k = fgetc(fp)) == EOF) {
			quit = TRUE;
			break;
		}
		else
			buff[j] = (char)k;
	}

	/* set up to dump any remaining data */
	if (quit) {
		status = FAILED;
		if (j == 0)
			break;
		else
			j--;
	}
	last = j;

	/* print the address */
	fprintf(stdout, "%06lx ", start + ulj);
	ulj += 16;
	if (ulj >= count) {
		quit = TRUE;
		if (last == 16)
			last = (count - 1) % 16;
	}

	/* print a line of hex data */
	for (j = 0 ; j < 16 ; j++ ) {
		if (j <= last)
			fprintf(stdout, " %02x", buff[j] & 0xff);
		else
			fprintf(stdout, "   ");
		if (j == 7)
			fprintf(stdout, " -");
	}

	/* print a trailer of ascii data */
	fprintf(stdout, "  ");
	for (j = 0 ; j < 16 ; j++ ) {
		if (j <= last)
			c = buff[j];
		else
			c = ' ';
		if (c < 32 || c > 127)
			c = '.';
		fputc(c, stdout);
	}

	fprintf(stdout, "\n");
	if (quit)
		break;
  }

  return(status);
}


/*
 * d u m p _ o d a t a
 *
 * Dump object file data section.
 *
 * Returns:	OK	Success
 *		FAILED	File read failure, invalid arguments
 *
 * The o_hdrbuf and o_sectab structures are read to determine section addresses.
 * The o_symtab and o_strtab structures are read to determine symbol names.
 */
int dump_odata(FILE *fp, long start, long count, int sec)
{
  char label[S_BUFF_LEN], data[S_BUFF_LEN], buff[BUFF_LEN];
  char *hex = "0123456789ABCDEF";
  int j, k, newflg, index, last, status, found, quit;
  long int lj, addr;
  unsigned long int ulj;
  struct locname *np;

  if (start < 0 || (start + count) > o_sectab[sec].os_flen)
	return(FAILED);

  ulj = start;
  quit = FALSE;
  status = OK;
  for (addr = start ; addr < (start + count) ; addr += 16) {
	/* get a line's worth of data */
	for (j = 0 ; j < 16 ; j++) {
		if (j == (start + count - addr)) {
			quit = TRUE;
			break;
		}
		if ((k = fgetc(fp)) == EOF) {
			status = FAILED;
			quit = TRUE;
			break;
		}
		data[j] = (char)k;
	}

	/* adjust for an unexpected EOF */
	if (quit && status == FAILED) {
		if (j == 0)
			break;
		else
			j--;
	}
	last = j;

	/* write out the address and clear the rest of the buffer */
	sprintf(buff, "%06lx", ulj);
	for (k = strlen(buff) ; k < BUFF_MAX ; k++)
		buff[k] = ' ';

	/* build the hex and ascii data representations */
	newflg = TRUE;
	found = FALSE;
	for (j = 0 ; j < last ; j++ ) {

		/* find a local symbol, one per address */
		for (np = locsym[sec] ; !found && np != (struct locname *)NULL ;
									np = np->l_next) {
			if (ulj == np->l_value) {
				/* write out any outstanding data */
				if (j != 0) {
					buff[75] = '\0';
					fprintf(stdout, "%s\n", buff);
					for (k = 8 ; k < 75 ; k++)
						buff[k] = ' ';
				}
				/* write out the symbol name */
				for (k = 0 ; k < 8 ; k++)
					label[k] = np->l_name[k];
				label[k] = '\0';
				fprintf(stdout, "%s\n", label);
				found = TRUE;
			}
		}

		/* find any global symbols, several per address */
		while (!found && (index = find_osym(ulj, sec)) != -1) {
			/* for the first symbol, write out any outstanding data */
			if (newflg && j != 0) {
				buff[75] = '\0';
				fprintf(stdout, "%s\n", buff);
				for (k = 8 ; k < 75 ; k++)
					buff[k] = ' ';
				newflg = FALSE;
			}
			/* write out the symbol name */
			lj = o_symtab[index].on_foff - (long)OFF_CHAR(o_hdrbuf);
			for (k = 0 ; k < 8 ; k++)
				label[k] = *(o_strtab + lj + k);
			label[k] = '\0';
			fprintf(stdout, "%s\n", label);
		}

		/* set up for the next pass */
		newflg = TRUE;
		found = FALSE;
		ulj++;
		/* hex digits */
		buff[8 + (3 * j) + (j < 8 ? 0 : 2)] = hex[(data[j] >> 4) & 0x0f];
		buff[9 + (3 * j) + (j < 8 ? 0 : 2)] = hex[data[j] & 0x0f];
		/* ascii conversion */
		if (data[j] < 32 || data[j] > 127)
			buff[59 + j] = '.';
		else
			buff[59 + j] = data[j];
		if (j == 8)
			buff[32] = '-';
	}
	buff[75] = '\0';

	/* write out the result */
	fprintf(stdout, "%s\n", buff);

	if (quit) break;
  }

  return(status);
}


/*
 * d u m p _ o h d r
 *
 * Dump object file header data.
 *
 * Returns:	OK	Always
 */
int dump_ohdr(struct outhead *ph)
{
  char buff[BUFF_LEN];

  sprintf(buff, "Magic number:          0x%04.4x", ph->oh_magic);
  if (ph->oh_magic == O_MAGIC) strcat(buff, " O_MAGIC");
  else strcat(buff, " UNKNOWN");
  fprintf(stdout, "%s\n", buff);

  fprintf(stdout, "Version stamp:         0x%04.4x\n", ph->oh_stamp);

  sprintf(buff, "Format flags:          0x%04.4x", ph->oh_flags);
  if (ph->oh_flags & HF_LINK) strcat(buff, " HF_LINK");
  if (ph->oh_flags & HF_8086) strcat(buff, " HF_8086");
  if (ph->oh_flags & ~(HF_LINK | HF_8086)) strcat(buff, " UNKNOWN");
  fprintf(stdout, "%s\n", buff);

  fprintf(stdout, "Number of sections:    0x%04.4x\n", ph->oh_nsect);
  fprintf(stdout, "Number of relocations: 0x%04.4x\n", ph->oh_nrelo);
  fprintf(stdout, "Number of symbols:     0x%04.4x\n", ph->oh_nname);
  fprintf(stdout, "Sum of section sizes:  0x%08.8x\n", ph->oh_nemit);
  fprintf(stdout, "Size of string area:   0x%08.8x\n", ph->oh_nchar);

  return(OK);
}


/*
 * d u m p _ o r e l
 *
 * Dump object file relocation data.
 *
 * Returns:	OK	Success
 *		FAILED	Invalid arguments
 *
 * The o_hdrbuf and o_sectab structures are read to determine section addresses.
 * The o_symtab and o_strtab structures are read to determine symbol values.
 */
int dump_orel(FILE *fp, long start, long count)
{
  char buff[BUFF_LEN], data[S_BUFF_LEN];
  int j;
  unsigned int uj;
  long int lj;
  struct outrelo relbuf;

  if (start < 0 || (start + count) > o_hdrbuf.oh_nrelo)
	return(FAILED);

  for (j = 0 ; j < count ; j++) {
	(void) getstruct(fp, (char *)&relbuf, SF_RELO);
	sprintf(buff, "%04d Type:", j + start);
	if (relbuf.or_type & RELO1) strcat(buff, " RELO1");
	if (relbuf.or_type & RELO2) strcat(buff, " RELO2");
	if (relbuf.or_type & RELO4) strcat(buff, " RELO4");
	if (relbuf.or_type & RELPC) strcat(buff, " RELPC");
	else strcat(buff, "      ");
	if (relbuf.or_type & RELBR) strcat(buff, " RELBR");
	if (relbuf.or_type & RELWR) strcat(buff, " RELWR");
	if (relbuf.or_type & ~(RELO1 | RELO2 | RELO4 | RELPC | RELBR | RELWR))
		strcat(buff, "UNKNOWN");

	strcat(buff, " Sect:");
	uj = relbuf.or_sect & S_TYP;
	if (uj >= S_MIN && uj <= S_MAX) {
#if 1
		/* use arbitrary names for Minix 386 */
		sprintf(data, " %-5s", o_secnam[uj - S_MIN]);
#else
		sprintf(data, "  0x%02.2x", uj - S_MIN);
#endif
		strcat(buff, data);
	}
	/* S_UND is the empty S_TYP field */
	if ((relbuf.or_sect & S_TYP) == S_UND) strcat(buff, " S_UND");
	if ((relbuf.or_sect & S_TYP) == S_ABS) strcat(buff, " S_ABS");
	if ((relbuf.or_sect & S_TYP) == S_CRS) strcat(buff, " S_CRS");

	if ((relbuf.or_sect & S_EXT) == S_EXT) strcat(buff, " S_EXT");
	else strcat(buff, "      ");

	if (relbuf.or_sect & ~(S_TYP | S_EXT))
		strcat(buff, " UNKNOWN");

	strcat(buff, " Symb:");
	if (relbuf.or_nami < o_hdrbuf.oh_nname) {
		lj = o_symtab[relbuf.or_nami].on_foff - (long)OFF_CHAR(o_hdrbuf);
		/* check that addressing isn't messed up */
		assert(lj >= 0 && lj < o_hdrbuf.oh_nchar);
		/* name size is defined by SZ_NAME */
		sprintf(data, "%-13s", o_strtab + lj);
	}
	else
		sprintf(data, " 0x%04.4x", relbuf.or_nami);
	strcat(buff, data);
	strcat(buff, " Addr:");
	sprintf(data, " 0x%08.8x", relbuf.or_addr);
	strcat(buff, data);
	fprintf(stdout, "%s\n", buff);

#if 0
	printf("Type Section Symbol Address\n");
	printf("0x%02.2x 0x%02.2x 0x%04.4x 0x%08.8x\n",
		relbuf.or_type, relbuf.or_sect,
		relbuf.or_nami, relbuf.or_addr);
#endif
  }

  return(OK);
}


/*
 * d u m p _ o s e c
 *
 * Dump object file section.
 *
 * Returns:	OK	Success
 *		FAILED	Invalid arguments
 */
int dump_osec(long addrfirst, long addrlast, int sec, int full)
{
  long int addrcount;

  /* check that all offsets are valid */
  addrcount = o_sectab[sec].os_flen;
  if (addrfirst > o_sectab[sec].os_flen || addrlast > o_sectab[sec].os_flen) {
	fprintf(stderr, "Invalid %s address range 0x%08.8lu to 0x%08.8lu\n",
					o_secnam[sec], addrfirst, addrlast);
	return(FAILED);
  }
  else {
	if (opt_l)
		addrcount = addrlast + 1;
	addrcount = addrcount - addrfirst;
	(void) fseek(objfp, o_sectab[sec].os_foff, SEEK_SET);
	fprintf(stdout, "\n%s%s:\n", full ? "Disassembled " : "", o_secnam[sec]);
	if (full)
		(void) dump_odata(objfp, addrfirst, addrcount, sec);
	else
		(void) dump_hex(objfp, addrfirst, addrcount);
	return(OK);
  }
  /* NOTREACHED */
}


/*
 * d u m p _ o s h d r
 *
 * Dump object file section headers.
 *
 * Returns:	OK	Always
 *
 * The o_secnam structure is read to determine section names.
 */
int dump_oshdr(FILE *fp, long start, long count)
{
  int j;
  struct outsect secbuf;

  fprintf(stdout,
	"Name          Index Core start Core size  File start File size  Alignment\n");
  for (j = 0 ; j < count ; j++) {
	(void) getstruct(fp, (char *)&secbuf, SF_SECT);
	if (j >= start)
		fprintf(stdout, "%-13s %4.4d  0x%08.8x 0x%08.8x 0x%08.8x 0x%08.8x 0x%08.8x\n",
			o_secnam[j], j, secbuf.os_base, secbuf.os_size, secbuf.os_foff,
			secbuf.os_flen, secbuf.os_lign);
  }

  return(OK);
}


/*
 * d u m p _ o s t r
 *
 * Dump object file string data.
 *
 * Returns:	OK	Success
 *		FAILED	File read failure, invalid arguments
 *
 * The o_hdrbuf structure is read to determine section addresses.
 */
int dump_ostr(FILE *fp, long start, long count)
{
  int j, k;

  if (start < 0 || count > o_hdrbuf.oh_nname)
	return(FAILED);

  /* we cannot precalculate the offset of a name record */
  for (j = 0 ; j < count ; j++) {
	fprintf(stdout, "%04d ", j + start);
	do {
		switch (k = fgetc(fp)) {
		case EOF:
			return(FAILED);
			break;
		case 0:
			fprintf(stdout, "\n");
			break;
		default:
			fprintf(stdout, "%c", k);
			break;
		}
	} while (k);
  }

  return(OK);
}


/*
 * d u m p _ o s y m
 *
 * Dump object file symbol table data.
 *
 * Returns:	OK	Success
 *		FAILED	Invalid arguments
 *
 * The o_hdrbuf structure is read to determine section addresses.
 * The o_strtab and o_secnam structures are read to determine symbol values.
 */
int dump_osym(FILE *fp, long start, long count)
{
  char buff[BUFF_LEN], data[S_BUFF_LEN];
  int j;
  unsigned int uj;
  long lj;
  struct outname nambuf;

  if (start < 0 || (start + count) > o_hdrbuf.oh_nname)
	return(FAILED);

  for (j = 0 ; j < count ; j++) {
	(void) getstruct(fp, (char *)&nambuf, SF_NAME);
	sprintf(buff, "%4.4d", j + start);
#if 1
	lj = nambuf.on_foff - (long)OFF_CHAR(o_hdrbuf);
	/* check that addressing isn't messed up */
	assert(lj >= 0 && lj < o_hdrbuf.oh_nchar);
	/* name size is defined by SZ_NAME */
	sprintf(data, " %-13s", o_strtab + lj);
	strcat(buff, data);
#else
	sprintf(data, " 0x%08.8x", nambuf.on_foff);
	strcat(buff, data);
#endif
	strcat(buff, " Type:");
	uj = nambuf.on_type & S_TYP;
	if (uj >= S_MIN && uj <= S_MAX) {
#if 1
		/* use arbitrary names for Minix 386 */
		sprintf(data, " %-5s", o_secnam[uj - S_MIN]);
#else
		sprintf(data, "  0x%02.2x", uj - S_MIN);
#endif
		strcat(buff, data);
	}
	/* S_UND is the empty S_TYP field */
	if ((nambuf.on_type & S_TYP) == S_UND) strcat(buff, " S_UND");
	if ((nambuf.on_type & S_TYP) == S_ABS) strcat(buff, " S_ABS");
	if ((nambuf.on_type & S_TYP) == S_CRS) strcat(buff, " S_CRS");

	if ((nambuf.on_type & S_EXT) == S_EXT) strcat(buff, " S_EXT");
	else strcat(buff, "      ");

	if ((nambuf.on_type & S_ETC) == S_SCT) strcat(buff, " S_SCT");
	if ((nambuf.on_type & S_ETC) == S_LIN) strcat(buff, " S_LIN");
	if ((nambuf.on_type & S_ETC) == S_FIL) strcat(buff, " S_FIL");
	if ((nambuf.on_type & S_ETC) == S_MOD) strcat(buff, " S_MOD");
	if ((nambuf.on_type & S_ETC) == S_COM) strcat(buff, " S_COM");
	if ((nambuf.on_type & S_ETC) == 0) strcat(buff, "      ");

	if (nambuf.on_type &
		~(S_TYP | S_EXT | S_SCT | S_LIN | S_FIL | S_MOD | S_COM))
		strcat(buff, " UNKNOWN");

#if 1
	/* Desc is not used, so save space */
	strcat(buff, " Desc: 0x00");
#else
	strcat(buff, " Desc:");
	sprintf(data, " 0x%04.4x", nambuf.on_desc);
	strcat(buff, data);
#endif
	strcat(buff, " Valu:");
	sprintf(data, " 0x%08.8x", nambuf.on_valu);
	strcat(buff, data);
	fprintf(stdout, "%s\n", buff);
  }
#if 0
  fprintf(stdout, "Name Type Debug Value\n");
  fprintf(stdout, "0x%08.8x 0x%04.4x 0x%04.4x 0x%08.8x\n",
		nambuf.on_u.on_off, nambuf.on_type,
		nambuf.on_desc, nambuf.on_valu);
#endif

  return(OK);
}


/*
 * f i n d _ o s y m
 *
 * Find an object file symbol name in a unsorted list.
 *
 * Returns:	index	Found
 *		-1	Not found
 *
 * There may be several symbols with the same value:
 * return each of them on successive calls.
 *
 */
int find_osym(long value, int sec)
{
  static int index = 0;
  static long oldval = 0;
  static int oldsec = 0;
  int j;

  /* check for a repeated search */
  if (value != oldval || sec != oldsec) {
	oldval = value;
	oldsec = sec;
	index = 0;
  }
  /* never happen */
  else if (index == -1)
	return(FAILED);

  /* do a linear search for a symbol, as the symbol table is unsorted */
  for (j = index ; j < o_hdrbuf.oh_nname ; j++) {
	if (value == o_symtab[j].on_valu &&
			sec == ((o_symtab[j].on_type & S_TYP) - S_MIN))
		break;
  }

  /* set up the index for the next pass */
  if (j == o_hdrbuf.oh_nname)
	index = 0;
  else
	index = j + 1;

 return(index - 1);
}


/*
 * g e n _ l o c s y m
 *
 * Generate local symbols.
 *
 * Returns:	OK	Success
 *		FAILED	Invalid arguments, malloc failure
 *
 * This works only for .data, .rom and .bss.  Text symbols need
 * a disassembly of the text section, and intelligent guesses as
 * to whether a local address refers to text or data.  In fact,
 * this routine can be usefully applied only to the .rom area.
 */
int gen_locsym(FILE *fp, int sec)
{
  char data[20];
  int j, txtflg, hdrflg;
  long int addrcount;
  struct locname *np, *current;

  /* check that all offsets are valid - this routine won't work for text */
  if (sec < ROM || sec > BSS) {
	fprintf(stderr, "Invalid section %s\n", o_secnam[sec]);
	return(FAILED);
  }

  /* initialise the label string */
  strncpy(data, o_secnam[sec], 4);
  data[4] = '\0';

  /* initialise the in-memory local name table pointers */
  current = (struct locname *)(NULL);

  /* read the data area and load the symbols */
  (void) fseek(fp, o_sectab[sec].os_foff, SEEK_SET);
  addrcount = 0;
  txtflg = hdrflg = FALSE;
  while (addrcount < o_sectab[sec].os_flen) {
	j = fgetc(fp);
	if (j < 040 || j > 0177) {
		txtflg = FALSE;
		hdrflg = FALSE;
	}
	else
		txtflg = TRUE;

	/* ensure that the start of each apparent string has a related symbol */
	if (txtflg && !hdrflg) {
		if (find_osym(addrcount, sec) == -1) {
			/* if malloc fails, just collapse */
			if ((np = (struct locname *)malloc(sizeof(struct locname)))
					== (struct locname *)NULL) {
				fprintf(stderr, "%s: malloc failed\n", progname);
				return(FAILED);
			}
			/* update the current record */
			sprintf(np->l_name, "%s%04x", data, addrcount & 0xffff);
			/* nb. must follow l_name update */
			if (sec == TEXT) np->l_sclass = S_TEXT & 0xff;
			else if (sec == ROM) np->l_sclass = S_DATA & 0xff;
			else if (sec == DATA) np->l_sclass = S_DATA & 0xff;
			else if (sec == BSS) np->l_sclass = S_BSS & 0xff;
			else sec = 0;
			np->l_value = addrcount;
			np->l_next = (struct locname *)NULL;
			/* and add it to the list */
			if (current == (struct locname *)NULL)
				locsym[sec] = np;
			else
				current->l_next = np;
			current = np;
		}
		hdrflg = TRUE;
	}
	addrcount++;
  }

  return(OK);
}



/*
 * g e t s t r u c t
 *
 * Returns:	0	Always
 *
 * Get 1, 2 and 4 byte values from the input file.
 *
 * Note that the bytes must be reordered and the
 * read pointer incremented correctly for each value;
 * hence the need for a structure format string.
 *
 * Called with:
 * a file destcriptor
 * a pointer to the output buffer
 * a structure format string
 */
int getstruct(FILE *fp, char *bp, char *s)
{
  int j;
  long lj;

  while (TRUE) {
	switch (*s++) {
#if 0
	/* not used */
	case '0':
		bp++;
		continue;
#endif
	case '1':
		*bp++ = (char) getc(fp);
		continue;
	case '2':
		j = getc(fp);
		j |= (getc(fp) << 8);
		*((short *)bp) = (short) j;
		bp += 2;
		continue;
	case '4':
		lj = (long)getc(fp);
		lj |= ((long)getc(fp) << 8);
		lj |= ((long)getc(fp) << 16);
		lj |= ((long)getc(fp) << 24);
		*((long *)bp) = lj;
		bp += 4;
		continue;
	default:
		break;
	}
	break;
  }

  return(0);
}


/*
 * i n i t _ o b j f
 *
 * Initialise object file tables.
 *
 * Returns:	OK	Success
 *		FAILED	Otherwise
 */
int init_objf(FILE *fp)
{
  char *cp;
  int j;
  unsigned int uj;
  long int lj;

  /* load the header into memory for fast access */
  (void) getstruct(fp, (char *)&o_hdrbuf, SF_HEAD);
  if (BADMAGIC(o_hdrbuf)) {
	fprintf(stderr, "%s: bad magic number.\n", progname);
	return(FAILED);
  }
  if (o_hdrbuf.oh_nsect == 0) {
	fprintf(stderr, "%s: no sections present.\n", progname);
	return(FAILED);
  }

  /* check that the whole file can be read */
  if (fseek(fp, OFF_CHAR(o_hdrbuf) + o_hdrbuf.oh_nchar, SEEK_SET) != 0) {
	fprintf(stderr, "%s: cannot seek to end of file.\n", progname);
	return(FAILED);
  }

  /* load the section data into memory for fast access */
  uj = o_hdrbuf.oh_nsect * sizeof(struct outsect);
  if (fseek(fp, OFF_SECT(o_hdrbuf), SEEK_SET) != 0) {
	fprintf(stderr, "%s: cannot seek to section area.\n", progname);
	return(FAILED);
  }
  if (fread(o_sectab, sizeof(char), uj, fp) != uj) {
	fprintf(stderr, "%s: cannot read section area.\n", progname);
	return(FAILED);
  }

  /* load the relocation data into memory for fast access */
  /* ### Should this be left on disk and only the indices loaded ? */
  uj = o_hdrbuf.oh_nrelo * sizeof(struct outrelo);
  if (fseek(fp, OFF_RELO(o_hdrbuf), SEEK_SET) != 0) {
	fprintf(stderr, "%s: cannot seek to relocation area.\n", progname);
	return(FAILED);
  }
  if ((cp = (char *)malloc(uj)) == (char *)NULL) {
	fprintf(stderr, "%s: malloc failed\n", progname);
	return(FAILED);
  }
  if (fread(cp, sizeof(char), uj, fp) != uj) {
	fprintf(stderr, "%s: cannot read relocation area.\n", progname);
	return(FAILED);
  }
  /* initialise the in-memory relocation table array pointers */
  o_reltab = (struct outrelo *)cp;

  /* ### needs to be optional for files without symbol tables */
  /* load the symbol table into memory for fast access */
  uj = o_hdrbuf.oh_nname * sizeof(struct outname);
  if ((cp = (char *)malloc(uj)) == (char *)NULL) {
	fprintf(stderr, "%s: malloc failed\n", progname);
	return(FAILED);
  }
  if (fseek(fp, OFF_NAME(o_hdrbuf), SEEK_SET) != 0) {
	fprintf(stderr, "%s: cannot seek to symbol area.\n", progname);
	return(FAILED);
  }
  if (fread(cp, sizeof(char), uj, fp) != uj) {
	fprintf(stderr, "%s: cannot read symbol area.\n", progname);
	return(FAILED);
  }
  /* initialise the in-memory symbol table array pointers */
  o_symtab = (struct outname *)cp;

  /* load the string area into memory for fast access */
  uj = (unsigned int)o_hdrbuf.oh_nchar;
  if ((o_strtab = (char *)malloc(uj)) == (char *)NULL) {
	fprintf(stderr, "%s: malloc failed\n", progname);
	return(FAILED);
  }
  if (fseek(fp, OFF_CHAR(o_hdrbuf), SEEK_SET) != 0) {
	fprintf(stderr, "%s: cannot seek to string area.\n", progname);
	return(FAILED);
  }
  if (fread(o_strtab, sizeof(char), uj, fp) != uj) {
	fprintf(stderr, "%s: cannot read string area.\n", progname);
	return(FAILED);
  }

  /* build the section name table */
  for (j = 0 ; j < o_hdrbuf.oh_nname ; j++) {
	if ((o_symtab[j].on_type & S_ETC) == S_SCT) {
		lj = o_symtab[j].on_foff - (long)OFF_CHAR(o_hdrbuf);
		/* check that addressing isn't messed up */
		assert(lj >= 0 && lj < o_hdrbuf.oh_nchar);
		strncpy(o_secnam[(o_symtab[j].on_type & S_TYP) - S_MIN],
				o_strtab + lj, SZ_NAME + 1);
	}
  }

  /* build the local symbol tables */
  for (j = 0 ; j < MAXSECT ; j++)
	locsym[j] = (struct locname *)NULL;

  /* build the local .text symbol table */
  /* ### full disassembly ? */

  /* build the local .rom symbol table */
  if (gen_locsym(fp, ROM) == FAILED)
	return(FAILED);

  /* there's no point in building the .data and .bss tables */

  return(OK);
}


/*
 * m a i n
 *
 * Main routine of dis_o386.
 */
int main(int argc, char *argv[])
{
  char *cp, objfile[BUFF_LEN], symbfile[BUFF_LEN];
  char table[MAXSECT*(SZ_NAME+2)];
  int j, errors;
  unsigned long int addrfirst, addrlast, addrcount;
  struct stat statbuff;

  /* initial set up */
  if ((cp = strrchr(argv[0], PSEP)) == (char *)NULL)
	cp = argv[0];
  else
	cp++;
  strncpy(progname, cp, BUFF_MAX);
  strncpy(objfile, OBJF, BUFF_MAX);
  addrfirst = addrlast = addrcount = 0;

  /* clear the in-core name tables */
  o_strtab = (char *)NULL;
  for (j = 0 ; j < MAXSECT ; j++)
	o_secnam[j] = table + j * (SZ_NAME + 2);	/* nb. leading '_' */
  for (j = 0 ; j < sizeof(table) ; j++) table[j] = '\0';

  /* check for an MSDOS-style option */
  if (argc == 2 && argv[1][0] == '/') {
	usage();
	exit(0);
  }

  /* parse arguments */
  errors = opterr = 0;
  while ((j = getopt(argc, argv, "O:S:abdf:hl:mnrstx:")) != EOF) {
	switch (j & 0177) {
#if 0
	case 'C':			/* core file name */
		opt_C = TRUE;
		if (optarg != (char *)NULL)
			strncpy(binfile, optarg, BUFF_MAX);
		else
			errors++;
		break;
	case 'E':			/* executable file name */
		opt_E = TRUE;
		if (optarg != (char *)NULL)
			strncpy(binfile, optarg, BUFF_MAX);
		else
			errors++;
		break;
#endif
	case 'O':			/* object file name */
		opt_O = TRUE;
		if (optarg != (char *)NULL)
			strncpy(objfile, optarg, BUFF_MAX);
		else
			errors++;
		break;
	case 'S':			/* symbol table name */
		opt_S = TRUE;
		if (optarg != (char *)NULL)
			strncpy(symbfile, optarg, BUFF_MAX);
		else
			errors++;
		break;
	case 'a':			/* dump tables and disassemble segments */
		opt_a = TRUE;
		break;
	case 'b':			/* dump straight binary */
		opt_b = TRUE;
		break;
	case 'd':			/* dump the data segment */
		opt_d = TRUE;
		break;
	case 'f':			/* first address of dump */
		opt_f = TRUE;
		if (optarg != (char *)NULL) {
			addrfirst = atoaddr(optarg);
		}
		else
			errors++;
		break;
	case 'h':			/* dump the header */
		opt_h = TRUE;
		break;
	case 'l':			/* last address of dump */
		opt_l = TRUE;
		if (optarg != (char *)NULL) {
			addrlast = atoaddr(optarg);
		}
		else
			errors++;
		break;
	case 'm':			/* dump the rom segment */
		opt_m = TRUE;
		break;
	case 'n':			/* dump the symbol names */
		opt_n = TRUE;
		break;
	case 'r':			/* dump the relocation structures */
		opt_r = TRUE;
		break;
	case 's':			/* dump the symbol table */
		opt_s = TRUE;
		break;
	case 't':			/* dump the text segment */
		opt_t = TRUE;
		break;
#if 0
	case 'u':			/* dump the bss segment */
		opt_u = TRUE;
		break;
#endif
	case 'x':			/* debugging flag */
		opt_x = TRUE;
		if (optarg != (char *)NULL)
			dbglvl = atoi(optarg);
		break;
	case '?':
	default:
		usage();
		exit(1);
		break;
	}
  }

  /* check the flags */
  if (errors > 0) {
	usage();
	exit(1);
  }
  if (opt_a && (opt_d || opt_h || opt_m || opt_n ||
		opt_r || opt_s || opt_t)) {
	usage();
	exit(1);
  }
  if ((opt_f || opt_l) && (addrlast > 0  && addrfirst > addrlast)) {
	usage();
	exit(1);
  }

  /* check for a specific input file */
  if (optind < argc)
	strncpy(objfile, argv[optind], BUFF_MAX);

  /* we must have a binary file of some sort */
  if ((objfp = fopen(objfile, "rb")) == (FILE *)NULL ||
		stat(objfile, &statbuff) == -1) {
	perror(objfile);
	exit(1);
  }

  /* initialise the object file data structures */
  if (init_objf(objfp) == FAILED) {
	perror(objfile);
	exit(1);
  }

  /* show the output file name and date */
  fprintf(stdout, "File name: %s\nFile date: %s",
		objfile, ctime(&statbuff.st_ctime));

  /* show the header and section data - default behaviour */
  if (opt_a || opt_h || (!opt_d && !opt_m && !opt_n &&
		!opt_r && !opt_s && !opt_t)) {
	fprintf(stdout, "\nHeader data:\n");
	(void) dump_ohdr(&o_hdrbuf);
	fprintf(stdout, "\nSection data:\n");
	(void) fseek(objfp, OFF_SECT(hdrbuf), SEEK_SET);
	(void) dump_oshdr(objfp, 0, o_hdrbuf.oh_nsect);
  }

  /* The core start address is zero for every section.  What allowances
   * should be made for the differences between file and core images?
   */

  /* dump or disassemble the rom section */
  if (opt_a || opt_m) {
	if (opt_b)
		(void) dump_osec(addrfirst, addrlast, ROM, FALSE);
	else
		(void) dump_osec(addrfirst, addrlast, ROM, TRUE);
  }

  /* dump or disassemble the data section */
  if (opt_a || opt_d) {
	if (opt_b)
		(void) dump_osec(addrfirst, addrlast, DATA, FALSE);
	else
		(void) dump_osec(addrfirst, addrlast, DATA, TRUE);
  }

  /* dump or disassemble the text section */
  if (opt_a || opt_t) {
	/* check that all offsets are valid */
	if (addrfirst > o_sectab[TEXT].os_flen || addrlast > o_sectab[TEXT].os_flen) {
		fprintf(stderr, "Invalid %s address range 0x%08.8lu to 0x%08.8lu\n",
						"text", addrfirst, addrlast);
	}
	else {
		if (opt_b)
			(void) dump_osec(addrfirst, addrlast, TEXT, FALSE);
		else {
			addrcount = (addrlast == 0) ? o_sectab[TEXT].os_flen : addrlast;
			addrcount -= addrfirst;
			disfp = objfp;			/* file to be disassembled */
			(void) fseek(disfp, o_sectab[TEXT].os_foff + addrfirst, SEEK_SET);
			fprintf(stdout, "\nDisassembled text:\n");
			(void) dasm(addrfirst, addrcount);
		}
	}
  }

  /* show the relocation data */
  if (opt_a || opt_r) {
	if (opt_b)
		addrcount = o_hdrbuf.oh_nrelo * sizeof(struct outrelo);
	else
		addrcount = o_hdrbuf.oh_nrelo;
	/* check that all offsets are valid */
	if (addrfirst >= addrcount || addrlast >= addrcount) {
		fprintf(stderr, "Invalid %s address range 0x%08.8lu to 0x%08.8lu\n",
				"relocation", addrfirst, addrlast);
	}
	else {
		if (opt_l)
			addrcount = addrlast + 1;
		addrcount = addrcount - addrfirst;
		if (opt_b) {
			fprintf(stdout, "\nRelocation data dump:\n");
			(void) fseek(objfp, OFF_RELO(o_hdrbuf) + addrfirst, SEEK_SET);
			(void) dump_hex(objfp, addrfirst, addrcount);
		}
		else {
			fprintf(stdout, "\nRelocation data:\n");
			(void) fseek(objfp, OFF_RELO(o_hdrbuf) + addrfirst *
					sizeof(struct outrelo), SEEK_SET);
			(void) dump_orel(objfp, addrfirst, addrcount);
		}
	}
  }

  /* show the symbol data */
  if (opt_a || opt_s) {
	if (opt_b)
		addrcount = o_hdrbuf.oh_nname * sizeof(struct outname);
	else
		addrcount = o_hdrbuf.oh_nname;
	/* check that all offsets are valid */
	if (addrfirst >= addrcount || addrlast >= addrcount) {
		fprintf(stderr, "Invalid %s address range 0x%08.8lu to 0x%08.8lu\n",
				"symbol", addrfirst, addrlast);
	}
	else {
		if (opt_l)
			addrcount = addrlast + 1;
		addrcount = addrcount - addrfirst;
		if (opt_b) {
			fprintf(stdout, "\nSymbol data dump:\n");
			(void) fseek(objfp, OFF_NAME(o_hdrbuf) + addrfirst, SEEK_SET);
			(void) dump_hex(objfp, addrfirst, addrcount);
		}
		else {
			fprintf(stdout, "\nSymbol data:\n");
			(void) fseek(objfp, OFF_NAME(o_hdrbuf) + addrfirst *
					sizeof(struct outname), SEEK_SET);
			(void) dump_osym(objfp, addrfirst, addrcount);
		}
	}
  }

  /* show the string data */
  if (opt_a || opt_n) {
	if (opt_b)
		addrcount = o_hdrbuf.oh_nchar;
	else
		addrcount = o_hdrbuf.oh_nname;	/* assumes one name per symbol */
	/* check that all offsets are valid */
	if (addrfirst >= addrcount || addrlast >= addrcount) {
		fprintf(stderr, "Invalid %s address range 0x%08.8lu to 0x%08.8lu\n",
				"symbol", addrfirst, addrlast);
	}
	else {
		if (opt_l)
			addrcount = addrlast + 1;
		addrcount = addrcount - addrfirst;
		if (opt_b) {
			fprintf(stdout, "\nName data dump:\n");
			(void) fseek(objfp, OFF_CHAR(o_hdrbuf) + addrfirst, SEEK_SET);
			(void) dump_hex(objfp, addrfirst, addrcount);
		}
		else {
			fprintf(stdout, "\nName data:\n");
			(void) fseek(objfp, o_symtab[addrfirst].on_foff, SEEK_SET);
			(void) dump_ostr(objfp, addrfirst, addrcount);
		}
	}
  }

  /* wrap up */
  fclose(objfp);

  exit(0);
}


/*
 * u s a g e
 *
 * Usage message.
 *
 * Returns:	Nothing		Always
 */
void usage()
{
  fprintf(stderr, "Usage: %s [-a|-dhmnrst] [-b] [-f #] [-l #] [-O objfile]\n",
		progname);
}


/*
 * EOF
 */

