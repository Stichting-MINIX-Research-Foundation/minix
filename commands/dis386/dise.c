/*
 * dis_e386: disassemble 386 executable files.
 *
 * $Id: dise.c,v 1.1 1997/10/20 12:00:00 cwr Exp cwr $
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

#define AOUT		"a.out"
#define CORE		"core"
#define STAB		"symbol.tab"
#define LINE_LEN	16
#define SYMLEN		8

#define TEXT		0	/* section indices for locsym[] */
#define ROM		1
#define DATA		2
#define BSS		3

#ifndef lint
static char *Version = "@(#) dis_e386.c $Revision: 1.1 $ $Date: 1997/10/20 12:00:00 $";
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

struct a_sec {			/* a.out section data */
  char *name;			/* section name */
  int first;			/* first index */
  int last;			/* last index */
  int total;
} a_sectab[] = {		/* all known a.out sections */
  "undefined", 0, 0, 0,
  "absolute", 0, 0, 0,
  "text", 0, 0, 0,
  "data", 0, 0, 0,
  "bss", 0, 0, 0,
  "common", 0, 0, 0,
  "rom", 0, 0, 0,		/* this one is unknown */
  (char *)NULL, 0, 0, 0
};

/* Forward declarations */
unsigned long atoaddr(char *);			/* Convert ascii hex/dec to unsigned long */
int binary(unsigned char, char*);		/* Binary output of 8-bit number */
int dump_ahdr(struct exec *ep);			/* Dump an a.out file header */
int dump_adata(FILE *fp, int start, int count);	/* Dump an a.out file data section */
int dump_asym(struct nlist *np, int start, int count);	/* Dump an a.out file symbol table */
int dump_hex(FILE *fp, int start, int count);	/* Dump bytes in hex and ascii */
int find_asym(long value, int section);		/* Find an a.out symbol */
int gen_locsym(FILE *fp, int sec);		/* Generate local symbols */
int init_aout(FILE *fp);			/* Initialise the a.out file tables */
void usage(void);				/* Usage message */


/*
 * a t o a d d r
 *
 * Convert ascii hex/dec to unsigned long.
 *
 * Returns:	Conversion result	Always
 */
unsigned long atoaddr(char *sp)
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
 * b i n a r y
 *
 * Produce a binary representation of an 8-bit number.
 *
 * Returns:	0	Always
 */
int binary(unsigned char uc, char *sp)
{
  int j;
  unsigned char k;

  for (k = 0x80, j = 0 ; j < 8 ; j++) {
	if ((uc & k) == 0) *sp++ = '0';
	else *sp++ = '1';
	if (j == 3) *sp++ = '$';
	k >>= 1;
  }
  *sp = '\0';

  return(0);
}


/*
 * d u m p _ a h d r
 *
 * Dump an a.out file header.
 *
 * Returns:	OK	Always
 */
int dump_ahdr(struct exec *ep)
{
  char buff[BUFF_LEN];

  fprintf(stdout, "Magic number is:             0x%02x%02x\n",
		ep->a_magic[0], ep->a_magic[1]);
  sprintf(buff, "Flags are:                   0x%02x", ep->a_flags);
  if (ep->a_flags & A_UZP) strcat(buff, " A_UZP");
  if (ep->a_flags & A_PAL) strcat(buff, " A_PAL");
  if (ep->a_flags & A_NSYM) strcat(buff, " A_NSYM");
  if (ep->a_flags & A_EXEC) strcat(buff, " A_EXEC");
  if (ep->a_flags & A_SEP) strcat(buff, " A_SEP");
  if (ep->a_flags & A_PURE) strcat(buff, " A_PURE");
  if (ep->a_flags & A_TOVLY) strcat(buff, " A_TOVLY");
  if (ep->a_flags & ~(A_UZP | A_PAL | A_NSYM | A_EXEC | A_SEP | A_PURE | A_TOVLY))
	strcat(buff, " UNKNOWN");
  fprintf(stdout, "%s\n", buff);

  sprintf(buff, "CPU type is:                 0x%02x", ep->a_cpu);
  if (ep->a_cpu == A_NONE) strcat(buff, " A_NONE");
  else if (ep->a_cpu == A_I8086) strcat(buff, " A_I8086");
  else if (ep->a_cpu == A_M68K) strcat(buff, " A_M68K");
  else if (ep->a_cpu == A_NS16K) strcat(buff, " A_NS16K");
  else if (ep->a_cpu == A_I80386) strcat(buff, " A_I80386");
  else if (ep->a_cpu == A_SPARC) strcat(buff, " A_SPARC");
  else strcat(buff, " UNKNOWN");
  fprintf(stdout, "%s\n", buff);

  fprintf(stdout, "Byte order is:               %s\n",
		A_BLR(ep->a_cpu) ? "left to right" : "right to left");
  fprintf(stdout, "Word order is:               %s\n",
		A_WLR(ep->a_cpu) ? "left to right" : "right to left");

  fprintf(stdout, "Header length is:            0x%02x\n", ep->a_hdrlen);
  fprintf(stdout, "Reserved byte is:            0x%02x\n", ep->a_unused);
  fprintf(stdout, "Version stamp is:            0x%04x\n", ep->a_version);
  fprintf(stdout, "Size of text segment is:     0x%08.8x\n", ep->a_text);
  fprintf(stdout, "Size of data segment is:     0x%08.8x\n", ep->a_data);
  fprintf(stdout, "Size of bss segment is:      0x%08.8x\n", ep->a_bss);
  fprintf(stdout, "Entry point is:              0x%08.8x\n", ep->a_entry);
  fprintf(stdout, "Total memory allocated is:   0x%08.8x\n", ep->a_total);
  fprintf(stdout, "Size of symbol table is:     0x%08.8x bytes, %d entries\n",
		ep->a_syms, ep->a_syms / sizeof(struct nlist));

  /* SHORT FORM ENDS HERE */
#if 0
  fprintf(stdout, "Size of text relocation is 0x%08.8x\n", ep->a_trsize);
  fprintf(stdout, "Size of data relocation is 0x%08.8x\n", ep->a_drsize);
  fprintf(stdout, "Base of text relocation is 0x%08.8x\n", ep->a_tbase);
  fprintf(stdout, "Base of data relocation is 0x%08.8x\n", ep->a_dbase);
#endif

  return(OK);
}


/*
 * d u m p _ a d a t a
 *
 * Dump an a.out data section.
 *
 * Returns:	OK	Success
 *		FAILED	File read failure, invalid arguments
 *
 * The a_hdrbuf structure is read to determine section addresses.
 * The a_symtab structure is read to determine symbol names (if available).
 */
int dump_adata(FILE *fp, int start, int count)
{
  char label[S_BUFF_LEN], data[S_BUFF_LEN], buff[BUFF_LEN];
  char *hex = "0123456789ABCDEF";
  int j, k, newflg, index, last, status, found, quit;
  long int addr;
  unsigned long int ulj;
  struct locname *np;

  if (start < 0 || (start + count) > (A_SYMPOS(a_hdrbuf) - a_hdrbuf.a_hdrlen))
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
		for (np = locsym[DATA] ; !found && np != (struct locname *)NULL ;
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
		while (!found && (index = find_asym(ulj, N_DATA)) != -1) {
			/* for the first symbol, write out any outstanding data */
			if (newflg && j != 0) {
				buff[75] = '\0';
				fprintf(stdout, "%s\n", buff);
				for (k = 8 ; k < 75 ; k++)
					buff[k] = ' ';
				newflg = FALSE;
			}
			/* write out the symbol name */
			for (k = 0 ; k < 8 ; k++)
				label[k] = a_symtab[index].n_name[k];
			label[k] = '\0';
			/* for some reason, some table entries are empty */
			if (label[0] != '\0') fprintf(stdout, "%s\n", label);
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
 * d u m p _ a s y m
 *
 * Dump an a.out file symbol table.
 *
 * Returns:	OK	Success
 *		FAILED	Invalid arguments
 *
 * The a_hdrbuf structure is read to determine section addresses.
 */
int dump_asym(struct nlist *np, int start, int count)
{
  char buff[BUFF_LEN], data[S_BUFF_LEN];
  unsigned char uc;
  int j, k;

  if (start < 0 || (start + count) > (a_hdrbuf.a_syms / sizeof(struct nlist)))
	return(FAILED);

  for (j = start ; j < (start + count) ; j++) {
	sprintf(buff, "%-4d ", j);
	for (k = 0 ; k < SYMLEN ; k++)
		data[k] = (np[j].n_name[k] == '\0') ? ' ' : np[j].n_name[k];
	data[k] = '\0';
	strcat(buff, data);
	sprintf(data, " Val: 0x%08x", np[j].n_value);
	strcat(buff, data);
 	sprintf(data, " Sto: 0x%02x", np[j].n_sclass);
	strcat(buff, data);
	uc = np[j].n_sclass;
	if ((uc & N_SECT) == N_UNDF) strcat(buff, " N_UNDF");
	else if ((uc & N_SECT) == N_ABS) strcat(buff, " N_ABS ");
	else if ((uc & N_SECT) == N_TEXT) strcat(buff, " N_TEXT");
	else if ((uc & N_SECT) == N_DATA) strcat(buff, " N_DATA");
	else if ((uc & N_SECT) == N_BSS) strcat(buff, " N_BSS ");
	else if ((uc & N_SECT) == N_COMM) strcat(buff, " N_COMM");
	else strcat(buff, " UNKNOWN");
	if ((uc & N_CLASS) == 0) strcat(buff, " C_NULL");
	else if ((uc & N_CLASS) == C_EXT) strcat(buff, " C_EXT ");
	else if ((uc & N_CLASS) == C_STAT) strcat(buff, " C_STAT");
	else strcat(buff, " UNKNOWN");
	sprintf(data, " Aux: 0x%02x", np[j].n_numaux);
	strcat(buff, data);
	sprintf(data, " Typ: 0x%04x", np[j].n_type);
	strcat(buff, data);
	fprintf(stdout, "%s\n", buff);
  }

  return(OK);
}


/*
 * d u m p _ h e x
 *
 * Dump bytes in hex and ascii.
 *
 * Returns:	OK	Success
 *		FAILED	File read failure, invalid arguments
 */
int dump_hex(FILE *fp, int start, int count)
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
		(void) fputc(c, stdout);
	}

	fprintf(stdout, "\n");
	if (quit)
		break;
  }

  return(status);
}


/*
 * f i n d _ a s y m
 *
 * Find an a.out symbol index in a sorted list.
 * There may be several symbols with the same value:
 * return the first in the sequence.
 *
 * Returns:	index		Success
 *		-1		Failure
 *
 * The a_sectab structure is read to determine section indices.
 * The a_symtab structure is read to determine symbol names.
 */
int find_asym(long value, int sec)
{
  static int index = 0;
  static long oldval = 0;
  static int oldsec = 0;
  int j;

  /* check for a repeated search */
  if (value != oldval || sec != oldsec) {
	oldval = value;
	oldsec = sec;
	index = a_sectab[sec].first;
  }
  /* never happen */
  else if (index == -1)
	return(FAILED);

  /* do a linear search for a symbol, since repeated searches may be needed */
  for (j = index ; j < a_sectab[sec].last ; j++) {
	if (value == a_symtab[j].n_value)
		break;
  }

  /* set up the index for the next pass */
  if (j == a_sectab[sec].last) {
	index = a_sectab[sec].first;
	return(-1);
  }
  else {
	index = j + 1;
	return(j);
  }
  /* NOTREACHED */
}


/*
 * g e n _ l o c s y m
 *
 * Generate local symbols.
 *
 * Returns:	OK	Success
 *		FAILED	Invalid arguments, malloc failure
 *
 * This works only for data and bss segments.  Text symbols need
 * a disassembly of the text section, and intelligent guesses as
 * to whether a local address refers to text or data.  In fact,
 * this routine is hardwired to the data area, and the bss area
 * is ignored.
 */
int gen_locsym(FILE *fp, int sec)
{
  char data[20];
  int j, txtflg, hdrflg;
  long int addrcount;
  struct locname *np, *current;

  /* check that all offsets are valid - this routine won't work for text */
  if (sec < ROM || sec > BSS) {
	fprintf(stderr, "Invalid section %s\n", a_sectab[sec & 7].name);
	return(FAILED);
  }

  /* initialise the label string */
  strncpy(data, ".DAT", 4);
  data[4] = '\0';

  /* initialise the in-memory local name table pointers */
  current = (struct locname *)(NULL);

  /* read the data area and load the symbols */
  (void) fseek(aoutfp, A_DATAPOS(a_hdrbuf), SEEK_SET);
  addrcount = 0;
  txtflg = hdrflg = FALSE;
  while (addrcount < a_hdrbuf.a_data) {
	j = fgetc(fp);
	if (j < 040 || j > 0177) {
		txtflg = FALSE;
		hdrflg = FALSE;
	}
	else
		txtflg = TRUE;

	/* ensure that the start of each apparent string has a related symbol */
	if (txtflg && !hdrflg) {
		if (find_asym(addrcount, sec) == -1) {
			/* if malloc fails, just collapse */
			if ((np = (struct locname *)malloc(sizeof(struct locname)))
					== (struct locname *)NULL) {
				fprintf(stderr, "%s: malloc failed\n", progname);
				return(FAILED);
			}
			/* update the current record */
			sprintf(np->l_name, "%s%04x", data,
					(a_hdrbuf.a_text + addrcount) & 0xffff);
			/* nb. must follow l_name update */
			if (sec == TEXT) np->l_sclass = S_TEXT & 0xff;
			else if (sec == ROM) np->l_sclass = S_DATA & 0xff;
			else if (sec == DATA) np->l_sclass = S_DATA & 0xff;
			else if (sec == BSS) np->l_sclass = S_BSS & 0xff;
			else sec = 0;
			np->l_value = a_hdrbuf.a_text + addrcount;
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
 * i n i t _ a o u t
 *
 * Initialise the a.out file tables.
 *
 * Returns:	OK	Success
 *		FAILED	File read failure
 *
 * The a_hdrbuf and a_symtab and a_sectab structures are
 * all initialised here.  Also, the ability to read the
 * entire file is checked; no read checking is done
 * later in the program.
 */
int init_aout(FILE *fp)
{
  char *cp;
  int j, k, maxsym;
  struct nlist *np;
  struct nlist ntmp;

  /* load the header into memory for fast access.
   * the header length is the fifth byte of the header.
   */
  cp = (char *)&a_hdrbuf;
  if (fread(cp, sizeof(char), 5, aoutfp) != 5) {
	fprintf(stderr, "Cannot read executable header.\n");
	return(FAILED);
  }
  j = cp[4] - 5;
  cp += 5;
  if (fread(cp, sizeof(char), j, aoutfp) != j) {
	fprintf(stderr, "Cannot read executable header.\n");
	return(FAILED);
  }
  if(BADMAG(a_hdrbuf)) {
	fprintf(stderr, "%s: bad magic number.\n", progname);
	return(FAILED);
  }

  /* check that the whole file can be read */
  if (fseek(aoutfp, A_SYMPOS(a_hdrbuf) + a_hdrbuf.a_syms, SEEK_SET) != 0) {
	fprintf(stderr, "%s: cannot seek to end of file.\n", progname);
	return(FAILED);
  }

  /* load the symbol table into memory for fast access */
  a_symtab = (struct nlist *)NULL;
  if (a_hdrbuf.a_syms != 0) {
	/* get space for the nlist data */
	if ((cp = (char *)malloc(a_hdrbuf.a_syms)) == (char *)NULL) {
		fprintf(stderr, "%s: malloc failed\n", progname);
		return(FAILED);
	}
	if (fseek(aoutfp, -a_hdrbuf.a_syms, SEEK_CUR) != 0) {
		fprintf(stderr, "%s: cannot seek to symbol area.\n", progname);
		return(FAILED);
	}
	/* load the symbols into a sorted list */
	np = (struct nlist *)cp;
	maxsym = 0;
	for (j = 0 ; j < a_hdrbuf.a_syms / sizeof(struct nlist) ; j++) {
		if (fread(&ntmp, sizeof(struct nlist), 1, aoutfp) != 1) {
			fprintf(stderr, "%s: cannot read symbol area.\n", progname);
			return(FAILED);
		}
		/* insertion sort, by class and value */
		for (k = maxsym ; k > 0 ; k--) {
			if ((ntmp.n_sclass & N_SECT) < (np[k-1].n_sclass & N_SECT))
				np[k] = np[k - 1];
			else if ((ntmp.n_sclass & N_SECT) == (np[k-1].n_sclass & N_SECT) &&
					ntmp.n_value < np[k-1].n_value)
				np[k] = np[k - 1];
			else
				break;
		}
		np[k] = ntmp;
		maxsym++;
	}
	/* finally, we have a valid symbol table */
	a_symtab = (struct nlist *)cp;

	/* update the symbol section index list */
	a_sectab[a_symtab[0].n_sclass & N_SECT].first = 0;
	for (j = 1 ; j < (a_hdrbuf.a_syms / sizeof(struct nlist)) ; j++) {
		if ((a_symtab[j].n_sclass & N_SECT) != (a_symtab[j-1].n_sclass & N_SECT)) {
			a_sectab[a_symtab[j-1].n_sclass & N_SECT].last = j - 1;
			a_sectab[a_symtab[j-1].n_sclass & N_SECT].total =
				j - a_sectab[a_symtab[j-1].n_sclass & N_SECT].first;
			a_sectab[a_symtab[j].n_sclass & N_SECT].first = j;
		}
	}
	a_sectab[a_symtab[j-1].n_sclass & N_SECT].last = j - 1;

	/* build the local symbol tables */
	for (j = 0 ; j < MAXSECT ; j++)
		locsym[j] = (struct locname *)NULL;

	/* build the local .text symbol table */
	/* ### full disassembly ? */

	/* build the local data symbol table */
	if (gen_locsym(fp, DATA) == FAILED)
		return(FAILED);
  }

  return(OK);
}


/*
 * m a i n
 *
 * Main routine of dis_a386.
 */
int main(int argc, char *argv[])
{
  char *cp, binfile[BUFF_LEN], symbfile[BUFF_LEN];
  int j, errors;
  unsigned long int addrfirst, addrlast, addrcount;
  struct stat statbuff;

  /* initial set up */
  if ((cp = strrchr(argv[0], PSEP)) == (char *)NULL)
	cp = argv[0];
  else
	cp++;
  strncpy(progname, cp, BUFF_MAX);
  strncpy(binfile, AOUT, BUFF_MAX);
  addrfirst = addrlast = addrcount = 0;

  /* check for an MSDOS-style option */
  if (argc == 2 && argv[1][0] == '/') {
	usage();
	exit(0);
  }

  /* parse arguments */
  errors = opterr = 0;
  while ((j = getopt(argc, argv, "E:abdf:hl:stx:")) != EOF) {
	switch (j & 0177) {
#if 0
	case 'C':			/* core file name */
		opt_C = TRUE;
		if (optarg != (char *)NULL)
			strncpy(binfile, optarg, BUFF_MAX);
		else
			errors++;
		break;
#endif
	case 'E':			/* executable file name */
		opt_E = TRUE;
		if (optarg != (char *)NULL)
			strncpy(binfile, optarg, BUFF_MAX);
		else
			errors++;
		break;
#if 0
	case 'O':			/* object file name */
		opt_O = TRUE;
		if (optarg != (char *)NULL)
			strncpy(binfile, optarg, BUFF_MAX);
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
#endif
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
		if (optarg != (char *)NULL)
			addrfirst = atoaddr(optarg);
		else
			errors++;
		break;
	case 'h':			/* dump the header */
		opt_h = TRUE;
		break;
	case 'l':			/* last address of dump */
		opt_l = TRUE;
		if (optarg != (char *)NULL)
			addrlast = atoaddr(optarg);
		else
			errors++;
		break;
#if 0
	case 'm':			/* dump the rom segment */
		opt_m = TRUE;
		break;
	case 'n':			/* dump the symbol names */
		opt_n = TRUE;
		break;
	case 'r':			/* dump the relocation structures */
		opt_r = TRUE;
		break;
#endif
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
  if (opt_a && (opt_d || opt_h || opt_s || opt_t)) {
	usage();
	exit(1);
  }
  if ((opt_f || opt_l) && (addrlast != 0 && addrfirst > addrlast)) {
	usage();
	exit(1);
  }

  /* check for a specific input file */
  if (optind < argc)
	strncpy(binfile, argv[optind], BUFF_MAX);

  /* we must have a binary file of some sort */
  if ((aoutfp = fopen(binfile, "rb")) == (FILE *)NULL ||
		stat(binfile, &statbuff) == -1) {
	perror(binfile);
	exit(1);
  }

  /* initialise the a.out data structures */
  if (init_aout(aoutfp) == FAILED) {
	perror(binfile);
	exit(1);
  }

  /* show the output file name and date */
  fprintf(stdout, "File name: %s\nFile date: %s",
		binfile, ctime(&statbuff.st_ctime));

  /* show the header section - default behaviour */
  if (opt_a || opt_h || (!opt_d && !opt_s && !opt_t)) {
	fprintf(stdout, "\nHeader data:\n");
	(void) dump_ahdr(&a_hdrbuf);
  }

  /* dump the data section */
  if (opt_d && opt_b) {
	/* check that all offsets are valid */
	if (addrfirst > a_hdrbuf.a_data || addrlast > a_hdrbuf.a_data) {
		fprintf(stderr, "Invalid data address range 0x%08.8lu to 0x%08.8lu\n",
								addrfirst, addrlast);
	}
	else {
		addrcount = (addrlast == 0) ? a_hdrbuf.a_data : addrlast;
		addrcount -= addrfirst;
		(void) fseek(aoutfp, A_DATAPOS(a_hdrbuf) + addrfirst, SEEK_SET);
		fprintf(stdout, "\nData:\n");
		(void) dump_hex(aoutfp, A_DATAPOS(a_hdrbuf) - a_hdrbuf.a_hdrlen + addrfirst,
								addrcount);
	}
  }

  /* disassemble the data section */
  if (opt_a || (opt_d && !opt_b)) {
	/* check that all offsets are valid */
	if (addrfirst > a_hdrbuf.a_data || addrlast > a_hdrbuf.a_data) {
		fprintf(stderr, "Invalid data address range 0x%08.8lu to 0x%08.8lu\n",
								addrfirst, addrlast);
	}
	else {
		addrcount = (addrlast == 0) ? a_hdrbuf.a_data : addrlast;
		addrcount -= addrfirst;
		(void) fseek(aoutfp, A_DATAPOS(a_hdrbuf) + addrfirst, SEEK_SET);
		fprintf(stdout, "\nDisassembled data:\n");
		(void) dump_adata(aoutfp, A_DATAPOS(a_hdrbuf) - a_hdrbuf.a_hdrlen
							+ addrfirst, addrcount);
	}
  }

  /* dump the text section */
  if (opt_t && opt_b) {
	/* check that all offsets are valid */
	if (addrfirst > a_hdrbuf.a_text || addrlast > a_hdrbuf.a_text) {
		fprintf(stderr, "Invalid text address range 0x%08.8lu to 0x%08.8lu\n",
								addrfirst, addrlast);
	}
	else {
		addrcount = (addrlast == 0) ? a_hdrbuf.a_text : addrlast;
		addrcount -= addrfirst;
		(void) fseek(aoutfp, A_TEXTPOS(a_hdrbuf) + addrfirst, SEEK_SET);
		fprintf(stdout, "\nText:\n");
		(void) dump_hex(aoutfp, A_TEXTPOS(a_hdrbuf) - a_hdrbuf.a_hdrlen
							+ addrfirst, addrcount);
	}
  }

  /* disassemble the text section */
  if (opt_a || (opt_t && !opt_b)) {
	/* check that all offsets are valid */
	if (addrfirst > a_hdrbuf.a_text || addrlast > a_hdrbuf.a_text) {
		fprintf(stderr, "Invalid text address range 0x%08.8lu to 0x%08.8lu\n",
								addrfirst, addrlast);
	}
	else {
		addrcount = (addrlast == 0) ? a_hdrbuf.a_text : addrlast;
		addrcount -= addrfirst;
		disfp = aoutfp;			/* file to be disassembled */
		objfp = (FILE *)NULL;		/* without relocation information */
		(void) fseek(disfp, A_TEXTPOS(a_hdrbuf) + addrfirst, SEEK_SET);
		fprintf(stdout, "\nDisassembled text:\n");
		(void) dasm(addrfirst, addrcount);
	}
  }

  /* show the symbol data */
  if (opt_a || opt_s) {
	fprintf(stdout, "\nSymbol data:\n");
	if (a_hdrbuf.a_syms == 0)
		fprintf(stdout, "No symbol table available.\n");
	else
		(void) dump_asym(a_symtab, 0, a_hdrbuf.a_syms / sizeof(struct nlist));
  }

  /* wrap up */
  (void) fclose(aoutfp);

  exit(0);
  /* NOTREACHED */
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
  fprintf(stderr, "Usage: %s [-a|-dhst] [-b] [-f #] [-l #] [-E executable]\n",
		progname);
}

/*
 * EOF
 */

