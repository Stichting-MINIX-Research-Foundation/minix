/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/*
**	print symbol tables for
**	ACK object files
**
**	anm [-gopruns] [name ...]
*/

#include	"out.h"
#include	"arch.h"
#include	"ranlib.h"

#include	<stdio.h>
#include	<ctype.h>

int	numsort_flg;
int	sectsort_flg;
int	undef_flg;
int	revsort_flg = 1;
int	globl_flg;
int	nosort_flg;
int	arch_flg;
int	prep_flg;
int	read_error;
struct	outhead	hbuf;
struct	outsect	sbuf;
long	off;
char	*malloc();
char	*realloc();
long	s_base[S_MAX];	/* for specially encoded bases */
char	*filename;
int	narg;

main(argc, argv)
char **argv;
{

	if (--argc>0 && argv[1][0]=='-' && argv[1][1]!=0) {
		argv++;
		while (*++*argv) switch (**argv) {
		case 'n':		/* sort numerically */
			numsort_flg++;
			continue;

		case 's':		/* sort in section order */
			sectsort_flg++;
			continue;

		case 'g':		/* globl symbols only */
			globl_flg++;
			continue;

		case 'u':		/* undefined symbols only */
			undef_flg++;
			continue;

		case 'r':		/* sort in reverse order */
			revsort_flg = -1;
			continue;

		case 'p':		/* don't sort -- symbol table order */
			nosort_flg++;
			continue;

		case 'o':		/* prepend a name to each line */
			prep_flg++;
			continue;

		default:		/* oops */
			fprintf(stderr, "anm: invalid argument -%c\n", *argv[0]);
			exit(1);
		}
		argc--;
	}
	if (argc == 0) {
		argc = 1;
		argv[1] = "a.out";
	}
	narg = argc;

	while(argc--) {
		int fd;

		filename = *++argv;
		if ((fd = open(filename, 0)) < 0) {
			fprintf(stderr, "anm: cannot open %s\n", filename);
			continue;
		}
		process(fd);
		close(fd);
	}
	exit(0);
}

extern int rd_unsigned2();
extern long lseek();
extern char *strncpy();

process(fd)
	int	fd;
{
	unsigned int	magic;
	long		nextpos;
	struct ar_hdr	archive_header;
	static char	buf[sizeof(archive_header.ar_name)+1];

	if (narg > 1) printf("\n%s:\n", filename);

	magic = rd_unsigned2(fd);
	switch(magic) {
	case O_MAGIC:
		lseek(fd, 0L, 0);
		do_file(fd);
		break;
	case ARMAG:
	case AALMAG:
		while (rd_arhdr(fd, &archive_header)) {
			nextpos = lseek(fd, 0L, 1) + archive_header.ar_size;
			if (nextpos & 1) nextpos++;
			strncpy(buf,archive_header.ar_name,sizeof(archive_header.ar_name));
			filename = buf;
			if ( strcmp(filename, SYMDEF)) {
				printf("\n%s:\n", filename);
				do_file(fd);
			}
			lseek(fd, nextpos, 0);
		}
		break;
	default:
		fprintf(stderr, "anm: %s -- bad format\n", filename);
		break;
	}
}

do_file(fd)
	int	fd;
{
	struct	outname	*nbufp = NULL;
	struct	outname	nbuf;
	char		*cbufp;
	long		fi_to_co;
	long		n;
	unsigned	readcount;
	int		i,j;
	int		compare();

	read_error = 0;
	rd_fdopen(fd);

	rd_ohead(&hbuf);
	if (read_error) {
		return;
	}
	if (BADMAGIC(hbuf)) {
		return;
	}

	n = hbuf.oh_nname;
	if (n == 0) {
		fprintf(stderr, "anm: %s -- no name list\n", filename);
		return;
	}

	if (hbuf.oh_nchar == 0) {
		fprintf(stderr, "anm: %s -- no names\n", filename);
		return;
	}
	if ((readcount = hbuf.oh_nchar) != hbuf.oh_nchar) {
		fprintf(stderr, "anm: string area too big in %s\n", filename);
		exit(2);
	}

	/* store special section bases ??? */
	if (hbuf.oh_flags & HF_8086) {
		rd_sect(&sbuf, hbuf.oh_nsect);
		if (read_error) {
			return;
		}
		for (i=0; i<hbuf.oh_nsect; i++) {
			s_base[i+S_MIN] =
				(sbuf.os_base>>12) & 03777760;
		}
	}

	if ((cbufp = (char *)malloc(readcount)) == NULL) {
		fprintf(stderr, "anm: out of memory on %s\n", filename);
		exit(2);
	}
	rd_string(cbufp, hbuf.oh_nchar);
	if (read_error) {
		free(cbufp);
		return;
	}

	fi_to_co = (long) (cbufp - OFF_CHAR(hbuf));
	i = 0;
	while (--n >= 0) {
		rd_name(&nbuf, 1);
		if (read_error) {
			break;
		}

		if (globl_flg && (nbuf.on_type&S_EXT)==0)
			continue;

		if (undef_flg
		    &&
		    ((nbuf.on_type&S_TYP)!=S_UND || (nbuf.on_type&S_ETC)!=0))
			continue;

		if (nbuf.on_foff == 0) nbuf.on_mptr = 0;
		else nbuf.on_mptr = (char *) (nbuf.on_foff + fi_to_co);

		/* adjust value for specially encoded bases */
		if (hbuf.oh_flags & HF_8086) {
		    if (((nbuf.on_type&S_ETC) == 0) ||
			((nbuf.on_type&S_ETC) == S_SCT)) {
			j = nbuf.on_type&S_TYP;
			if ((j>=S_MIN) && (j<=S_MAX))
			    nbuf.on_valu += s_base[j];
		    }
		}

		if (nbufp == NULL)
			nbufp = (struct outname *)malloc(sizeof(struct outname));
		else
			nbufp = (struct outname *)realloc(nbufp, (i+1)*sizeof(struct outname));
		if (nbufp == NULL) {
			fprintf(stderr, "anm: out of memory on %s\n", filename);
			exit(2);
		}
		nbufp[i++] = nbuf;
	}

	if (nbufp && nosort_flg==0)
		qsort(nbufp, i, sizeof(struct outname), compare);

	for (n=0; n<i; n++) {
		char	cs1[4];
		char	cs2[4];

		if (prep_flg)
			printf("%s:", filename);

		switch(nbufp[n].on_type&S_ETC) {
		case S_SCT:
			sprintf(cs1, "%2d", (nbufp[n].on_type&S_TYP) - S_MIN);
			sprintf(cs2, " S");
			break;
		case S_FIL:
			sprintf(cs1, " -");
			sprintf(cs2, " F");
			break;
		case S_MOD:
			sprintf(cs1, " -");
			sprintf(cs2, " M");
			break;
		case S_COM:
			sprintf(cs1, " C");
			if (nbufp[n].on_type&S_EXT)
				sprintf(cs2, " E");
			else
				sprintf(cs2, " -");
			break;
		case 0:
			if (nbufp[n].on_type&S_EXT)
				sprintf(cs2, " E");
			else
				sprintf(cs2, " -");

			switch(nbufp[n].on_type&S_TYP) {
			case S_UND:
				sprintf(cs1, " U");
				break;
			case S_ABS:
				sprintf(cs1, " A");
				break;
			default:
				sprintf(cs1, "%2d", (nbufp[n].on_type&S_TYP) - S_MIN);
			}
			break;
		default:
			sprintf(cs1, "??");
			sprintf(cs2, " ?");
		}

		printf("%8lx %s %s %s\n",nbufp[n].on_valu,cs1,cs2,nbufp[n].on_mptr ? nbufp[n].on_mptr : "(NULL)");
	}

	if (nbufp)
		free((char *)nbufp);
	if (cbufp)
		free((char *)cbufp);
}

compare(p1, p2)
struct outname	*p1, *p2;
{
	int	i;

	if (sectsort_flg) {
		if ((p1->on_type&S_TYP) > (p2->on_type&S_TYP))
			return(revsort_flg);
		if ((p1->on_type&S_TYP) < (p2->on_type&S_TYP))
			return(-revsort_flg);
	}

	if (numsort_flg) {
		if (p1->on_valu > p2->on_valu)
			return(revsort_flg);
		if (p1->on_valu < p2->on_valu)
			return(-revsort_flg);
	}

	if (! p1->on_mptr) {
		if (! p2->on_mptr) return 0;
		return -revsort_flg;
	}
	if (! p2->on_mptr) return revsort_flg;

	i = strcmp(p1->on_mptr, p2->on_mptr);

	if (i > 0)
		return(revsort_flg);
	if (i < 0)
		return(-revsort_flg);

	return(0);
}

rd_fatal()
{
	fprintf(stderr,"read error on %s\n", filename);
	read_error = 1;
}
