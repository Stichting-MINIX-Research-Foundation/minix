/* @(#)anm.c	1.6 */
/*
**	print symbol tables for
**	ACK object files
**
**	anm [-gopruns] [name ...]
*/
#define	ushort	unsigned short

#include	"out.h"

#include	<stdlib.h>
#include	<ctype.h>
#include	<stdio.h>

int	numsort_flg;
int	sectsort_flg;
int	undef_flg;
int	revsort_flg = 1;
int	globl_flg;
int	nosort_flg;
int	arch_flg;
int	prep_flg;
struct	outhead	hbuf;
struct	outsect	sbuf;
FILE	*fi;
long	off;
long	s_base[S_MAX];	/* for specially encoded bases */

main(argc, argv)
char **argv;
{
	int	narg;
	int	compare();

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
		struct	outname	*nbufp = (struct outname *)NULL;
		struct	outname	nbuf;
		char		*cbufp;
		long		fi_to_co;
		long		n;
		unsigned	readcount;
		int		i,j;

		fi = fopen(*++argv,"r");
		if (fi == (FILE *)NULL) {
			fprintf(stderr, "anm: cannot open %s\n", *argv);
			continue;
		}

		getofmt((char *)&hbuf, SF_HEAD, fi);
		if (BADMAGIC(hbuf)) {
			fprintf(stderr, "anm: %s-- bad format\n", *argv);
			fclose(fi);
			continue;
		}
		if (narg > 1)
			printf("\n%s:\n", *argv);

		n = hbuf.oh_nname;
		if (n == 0) {
			fprintf(stderr, "anm: %s-- no name list\n", *argv);
			fclose(fi);
			continue;
		}

		if (hbuf.oh_nchar == 0) {
			fprintf(stderr, "anm: %s-- no names\n", *argv);
			fclose(fi);
			continue;
		}
		if ((readcount = hbuf.oh_nchar) != hbuf.oh_nchar) {
			fprintf(stderr, "anm: string area too big in %s\n", *argv);
			exit(2);
		}

		/* store special section bases */
		if (hbuf.oh_flags & HF_8086) {
			for (i=0; i<hbuf.oh_nsect; i++) {
				getofmt((char *)&sbuf, SF_SECT, fi);
				s_base[i+S_MIN] =
					(sbuf.os_base>>12) & 03777760;
			}
		}
		 
		if ((cbufp = (char *)malloc(readcount)) == (char *)NULL) {
			fprintf(stderr, "anm: out of memory on %s\n", *argv);
			exit(2);
		}
		fseek(fi, OFF_CHAR(hbuf), 0);
		if (fread(cbufp, 1, readcount, fi) == 0) {
			fprintf(stderr, "anm: read error on %s\n", *argv);
			exit(2);
		}
		fi_to_co = (long)cbufp - OFF_CHAR(hbuf);

		fseek(fi, OFF_NAME(hbuf), 0);
		i = 0;
		while (--n >= 0) {
			getofmt((char *)&nbuf, SF_NAME, fi);

			if (nbuf.on_foff == 0)
				continue; /* skip entries without names */

			if (globl_flg && (nbuf.on_type&S_EXT)==0)
				continue;

			if (undef_flg
			    &&
			    ((nbuf.on_type&S_TYP)!=S_UND || (nbuf.on_type&S_ETC)!=0))
				continue;

			nbuf.on_mptr = (char *)(nbuf.on_foff + fi_to_co);

			/* adjust value for specially encoded bases */
			if (hbuf.oh_flags & HF_8086) {
			    if (((nbuf.on_type&S_ETC) == 0) ||
				((nbuf.on_type&S_ETC) == S_SCT)) {
				j = nbuf.on_type&S_TYP;
				if ((j>=S_MIN) && (j<=S_MAX))
				    nbuf.on_valu += s_base[j];
			    }
			}

			if (nbufp == (struct outname *)NULL)
				nbufp = (struct outname *)malloc(sizeof(struct outname));
			else
				nbufp = (struct outname *)realloc(nbufp, (i+1)*sizeof(struct outname));
			if (nbufp == (struct outname *)NULL) {
				fprintf(stderr, "anm: out of memory on %s\n", *argv);
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
				printf("%s:", *argv);

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
					sprintf(cs2," E");
				else
					sprintf(cs2," -");
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

			printf("%8lx %s %s %s\n",nbufp[n].on_valu,cs1,cs2,nbufp[n].on_mptr);
		}

		if (nbufp)
			free((char *)nbufp);
		if (cbufp)
			free((char *)cbufp);
		fclose(fi);
	}
	exit(0);
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

	i = strcmp(p1->on_mptr, p2->on_mptr);

	if (i > 0)
		return(revsort_flg);
	if (i < 0)
		return(-revsort_flg);

	return(0);
}

getofmt(p, s, f)
register char	*p;
register char	*s;
register FILE	*f;
{
	register i;
	register long l;

	for (;;) {
		switch (*s++) {
/*		case '0': p++; continue; */
		case '1':
			*p++ = getc(f);
			continue;
		case '2':
			i = getc(f);
			i |= (getc(f) << 8);
			*((short *)p) = i; p += sizeof(short);
			continue;
		case '4':
			l = (long)getc(f);
			l |= ((long)getc(f) << 8);
			l |= ((long)getc(f) << 16);
			l |= ((long)getc(f) << 24);
			*((long *)p) = l; p += sizeof(long);
			continue;
		default:
		case '\0':
			break;
		}
		break;
	}
}
