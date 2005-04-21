/* @(#)asize.c	1.4 */
#define	ushort	unsigned short

#include	<stdio.h>
#include 	"out.h"

/*
	asize -- determine object size

*/

main(argc, argv)
char **argv;
{
	struct outhead	buf;
	struct outsect	sbuf;
	ushort		nrsect;
	long		sum;
	int		gorp;
	FILE		*f;

	if (--argc == 0) {
		argc = 1;
		argv[1] = "a.out";
	}
	gorp = argc;

	while(argc--) {
		if ((f = fopen(*++argv, "r"))==NULL) {
			fprintf(stderr, "asize: cannot open %s\n", *argv);
			continue;
		}
		getofmt ((char *)&buf, SF_HEAD , f);
		if(BADMAGIC(buf)) {
			fprintf(stderr, "asize: %s-- bad format\n", *argv);
			fclose(f);
			continue;
		}
		nrsect = buf.oh_nsect;
		if (nrsect == 0) {
			fprintf(stderr, "asize: %s-- no sections\n", *argv);
			fclose(f);
			continue;
		}
		if (gorp > 1)
			printf("%s: ", *argv);

		sum = 0;
		while (nrsect-- > 0) {
			getofmt ((char *)&sbuf, SF_SECT , f);
			printf("%ld", sbuf.os_size);
			sum += sbuf.os_size;
			if (nrsect > 0)
				putchar('+');
		}
		printf(" = %ld = 0x%lx\n", sum, sum);
		fclose(f);
	}
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
