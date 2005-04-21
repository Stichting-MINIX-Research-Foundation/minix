/*
 * mmove.c
 * Renames/moves an MSDOS file
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

/*
 * Preserve the file modification times after the fclose()
 */

typedef struct Arg_t {
	const char *fromname;
	int verbose;
	MainParam_t mp;

	direntry_t *entry;
	ClashHandling_t ch;
} Arg_t;


/*
 * Open the named file for read, create the cluster chain, return the
 * directory structure or NULL on error.
 */
int renameit(char *dosname,
	     char *longname,
	     void *arg0,
	     direntry_t *targetEntry)
{
	Arg_t *arg = (Arg_t *) arg0;
	int fat;

	targetEntry->dir = arg->entry->dir;
	strncpy(targetEntry->dir.name, dosname, 8);
	strncpy(targetEntry->dir.ext, dosname + 8, 3);

	if(IS_DIR(targetEntry)) {
		direntry_t *movedEntry;

		/* get old direntry. It is important that we do this
		 * on the actual direntry which is stored in the file,
		 * and not on a copy, because we will modify it, and the
		 * modification should be visible at file 
		 * de-allocation time */
		movedEntry = getDirentry(arg->mp.File);
		if(movedEntry->Dir != targetEntry->Dir) {
			/* we are indeed moving it to a new directory */
			direntry_t subEntry;
			Stream_t *oldDir;
			/* we have a directory here. Change its parent link */
			
			initializeDirentry(&subEntry, arg->mp.File);

			switch(vfat_lookup(&subEntry, "..", 2, ACCEPT_DIR,
					   NULL, NULL)) {
			    case -1:
				fprintf(stderr,
					" Directory has no parent entry\n");
				break;
			    case -2:
				return ERROR_ONE;
			    case 0:
				GET_DATA(targetEntry->Dir, 0, 0, 0, &fat);
				if (fat == fat32RootCluster(targetEntry->Dir)) {
				    fat = 0;
				}

				subEntry.dir.start[1] = (fat >> 8) & 0xff;
				subEntry.dir.start[0] = fat & 0xff;
				dir_write(&subEntry);
				if(arg->verbose){
					fprintf(stderr,
						"Easy, isn't it? I wonder why DOS can't do this.\n");
				}
				break;
			}
			
			/* wipe out original entry */			
			movedEntry->dir.name[0] = DELMARK;
			dir_write(movedEntry);
			
			/* free the old parent, allocate the new one. */
			oldDir = movedEntry->Dir;
			*movedEntry = *targetEntry;
			COPY(targetEntry->Dir);
			FREE(&oldDir);
			return 0;
		}
	}

	/* wipe out original entry */
	arg->mp.direntry->dir.name[0] = DELMARK;
	dir_write(arg->mp.direntry);
	return 0;
}



static int rename_file(direntry_t *entry, MainParam_t *mp)
/* rename a messy DOS file to another messy DOS file */
{
	int result;
	Stream_t *targetDir;
	char *shortname;
	const char *longname;

	Arg_t * arg = (Arg_t *) (mp->arg);

	arg->entry = entry;
	targetDir = mp->targetDir;

	if (targetDir == entry->Dir){
		arg->ch.ignore_entry = -1;
		arg->ch.source = entry->entry;
		arg->ch.source_entry = entry->entry;
	} else {
		arg->ch.ignore_entry = -1;
		arg->ch.source = -2;
	}

	longname = mpPickTargetName(mp);
	shortname = 0;
	result = mwrite_one(targetDir, longname, shortname,
			    renameit, (void *)arg, &arg->ch);
	if(result == 1)
		return GOT_ONE;
	else
		return ERROR_ONE;
}


static int rename_directory(direntry_t *entry, MainParam_t *mp)
{
	int ret;

	/* moves a DOS dir */
	if(isSubdirOf(mp->targetDir, mp->File)) {
		fprintf(stderr, "Cannot move directory ");
		fprintPwd(stderr, entry,0);
		fprintf(stderr, " into one of its own subdirectories (");
		fprintPwd(stderr, getDirentry(mp->targetDir),0);
		fprintf(stderr, ")\n");
		return ERROR_ONE;
	}

	if(entry->entry == -3) {
		fprintf(stderr, "Cannot move a root directory: ");
		fprintPwd(stderr, entry,0);
		return ERROR_ONE;
	}

	ret = rename_file(entry, mp);
	if(ret & ERROR_ONE)
		return ret;
	
	return ret;
}

static int rename_oldsyntax(direntry_t *entry, MainParam_t *mp)
{
	int result;
	Stream_t *targetDir;
	const char *shortname, *longname;

	Arg_t * arg = (Arg_t *) (mp->arg);
	arg->entry = entry;
	targetDir = entry->Dir;

	arg->ch.ignore_entry = -1;
	arg->ch.source = entry->entry;
	arg->ch.source_entry = entry->entry;

#if 0
	if(!strcasecmp(mp->shortname, arg->fromname)){
		longname = mp->longname;
		shortname = mp->targetName;
	} else {
#endif
		longname = mp->targetName;
		shortname = 0;
#if 0
	}
#endif
	result = mwrite_one(targetDir, longname, shortname,
			    renameit, (void *)arg, &arg->ch);
	if(result == 1)
		return GOT_ONE;
	else
		return ERROR_ONE;
}


static void usage(void)
{
	fprintf(stderr,
		"Mtools version %s, dated %s\n", mversion, mdate);
	fprintf(stderr,
		"Usage: %s [-vo] [-D clash_option] file targetfile\n", progname);
	fprintf(stderr,
		"       %s [-vo] [-D clash_option] file [files...] target_directory\n", 
		progname);
	fprintf(stderr, "\t-v Verbose\n");
	exit(1);
}

void mmove(int argc, char **argv, int oldsyntax)
{
	Arg_t arg;
	int c;
	char shortname[13];
	char longname[VBUFSIZE];
	char *def_drive;
	int i;

	/* get command line options */

	init_clash_handling(& arg.ch);

	/* get command line options */
	arg.verbose = 0;
	while ((c = getopt(argc, argv, "vD:o")) != EOF) {
		switch (c) {
			case 'v':	/* dummy option for mcopy */
				arg.verbose = 1;
				break;
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

	if (argc - optind < 2)
		usage();

	init_mp(&arg.mp);		
	arg.mp.arg = (void *) &arg;
	arg.mp.openflags = O_RDWR;

	/* look for a default drive */
	def_drive = NULL;
	for(i=optind; i<argc; i++)
		if(skip_drive(argv[i]) > argv[i]){
			char *drive = get_drive(argv[i], NULL);
			if(!def_drive)
				def_drive = drive;
			else if(strcmp(def_drive, drive) != 0){
				fprintf(stderr,
					"Cannot move files across different drives\n");
				exit(1);
			}
		}

	if(def_drive) {
		char mcwd[MAXPATHLEN];

		strcpy(mcwd, skip_drive(arg.mp.mcwd));
		if(strlen(def_drive) + 1 + strlen(mcwd) + 1 > MAXPATHLEN){
			fprintf(stderr,
				"Path name to current directory too long\n");
			exit(1);
		}
		strcpy(arg.mp.mcwd, def_drive);
		strcat(arg.mp.mcwd, ":");
		strcat(arg.mp.mcwd, mcwd);
	}

	if (oldsyntax && (argc - optind != 2 || strpbrk(":/", argv[argc-1])))
		oldsyntax = 0;

	arg.mp.lookupflags = 
	  ACCEPT_PLAIN | ACCEPT_DIR | DO_OPEN_DIRS | NO_DOTS | NO_UNIX;

	if (!oldsyntax){
		target_lookup(&arg.mp, argv[argc-1]);
		arg.mp.callback = rename_file;
		arg.mp.dirCallback = rename_directory;
	} else {
		/* do not look up the target; it will be the same dir as the
		 * source */
		arg.fromname = _basename(skip_drive(argv[optind]));
		arg.mp.targetName = strdup(argv[argc-1]);
		arg.mp.callback = rename_oldsyntax;
	}


	arg.mp.longname = longname;
	longname[0]='\0';

	arg.mp.shortname = shortname;
	shortname[0]='\0';

	exit(main_loop(&arg.mp, argv + optind, argc - optind - 1));
}
