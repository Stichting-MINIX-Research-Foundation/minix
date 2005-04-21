/*
 * mmd.c
 * Makes an MSDOS directory
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

/*
 * Preserve the file modification times after the fclose()
 */

typedef struct Arg_t {
	char *target;
	MainParam_t mp;

	Stream_t *SrcDir;
	int entry;
	ClashHandling_t ch;
	Stream_t *targetDir;
} Arg_t;


typedef struct CreateArg_t {
	Stream_t *Dir;
	Stream_t *NewDir;
	unsigned char attr;
	time_t mtime;
} CreateArg_t;

/*
 * Open the named file for read, create the cluster chain, return the
 * directory structure or NULL on error.
 */
int makeit(char *dosname,
	    char *longname,
	    void *arg0,
	    direntry_t *targetEntry)
{
	Stream_t *Target;
	CreateArg_t *arg = (CreateArg_t *) arg0;
	int fat;
	direntry_t subEntry;	

	/* will it fit? At least one cluster must be free */
	if (!getfreeMinClusters(targetEntry->Dir, 1))
		return -1;
	
	mk_entry(dosname, ATTR_DIR, 1, 0, arg->mtime, &targetEntry->dir);
	Target = OpenFileByDirentry(targetEntry);
	if(!Target){
		fprintf(stderr,"Could not open Target\n");
		return -1;
	}

	/* this allocates the first cluster for our directory */

	initializeDirentry(&subEntry, Target);

	subEntry.entry = 1;
	GET_DATA(targetEntry->Dir, 0, 0, 0, &fat);
	if (fat == fat32RootCluster(targetEntry->Dir)) {
	    fat = 0;
	}
	mk_entry("..         ", ATTR_DIR, fat, 0, arg->mtime, &subEntry.dir);
	dir_write(&subEntry);

	FLUSH((Stream_t *) Target);
	subEntry.entry = 0;
	GET_DATA(Target, 0, 0, 0, &fat);
	mk_entry(".          ", ATTR_DIR, fat, 0, arg->mtime, &subEntry.dir);
	dir_write(&subEntry);

	mk_entry(dosname, ATTR_DIR | arg->attr, fat, 0, arg->mtime, 
		 &targetEntry->dir);
	arg->NewDir = Target;
	return 0;
}


static void usage(void)
{
	fprintf(stderr,
		"Mtools version %s, dated %s\n", mversion, mdate);
	fprintf(stderr,
		"Usage: %s [-D clash_option] file targetfile\n", progname);
	fprintf(stderr,
		"       %s [-D clash_option] file [files...] target_directory\n", 
		progname);
	exit(1);
}

Stream_t *createDir(Stream_t *Dir, const char *filename, ClashHandling_t *ch, 
					unsigned char attr, time_t mtime)
{
	CreateArg_t arg;
	int ret;

	arg.Dir = Dir;
	arg.attr = attr;
	arg.mtime = mtime;

	if (!getfreeMinClusters(Dir, 1))
		return NULL;

	ret = mwrite_one(Dir, filename,0, makeit, &arg, ch);
	if(ret < 1)
		return NULL;
	else
		return arg.NewDir;
}

static int createDirCallback(direntry_t *entry, MainParam_t *mp)
{
	Stream_t *ret;
	time_t now;

	ret = createDir(mp->File, mp->targetName, &((Arg_t *)(mp->arg))->ch, 
					ATTR_DIR, getTimeNow(&now));
	if(ret == NULL)
		return ERROR_ONE;
	else {
		FREE(&ret);
		return GOT_ONE;
	}
	
}

void mmd(int argc, char **argv, int type)
{
	Arg_t arg;
	int c;

	/* get command line options */

	init_clash_handling(& arg.ch);

	/* get command line options */
	while ((c = getopt(argc, argv, "D:o")) != EOF) {
		switch (c) {
			case '?':
				usage();
			case 'o':
				handle_clash_options(&arg.ch, c);
				break;
			case 'D':
				if(handle_clash_options(&arg.ch, *optarg))
					usage();
				break;
			default:
				break;
		}
	}

	if (argc - optind < 1)
		usage();

	init_mp(&arg.mp);
	arg.mp.arg = (void *) &arg;
	arg.mp.openflags = O_RDWR;
	arg.mp.callback = createDirCallback;
	arg.mp.lookupflags = OPEN_PARENT | DO_OPEN_DIRS;
	exit(main_loop(&arg.mp, argv + optind, argc - optind));
}
