/**     Adventure translated from Fortran to "C"
  and ported to Minix by:
  Robert R. Hall
  San Diego,  Calif  92115
  hall@crash.cts.com
 */

/**	program ADVENT.C					*
 *		"advent.c" allocates GLOBAL storage space by	*
 *		#defining EXTERN before #including "advdec.h".	*/


#include        <string.h>
#include	<ctype.h>
#include	<stdlib.h>
#include	<time.h>
#include        <stdio.h>
#include	<errno.h>
#include	"advent.h"		/* #define preprocessor equates	 */
#include	"advdec.h"

#ifndef TEXTDIR
#define TEXTDIR ""
#endif

char textdir[] = TEXTDIR;		/* directory where text files
					   live. */

_PROTOTYPE(int main, (int, char **));
_PROTOTYPE(static void opentxt, (void));
_PROTOTYPE(static void file_error, (char *));

int main(argc, argv)
int argc;
char **argv;
{
    opentxt();
    initialize();
    rspeak(325);
    if (argc == 2)
	restore(argv[1]);
    else {
	g.hinted[3] = yes(65, 1, 0);
	g.limit = (g.hinted[3] ? 800 : 550);
    }
    gaveup = FALSE;
    srand((unsigned) time(NULL));	/* seed random	 */
    while (!gaveup)
	turn();
    fclose(fd1);
    fclose(fd2);
    fclose(fd3);
    fclose(fd4);
    return (EXIT_SUCCESS);		/* exit = ok	 */
}					/* main		 */

/*
  Open advent?.txt files
*/
static void opentxt()
{
    static char filename[sizeof(textdir) + 16];
    static FILE **fp[] = {0, &fd1, &fd2, &fd3, &fd4};
    int i;
    for (i = 1; i <= 4; i++) {
	sprintf(filename, "%sadvent%d.dat", textdir, i);
	*fp[i] = fopen(filename, "r");
	if (!*fp[i])
	    file_error(filename);
    }
}

/*
  save adventure game
*/
void saveadv(username)
char *username;
{
    int cnt;
    FILE *savefd;

    savefd = fopen(username, "wb");
    if (savefd == NULL) {
	perror(username);
	return;
    }
    cnt = fwrite((void *) &g, 1, sizeof(struct playinfo), savefd);
    if (cnt != sizeof(struct playinfo)) {
	fprintf(stderr, "wrote %d of %u bytes\n",
		cnt, (unsigned) sizeof(struct playinfo));
	if (ferror(savefd)) {
	    fprintf(stderr, "errno is: 0x%.4x\n", errno);
	    perror(username);
	}
    }
    if (fclose(savefd) == -1) {
	perror(username);
    }
    printf("Saved in %s.\n", username);
    return;
}

/*
  restore saved game handler
*/
void restore(username)
char *username;
{
    int cnt;
    FILE *restfd;

    restfd = fopen(username, "rb");
    if (restfd == NULL)
	file_error(username);
    cnt = fread((void *) &g, 1, sizeof(struct playinfo), restfd);
    if (cnt != sizeof(struct playinfo)) {
	fprintf(stderr, "read %d bytes, expected %u\n",
		cnt, (unsigned) sizeof(struct playinfo));
	if (ferror(restfd)) {
	    fprintf(stderr, "errno is: 0x%.4x\n", errno);
	    perror(username);
	}
    }
    if (fclose(restfd) == -1) {
	perror(username);
    }
    printf("Restored from %s.\n", username);
    return;
}

static void file_error(filename)
char *filename;
{
    perror(filename);
    exit(EXIT_FAILURE);
}
