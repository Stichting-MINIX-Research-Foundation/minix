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


/*
 * Preserve the file modification times after the fclose()
 */

static void set_mtime(const char *target, time_t mtime)
{
	if (target && strcmp(target, "-") && mtime != 0L) {
#ifdef HAVE_UTIMES
		struct timeval tv[2];	
		tv[0].tv_sec = mtime;
		tv[0].tv_usec = 0;
		tv[1].tv_sec = mtime;
		tv[1].tv_usec = 0;
		utimes((char *)target, tv);
#else
#ifdef HAVE_UTIME
		struct utimbuf utbuf;

		utbuf.actime = mtime;
		utbuf.modtime = mtime;
		utime(target, &utbuf);
#endif
#endif
	}
	return;
}

typedef struct Arg_t {
	int recursive;
	int preserveAttributes;
	int preserveTime;
	unsigned char attr;
	char *path;
	int textmode;
	int needfilter;
	int nowarn;
	int verbose;
	int type;
	MainParam_t mp;
	ClashHandling_t ch;
} Arg_t;

/* Write the Unix file */
static int unix_write(direntry_t *entry, MainParam_t *mp, int needfilter)
{
	Arg_t *arg=(Arg_t *) mp->arg;
	time_t mtime;
	Stream_t *File=mp->File;
	Stream_t *Target, *Source;
	struct stat stbuf;
	int ret;
	char errmsg[80];
	char *unixFile;

	File->Class->get_data(File, &mtime, 0, 0, 0);

	if (!arg->preserveTime)
		mtime = 0L;

	if(arg->type)
		unixFile = "-";
	else
		unixFile = mpBuildUnixFilename(mp);
	if(!unixFile) {
		printOom();
		return ERROR_ONE;
	}

	/* if we are creating a file, check whether it already exists */
	if(!arg->type) {
		if (!arg->nowarn && &arg->type && !access(unixFile, 0)){
			if( ask_confirmation("File \"%s\" exists, overwrite (y/n) ? ",
					     unixFile,0)) {
				free(unixFile);
				return ERROR_ONE;
			}
			
			/* sanity checking */
			if (!stat(unixFile, &stbuf) && !S_ISREG(stbuf.st_mode)) {
				fprintf(stderr,"\"%s\" is not a regular file\n",
					unixFile);
				
				free(unixFile);
				return ERROR_ONE;
			}
		}
	}

	if(!arg->type && arg->verbose) {
		fprintf(stderr,"Copying ");
		mpPrintFilename(stderr,mp);
		fprintf(stderr,"\n");
	}
	
	if(got_signal) {
		free(unixFile);
		return ERROR_ONE;
	}

	if ((Target = SimpleFileOpen(0, 0, unixFile,
				     O_WRONLY | O_CREAT | O_TRUNC,
				     errmsg, 0, 0, 0))) {
		ret = 0;
		if(needfilter && arg->textmode){
			Source = open_filter(COPY(File));
			if (!Source)
				ret = -1;
		} else
			Source = COPY(File);

		if (ret == 0 )
			ret = copyfile(Source, Target);
		FREE(&Source);
		FREE(&Target);
		if(ret <= -1){
			if(!arg->type) {
				unlink(unixFile);
				free(unixFile);
			}
			return ERROR_ONE;
		}
		if(!arg->type) {
			set_mtime(unixFile, mtime);
			free(unixFile);
		}
		return GOT_ONE;
	} else {
		fprintf(stderr,"%s\n", errmsg);
		if(!arg->type)
			free(unixFile);
		return ERROR_ONE;
	}
}

static int makeUnixDir(char *filename)
{
	if(!mkdir(filename, 0777))
		return 0;
	if(errno == EEXIST) {
		struct stat buf;
		if(stat(filename, &buf) < 0)
			return -1;
		if(S_ISDIR(buf.st_mode))
			return 0;
		errno = ENOTDIR;
	}
	return -1;
}

/* Copy a directory to Unix */
static int unix_copydir(direntry_t *entry, MainParam_t *mp)
{
	Arg_t *arg=(Arg_t *) mp->arg;
	time_t mtime;
	Stream_t *File=mp->File;
	int ret;
	char *unixFile;

	if (!arg->recursive && mp->basenameHasWildcard)
		return 0;

	File->Class->get_data(File, &mtime, 0, 0, 0);	
	if (!arg->preserveTime)
		mtime = 0L;
	if(!arg->type && arg->verbose) {
		fprintf(stderr,"Copying ");
		fprintPwd(stderr, entry,0);
		fprintf(stderr, "\n");
	}
	if(got_signal)
		return ERROR_ONE;
	unixFile = mpBuildUnixFilename(mp);
	if(!unixFile) {
		printOom();
		return ERROR_ONE;
	}
	if(arg->type || !*mpPickTargetName(mp) || !makeUnixDir(unixFile)) {
		Arg_t newArg;

		newArg = *arg;
		newArg.mp.arg = (void *) &newArg;
		newArg.mp.unixTarget = unixFile;
		newArg.mp.targetName = 0;
		newArg.mp.basenameHasWildcard = 1;

		ret = mp->loop(File, &newArg.mp, "*");
		set_mtime(unixFile, mtime);
		free(unixFile);
		return ret | GOT_ONE;		
	} else {
		perror("mkdir");
		fprintf(stderr, 
			"Failure to make directory %s\n", 
			unixFile);
		free(unixFile);
		return ERROR_ONE;
	}
}

static  int dos_to_unix(direntry_t *entry, MainParam_t *mp)
{
	return unix_write(entry, mp, 1);
}


static  int unix_to_unix(MainParam_t *mp)
{
	return unix_write(0, mp, 0);
}


static int directory_dos_to_unix(direntry_t *entry, MainParam_t *mp)
{
	return unix_copydir(entry, mp);
}

/*
 * Open the named file for read, create the cluster chain, return the
 * directory structure or NULL on error.
 */
static int writeit(char *dosname,
		   char *longname,
		   void *arg0,
		   direntry_t *entry)
{
	Stream_t *Target;
	time_t now;
	int type, fat, ret;
	time_t date;
	mt_size_t filesize, newsize;
	Arg_t *arg = (Arg_t *) arg0;



	if (arg->mp.File->Class->get_data(arg->mp.File,
									  & date, &filesize, &type, 0) < 0 ){
		fprintf(stderr, "Can't stat source file\n");
		return -1;
	}

	if (type){
		if (arg->verbose)
			fprintf(stderr, "\"%s\" is a directory\n", longname);
		return -1;
	}

	/*if (!arg->single || arg->recursive)*/
	if(arg->verbose)
		fprintf(stderr,"Copying %s\n", longname);
	if(got_signal)
		return -1;

	/* will it fit? */
	if (!getfreeMinBytes(arg->mp.targetDir, filesize))
		return -1;
	
	/* preserve mod time? */
	if (arg->preserveTime)
		now = date;
	else
		getTimeNow(&now);

	mk_entry(dosname, arg->attr, 1, 0, now, &entry->dir);

	Target = OpenFileByDirentry(entry);
	if(!Target){
		fprintf(stderr,"Could not open Target\n");
		exit(1);
	}
	if (arg->needfilter & arg->textmode)
		Target = open_filter(Target);



	ret = copyfile(arg->mp.File, Target);
	GET_DATA(Target, 0, &newsize, 0, &fat);
	FREE(&Target);
	if (arg->needfilter & arg->textmode)
	    newsize++; /* ugly hack: we gathered the size before the Ctrl-Z
			* was written.  Increment it manually */
	if(ret < 0 ){
		fat_free(arg->mp.targetDir, fat);
		return -1;
	} else {
		mk_entry(dosname, arg->attr, fat, truncBytes32(newsize),
				 now, &entry->dir);
		return 0;
	}
}



static int dos_write(direntry_t *entry, MainParam_t *mp, int needfilter)
/* write a messy dos file to another messy dos file */
{
	int result;
	Arg_t * arg = (Arg_t *) (mp->arg);
	const char *targetName = mpPickTargetName(mp);

	if(entry && arg->preserveAttributes)
		arg->attr = entry->dir.attr;
	else
		arg->attr = ATTR_ARCHIVE;

	arg->needfilter = needfilter;
	if (entry && mp->targetDir == entry->Dir){
		arg->ch.ignore_entry = -1;
		arg->ch.source = entry->entry;
	} else {
		arg->ch.ignore_entry = -1;
		arg->ch.source = -2;
	}
	result = mwrite_one(mp->targetDir, targetName, 0,
			    writeit, (void *)arg, &arg->ch);
	if(result == 1)
		return GOT_ONE;
	else
		return ERROR_ONE;
}

static Stream_t *subDir(Stream_t *parent, const char *filename)
{
	direntry_t entry;		
	initializeDirentry(&entry, parent);

	switch(vfat_lookup(&entry, filename, -1, ACCEPT_DIR, 0, 0)) {
	    case 0:
		return OpenFileByDirentry(&entry);
	    case -1:
		return NULL;
	    default: /* IO Error */
		return NULL;
	}
}

static int dos_copydir(direntry_t *entry, MainParam_t *mp)
/* copyes a directory to Dos */
{
	Arg_t * arg = (Arg_t *) (mp->arg);
	Arg_t newArg;
	time_t now;
	time_t date;
	int ret;
	const char *targetName = mpPickTargetName(mp);

	if (!arg->recursive && mp->basenameHasWildcard)
		return 0;

	if(entry && isSubdirOf(mp->targetDir, mp->File)) {
		fprintf(stderr, "Cannot recursively copy directory ");
		fprintPwd(stderr, entry,0);
		fprintf(stderr, " into one of its own subdirectories ");
		fprintPwd(stderr, getDirentry(mp->targetDir),0);
		fprintf(stderr, "\n");
		return ERROR_ONE;
	}

	if (arg->mp.File->Class->get_data(arg->mp.File,
					  & date, 0, 0, 0) < 0 ){
		fprintf(stderr, "Can't stat source file\n");
		return ERROR_ONE;
	}

	if(!arg->type && arg->verbose)
		fprintf(stderr,"Copying %s\n", mpGetBasename(mp));

	if(entry && arg->preserveAttributes)
		arg->attr = entry->dir.attr;
	else
		arg->attr = 0;

	if (entry && (mp->targetDir == entry->Dir)){
		arg->ch.ignore_entry = -1;
		arg->ch.source = entry->entry;
	} else {
		arg->ch.ignore_entry = -1;
		arg->ch.source = -2;
	}

	/* preserve mod time? */
	if (arg->preserveTime)
		now = date;
	else
		getTimeNow(&now);

	newArg = *arg;
	newArg.mp.arg = &newArg;
	newArg.mp.targetName = 0;
	newArg.mp.basenameHasWildcard = 1;
	if(*targetName) {
		/* maybe the directory already exist. Use it */
		newArg.mp.targetDir = subDir(mp->targetDir, targetName);
		if(!newArg.mp.targetDir)
			newArg.mp.targetDir = createDir(mp->targetDir, 
							targetName,
							&arg->ch, arg->attr, 
							now);
	} else
		newArg.mp.targetDir = mp->targetDir;

	if(!newArg.mp.targetDir)
		return ERROR_ONE;

	ret = mp->loop(mp->File, &newArg.mp, "*");
	if(*targetName)
		FREE(&newArg.mp.targetDir);
	return ret | GOT_ONE;
}


static int dos_to_dos(direntry_t *entry, MainParam_t *mp)
{
	return dos_write(entry, mp, 0);
}

static int unix_to_dos(MainParam_t *mp)
{
	return dos_write(0, mp, 1);
}


static void usage(void)
{
	fprintf(stderr,
		"Mtools version %s, dated %s\n", mversion, mdate);
	fprintf(stderr,
		"Usage: %s [-/spabtnmvQB] [-D clash_option] sourcefile targetfile\n", progname);
	fprintf(stderr,
		"       %s [-/spabtnmvQB] [-D clash_option] sourcefile [sourcefiles...] targetdirectory\n", 
		progname);
	fprintf(stderr,
		"\t-/ -s Recursive\n"
		"\t-p Preserve attributes\n"
		"\t-a -t Textmode\n"
		"\t-n Overwrite UNIX files without confirmation\n"
		"\t-m Preserve file time (default under Minix)\n"
		"\t-v Verbose\n"
		"\t-Q Quit on the first error\n"
		"\t-b -B Batch mode (faster, but less crash resistent)\n"
		"\t-o Overwrite DOS files without confirmation\n");
	exit(1);
}

void mcopy(int argc, char **argv, int mtype)
{
	Arg_t arg;
	int c, ret, fastquit;
	int todir;
	

	/* get command line options */

	init_clash_handling(& arg.ch);

	/* get command line options */
	todir = 0;
	arg.recursive = 0;
#ifdef OS_Minix
	arg.preserveTime = 1;	/* Copy file time as DOS does. */
#else
	arg.preserveTime = 0;
#endif
	arg.preserveAttributes = 0;
	arg.nowarn = 0;
	arg.textmode = 0;
	arg.verbose = 0;
	arg.type = mtype;
	fastquit = 0;
	while ((c = getopt(argc, argv, "abB/sptnmvQD:o")) != EOF) {
		switch (c) {
			case 's':
			case '/':
				arg.recursive = 1;
				break;
			case 'p':
				arg.preserveAttributes = 1;
				break;
			case 'a':
			case 't':
				arg.textmode = 1;
				break;
			case 'n':
				arg.nowarn = 1;
				break;
			case 'm':
				arg.preserveTime = 1;
				break;
			case 'v':
				arg.verbose = 1;
				break;
			case 'Q':
				fastquit = 1;
				break;
			case 'B':
			case 'b':
				batchmode = 1;
				break;
			case 'o':
				handle_clash_options(&arg.ch, c);
				break;
			case 'D':
				if(handle_clash_options(&arg.ch, *optarg))
					usage();
				break;
			case '?':
				usage();
			default:
				break;
		}
	}

	if (argc - optind < 1)
		usage();

	init_mp(&arg.mp);
	arg.mp.lookupflags = ACCEPT_PLAIN | ACCEPT_DIR | DO_OPEN | NO_DOTS;
	arg.mp.fast_quit = fastquit;
	arg.mp.arg = (void *) &arg;
	arg.mp.openflags = O_RDONLY;

	/* last parameter is "-", use mtype mode */
	if(!mtype && !strcmp(argv[argc-1], "-")) {
		arg.type = mtype = 1;
		argc--;
	}

	if(mtype){
		/* Mtype = copying to stdout */
		arg.mp.targetName = strdup("-");
		arg.mp.unixTarget = strdup("");
		arg.mp.callback = dos_to_unix;
		arg.mp.dirCallback = unix_copydir;
		arg.mp.unixcallback = unix_to_unix;		
	} else {
		char *target;
		if (argc - optind == 1) {
			/* copying to the current directory */
			target = ".";
		} else {
			/* target is the last item mentioned */
			argc--;
			target = argv[argc];
		}

		ret = target_lookup(&arg.mp, target);
		if(!arg.mp.targetDir && !arg.mp.unixTarget) {
			fprintf(stderr,"Bad target %s\n", target);
			exit(1);
		}

		/* callback functions */
		if(arg.mp.unixTarget) {
			arg.mp.callback = dos_to_unix;
			arg.mp.dirCallback = directory_dos_to_unix;
			arg.mp.unixcallback = unix_to_unix;
		} else {
			arg.mp.dirCallback = dos_copydir;
			arg.mp.callback = dos_to_dos;
			arg.mp.unixcallback = unix_to_dos;
		}
	}

	exit(main_loop(&arg.mp, argv + optind, argc - optind));
}
