/* Test program for doctoring the fat */


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
#include "fsP.h"

typedef struct Arg_t {
	char *target;
	MainParam_t mp;
	ClashHandling_t ch;
	Stream_t *sourcefile;
	unsigned long fat;
	int markbad;
	int setsize;
	unsigned long size;
	Fs_t *Fs;
} Arg_t;

static int dos_doctorfat(direntry_t *entry, MainParam_t *mp)
{
	Fs_t *Fs = getFs(mp->File);
	Arg_t *arg=(Arg_t *) mp->arg;
	
	if(!arg->markbad && entry->entry != -3) {
		/* if not root directory, change it */
		set_word(entry->dir.start, arg->fat & 0xffff);
		set_word(entry->dir.startHi, arg->fat >> 16);
		if(arg->setsize)
			set_dword(entry->dir.size, arg->size);
		dir_write(entry);		
	}
	arg->Fs = Fs; 
	return GOT_ONE;
}

static int unix_doctorfat(MainParam_t *mp)
{
	fprintf(stderr,"File does not reside on a Dos fs\n");
	return ERROR_ONE;
}


static void usage(void)
{
	fprintf(stderr,
		"Mtools version %s, dated %s\n", mversion, mdate);
	fprintf(stderr,
		"Usage: %s [-b] [-o offset] [-s size] file fat\n", progname);
	exit(1);
}

void mdoctorfat(int argc, char **argv, int mtype)
{
	Arg_t arg;
	int c, ret;
	long address, begin, end;
	char *number, *eptr;
	int i, j;
	long offset;
	
	/* get command line options */

	init_clash_handling(& arg.ch);

	offset = 0;

	arg.markbad = 0;
	arg.setsize = 0;

	/* get command line options */
	while ((c = getopt(argc, argv, "bo:s:")) != EOF) {
		switch (c) {
			case 'b':
				arg.markbad = 1;
				break;
			case 'o':
				offset = strtoul(optarg,0,0);
				break;
			case 's':
				arg.setsize=1;
				arg.size = strtoul(optarg,0,0);
				break;
			case '?':
				usage();
				break;
		}
	}

	if (argc - optind < 2)
		usage();


	/* only 1 file to copy... */
	init_mp(&arg.mp);
	arg.mp.arg = (void *) &arg;
		
	arg.mp.callback = dos_doctorfat;
	arg.mp.unixcallback = unix_doctorfat;
	
	arg.mp.lookupflags = ACCEPT_PLAIN | ACCEPT_DIR | DO_OPEN;
	arg.mp.openflags = O_RDWR;
	arg.fat = strtoul(argv[optind+1], 0, 0) + offset;
	ret=main_loop(&arg.mp, argv + optind, 1);
	if(ret)
		exit(ret);
	address = 0;
	for(i=optind+1; i < argc; i++) {
		number = argv[i];
		if (*number == '<') {
			number++;
		}
		begin = strtoul(number, &eptr, 0);
		if (eptr && *eptr == '-') {
			number = eptr+1;
			end = strtoul(number, &eptr, 0);
		} else {
			end = begin;
		}
		if (eptr == number) {
			fprintf(stderr, "Not a number: %s\n", number);
			exit(-1);
		}

		if (eptr && *eptr == '>') {
			eptr++;
		}
		if (eptr && *eptr) {
			fprintf(stderr, "Not a number: %s\n", eptr);
			exit(-1);
		}

		for (j=begin; j <= end; j++) {
			if(arg.markbad) {
				arg.Fs->fat_encode(arg.Fs, j+offset, arg.Fs->last_fat ^ 6 ^ 8);
			} else {
				if(address) {
					arg.Fs->fat_encode(arg.Fs, address, j+offset);
				}
				address = j+offset;
			}
		}
	}

	if (address && !arg.markbad) {
		arg.Fs->fat_encode(arg.Fs, address, arg.Fs->end_fat);
	}

	exit(ret);
}
