/* nm - print name list.		Author: Dick van Veen */

/* Dick van Veen: veench@cs.vu.nl */

#include <sys/types.h>
#include <a.out.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Read the name list in memory, sort it, and print it.  */

/* Nm [-gnopru] [file] ...
 *
 * flags:
 *	-d	address in decimal
 *	-g	print only external symbols.
 *	-n	sort numerically rather than alphabetically.
 *	-o	prepend file name to each line rather than only once.
 *	-p	don't sort, pint n symbol-table order.
 *	-r	sort in reverse order.
 *	-u	print only undefined symbols.
 *
 *	-	when no file name is present, a.out is assumed.
 *
 *	NOTE:	no archives are supported because assembly files don't
 *		have symbol tables.
 *
 */

#define A_OUT		"a.out"

int d_flag;
int g_flag;
int n_flag;
int o_flag;
int p_flag;
int r_flag;
int u_flag;

char io_buf[BUFSIZ];		/* io buffer */
struct exec header;		/* header of a.out file */
int stbl_elems;			/* #elements in symbol table */

int main(int argc, char **argv);
int nm_sort(const void *tmp_stbl1, const void *tmp_stbl2);
void nm(char *file);
int read_header(int fd);
void nm_print(char *file, struct nlist *stbl);

int main(argc, argv)
int argc;
char **argv;
{
  argv++;
  while (*argv != 0 && **argv == '-') {
	*argv += 1;
	while (**argv != '\0') {
		switch (**argv) {
		    case 'd':	d_flag = 1;	break;
		    case 'g':	g_flag = 1;	break;
		    case 'n':	n_flag = 1;	break;
		    case 'o':	o_flag = 1;	break;
		    case 'p':	p_flag = 1;	break;
		    case 'r':	r_flag = 1;	break;
		    case 'u':	u_flag = 1;	break;
		    default:
			fprintf(stderr, "illegal flag: -%c\n", **argv);
			exit(-1);
		}
		*argv += 1;
	}
	argv++;
  }
  setbuf(stdin, io_buf);
  if (*argv == 0)
	nm(A_OUT);
  else
	while (*argv != 0) {
		nm(*argv);
		argv++;
	}
  return(0);
}

int nm_sort(tmp_stbl1, tmp_stbl2)
const void *tmp_stbl1, *tmp_stbl2;
{

  struct nlist *stbl1 = (struct nlist *)tmp_stbl1;
  struct nlist *stbl2 = (struct nlist *)tmp_stbl2;
  int cmp;

  if (n_flag) {			/* sort numerically */
	if ((stbl1->n_sclass & N_SECT) <
	    (stbl2->n_sclass & N_SECT))
		cmp = -1;
	else if ((stbl1->n_sclass & N_SECT) >
		 (stbl2->n_sclass & N_SECT))
		cmp = 1;
	else if (stbl1->n_value < stbl2->n_value)
		cmp = -1;
	else if (stbl1->n_value > stbl2->n_value)
		cmp = 1;
	else
		cmp = strncmp(stbl1->n_name, stbl2->n_name, (size_t)8);
  } else {
	cmp = strncmp(stbl1->n_name, stbl2->n_name, (size_t)8);
	if (cmp == 0) {
		if (stbl1->n_value < stbl2->n_value)
			cmp = -1;
		else if (stbl1->n_value > stbl2->n_value)
			cmp = 1;
	}
  }

  if (r_flag) cmp = -cmp;	/* reverse sort */
  return(cmp);
}

void nm(file)
char *file;
{
  struct nlist *stbl;
  int fd;

  fd = open(file, O_RDONLY);
  if (fd == -1) {
	fprintf(stderr, "can't open %s\n", file);
	return;
  }
  if (read_header(fd)) {
	fprintf(stderr, "%s: no executable file\n", file);
	close(fd);
	return;
  }
  if (header.a_syms == 0) {
	close(fd);
	return;
  }
  if ((size_t) header.a_syms != header.a_syms) {
	fprintf(stderr, "%s: symbol table too large to allocate\n", file);
	close(fd);
	return;
  }
  if ((int) header.a_syms != header.a_syms) {
	/* This is necessary because we are too lazy to iterate the read. */
	fprintf(stderr, "%s: symbol table too large to read\n", file);
	close(fd);
	return;
  }
  stbl = (struct nlist *) malloc((size_t) header.a_syms);
  if (stbl == NULL) {
	fprintf(stderr, "%s: can't allocate symbol table\n", file);
	close(fd);
	return;
  }
  if (read(fd, (char *) stbl, (unsigned) header.a_syms) != header.a_syms) {
	fprintf(stderr, "%s: can't read symbol table\n", file);
	free(stbl);
	close(fd);
	return;
  }
  stbl_elems = (int) header.a_syms / sizeof(struct nlist);
  if (!p_flag) qsort(stbl, (size_t)stbl_elems, sizeof(struct nlist), nm_sort);
  nm_print(file, stbl);
  free(stbl);
  close(fd);
}

int read_header(fd)
int fd;
{
  if (read(fd, (char *) &header, sizeof(struct exec)) != sizeof(struct exec))
	return(1);
  if (BADMAG(header)) return(1);
  lseek(fd, A_SYMPOS(header), SEEK_SET);

  return(0);
}

void nm_print(file, stbl)
char *file;
register struct nlist *stbl;
{
  struct nlist *last;
  char name[9];
  int n_sclass;
  char type;

  name[8] = '\0';
  if (!o_flag) printf("%s:\n", file);
  for (last = &stbl[stbl_elems]; stbl != last; stbl++) {
	if (g_flag && (stbl->n_sclass & N_CLASS) != C_EXT) continue;
	if (u_flag && (stbl->n_sclass & N_SECT) != N_UNDF) continue;

	n_sclass = stbl->n_sclass & N_SECT;
	if (n_sclass == N_ABS)
		type = 'a';
	else if (n_sclass == N_TEXT)
		type = 't';
	else if (n_sclass == N_DATA)
		type = 'd';
	else if (n_sclass == N_BSS)
		type = 'b';
	else
		type = 'u';
	if ((stbl->n_sclass & N_CLASS) == C_EXT) type += 'A' - 'a';
	strncpy(name, stbl->n_name, (size_t)8);
	if (d_flag) {
		/* Offsets in decimal. */
		if (o_flag) 
		       printf("%s:%08ld %c %s\n",file,stbl->n_value,type,name);
		else
		       printf("%08ld %c %s\n", stbl->n_value, type, name);
	} else {
		/* Offsets in hex. */
		if (o_flag) 
		       printf("%s:%08lx %c %s\n",file,stbl->n_value,type,name);
		else
		       printf("%08lx %c %s\n", stbl->n_value, type, name);
	}
  }
}
