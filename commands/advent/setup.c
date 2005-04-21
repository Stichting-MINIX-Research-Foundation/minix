/**	program SETUP.C  					*
 *	execution will read the four adventure text files	*
 *	files; "advent1.txt", "advent2.txt", "advent3.txt" &	*
 *	"advent4.txt".  it will create the file "advtext.h"	*
 *	which is an Index Sequential Access Method (ISAM)	*
 *	header to be #included into "advent.c" before the	*
 *	header "advdec.h" is #included.				*/


#include	<stdio.h>
#include	<stdlib.h>
#include	"advent.h"

_PROTOTYPE(int main, (void));
_PROTOTYPE(void file_error, (char *));
_PROTOTYPE(void encode, (unsigned char *));

int main()
{

    FILE *isam, *src, *dest;
    char itxt[255];
    int cnt, i;
    long llen;
    char filename[12];
    static char *headername[] = {
       "idx1[MAXLOC]", "idx2[MAXLOC]", "idx3[MAXOBJ]", "idx4[MAXMSG]",
    };

    long x29 = (1L << 29), x30 = (1L << 30);
    if (!(x30 / 2 == x29 && 0L < x30 && x29 < x30)) {
	fprintf(stderr, "Sorry, advent needs 32-bit `long int's.\n");
	exit(EXIT_FAILURE);
    }
    isam = fopen("advtext.h", "w");
    if (!isam) {
	fprintf(stderr, "Sorry, I can't open advtext.h...\n");
	exit(EXIT_FAILURE);
    }
    fprintf(isam, "\n/*\theader: ADVTEXT.H\t\t\t\t\t*/\n\n\n");

    for (i = 1; i <= 4; i++) {
	cnt = -1;
	llen = 0L;
	sprintf(filename, "advent%d.txt", i);
	src = fopen(filename, "r");
	if (!src)
	    file_error(filename);
	sprintf(filename, "advent%d.dat", i);
	dest = fopen(filename, "w");
	if (!dest)
	    file_error(filename);
	fprintf(isam, "long\t%s = {\n\t", headername[i - 1]);
	while (fgets(itxt, 255, src)) {
	    encode((unsigned char *) itxt);
	    if (fprintf(dest, "%s\n", itxt) == EOF)
		file_error(filename);
	    if (itxt[0] == '#') {
		if (llen)
		    fprintf(isam, "%ld,%s\t", llen,
			    &"\0\0\0\0\0\0\0\n"[++cnt & 7]);
		llen = ftell(dest);
		if (llen <= 0) {
		    fprintf(stderr, "ftell err in %s\n", filename);
		    exit(EXIT_FAILURE);
		}			/* if (!llen)	 */
	    }				/* if (itxt[0])	 */
	}				/* while fgets	 */
	if (fprintf(isam, "%ld\n\t};\n\n", llen) == EOF)
	    file_error("advtext.h");
	fclose(src);
	if (fclose(dest) == EOF)
	    file_error(filename);
    }

    if (fclose(isam) == EOF)
	file_error("advtext.h");
    return EXIT_SUCCESS;
}					/* main		 */

void file_error(filename)
char *filename;
{
    perror(filename);
    exit(EXIT_FAILURE);
}

_CONST unsigned char key[4] = {'c' | 0x80, 'L' | 0x80, 'y' | 0x80, 'D' | 0x80};

void encode(msg)
unsigned char *msg;
{
    register int i;

    for (i = 1; msg[i]; i++)
	msg[i] ^= key[i & 3];
    msg[--i] = '\0';
    return;
}
