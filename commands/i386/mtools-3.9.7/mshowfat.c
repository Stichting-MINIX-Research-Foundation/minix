/*
 * mcopy.c
 * Copy an MSDOS files to and from Unix
 *
 */


#define LOWERCASE

#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"
#include "vfat.h"
#include "mainloop.h"
#include "plain_io.h"
#include "nameclash.h"
#include "file.h"
#include "fs.h"



typedef struct Arg_t {
	char *target;
	MainParam_t mp;
	ClashHandling_t ch;
	Stream_t *sourcefile;
} Arg_t;

static int dos_showfat(direntry_t *entry, MainParam_t *mp)
{
	Stream_t *File=mp->File;

	fprintPwd(stdout, entry,0);
	putchar(' ');
	printFat(File);
	printf("\n");
	return GOT_ONE;
}

static int unix_showfat(MainParam_t *mp)
{
	fprintf(stderr,"File does not reside on a Dos fs\n");
	return ERROR_ONE;
}


static void usage(void)
{
	fprintf(stderr,
		"Mtools version %s, dated %s\n", mversion, mdate);
	fprintf(stderr,
		"Usage: %s file ...\n", progname);
	exit(1);
}

void mshowfat(int argc, char **argv, int mtype)
{
	Arg_t arg;
	int c, ret;
	
	/* get command line options */

	init_clash_handling(& arg.ch);

	/* get command line options */
	while ((c = getopt(argc, argv, "")) != EOF) {
		switch (c) {
			case '?':
				usage();
				break;
		}
	}

	if (argc - optind < 1)
		usage();

	/* only 1 file to copy... */
	init_mp(&arg.mp);
	arg.mp.arg = (void *) &arg;

	arg.mp.callback = dos_showfat;
	arg.mp.unixcallback = unix_showfat;

	arg.mp.lookupflags = ACCEPT_PLAIN | ACCEPT_DIR | DO_OPEN;
	ret=main_loop(&arg.mp, argv + optind, argc - optind);
	exit(ret);
}
