/*
 * mlabel.c
 * Make an MSDOS volume label
 */

#include "sysincludes.h"
#include "msdos.h"
#include "mainloop.h"
#include "vfat.h"
#include "mtools.h"
#include "nameclash.h"

char *label_name(char *filename, int verbose, 
		 int *mangled, char *ans)
{
	int len;
	int i;
	int have_lower, have_upper;

	strcpy(ans,"           ");
	len = strlen(filename);
	if(len > 11){
		*mangled = 1;
		len = 11;
	} else
		*mangled = 0;
	strncpy(ans, filename, len);
	have_lower = have_upper = 0;
	for(i=0; i<11; i++){
		if(islower((unsigned char)ans[i]))
			have_lower = 1;
		if(isupper(ans[i]))
			have_upper = 1;
		ans[i] = toupper((unsigned char)ans[i]);

		if(strchr("^+=/[]:,?*\\<>|\".", ans[i])){
			*mangled = 1;
			ans[i] = '~';
		}
	}
	if (have_lower && have_upper)
		*mangled = 1;
	return ans;
}

int labelit(char *dosname,
	    char *longname,
	    void *arg0,
	    direntry_t *entry)
{
	time_t now;

	/* find out current time */
	getTimeNow(&now);
	mk_entry(dosname, 0x8, 0, 0, now, &entry->dir);
	return 0;
}

static void usage(void)
{
	fprintf(stderr, "Mtools version %s, dated %s\n",
		mversion, mdate);
	fprintf(stderr, "Usage: %s [-vscn] [-N serial] drive:[label]\n"
		"\t-v Verbose\n"
		"\t-s Show label\n"
		"\t-c Clear label\n"
		"\t-n New random serial number\n"
		"\t-N New given serial number\n", progname);
	exit(1);
}


void mlabel(int argc, char **argv, int type)
{
    
	char *drive, *newLabel;
	int verbose, clear, interactive, show, open_mode;
	direntry_t entry;
	int result=0;
	char longname[VBUFSIZE];
	char shortname[13];
	ClashHandling_t ch;
	struct MainParam_t mp;
	Stream_t *RootDir;
	int c;
	int mangled;
	enum { SER_NONE, SER_RANDOM, SER_SET }  set_serial = SER_NONE;
	long serial = 0;
	int need_write_boot = 0;
	int have_boot = 0;
	char *eptr = "";
	struct bootsector boot;
	Stream_t *Fs=0;
	int r;
	struct label_blk_t *labelBlock;

	init_clash_handling(&ch);
	ch.name_converter = label_name;
	ch.ignore_entry = -2;

	verbose = 0;
	clear = 0;
	show = 0;

	while ((c = getopt(argc, argv, "vcsnN:")) != EOF) {
		switch (c) {
			case 'v':
				verbose = 1;
				break;
			case 'c':
				clear = 1;
				break;
			case 's':
				show = 1;
				break;
			case 'n':
				set_serial = SER_RANDOM;
				srandom((long)time (0));
				serial=random();
				break;
			case 'N':
				set_serial = SER_SET;
				serial = strtol(optarg, &eptr, 16);
				if(*eptr) {
					fprintf(stderr,
						"%s not a valid serial number\n",
						optarg);
					exit(1);
				}
				break;
			default:
				usage();
			}
	}

	if (argc - optind != 1 || skip_drive(argv[optind]) == argv[optind]) 
		usage();

	init_mp(&mp);
	newLabel = skip_drive(argv[optind]);
	interactive = !show && !clear &&!newLabel[0] && 
		(set_serial == SER_NONE);
	open_mode = O_RDWR;
	drive = get_drive(argv[optind], NULL);
	RootDir = open_root_dir(drive, open_mode);
	if(strlen(newLabel) > VBUFSIZE) {
		fprintf(stderr, "Label too long\n");
		FREE(&RootDir);
		exit(1);
	}

	if(!RootDir && open_mode == O_RDWR && !clear && !newLabel[0] &&
	   ( errno == EACCES || errno == EPERM) ) {
		show = 1;
		interactive = 0;
		RootDir = open_root_dir(drive, O_RDONLY);
	}	    
	if(!RootDir) {
		fprintf(stderr, "%s: Cannot initialize drive\n", argv[0]);
		exit(1);
	}

	initializeDirentry(&entry, RootDir);
	r=vfat_lookup(&entry, 0, 0, ACCEPT_LABEL | MATCH_ANY,
		      shortname, longname);
	if (r == -2) {
		FREE(&RootDir);
		exit(1);
	}

	if(show || interactive){
		if(isNotFound(&entry))
			printf(" Volume has no label\n");
		else if (*longname)
			printf(" Volume label is %s (abbr=%s)\n",
			       longname, shortname);
		else
			printf(" Volume label is %s\n", shortname);

	}

	/* ask for new label */
	if(interactive){
		newLabel = longname;
		fprintf(stderr,"Enter the new volume label : ");
		fgets(newLabel, VBUFSIZE, stdin);
		if(newLabel[0])
			newLabel[strlen(newLabel)-1] = '\0';
	}

	if((!show || newLabel[0]) && !isNotFound(&entry)){
		/* if we have a label, wipe it out before putting new one */
		if(interactive && newLabel[0] == '\0')
			if(ask_confirmation("Delete volume label (y/n): ",0,0)){
				FREE(&RootDir);
				exit(0);
			}		
		entry.dir.name[0] = DELMARK;
		entry.dir.attr = 0; /* for old mlabel */
		dir_write(&entry);
	}

	if (newLabel[0] != '\0') {
		ch.ignore_entry = 1;
		result = mwrite_one(RootDir,newLabel,0,labelit,NULL,&ch) ? 
		  0 : 1;
	}

	have_boot = 0;
	if( (!show || newLabel[0]) || set_serial != SER_NONE) {
		Fs = GetFs(RootDir);
		have_boot = (force_read(Fs,(char *)&boot,0,sizeof(boot)) == 
			     sizeof(boot));
	}

	if(_WORD(boot.fatlen)) {
	    labelBlock = &boot.ext.old.labelBlock;
	} else {
	    labelBlock = &boot.ext.fat32.labelBlock;
	}

	if(!show || newLabel[0]){

		if(!newLabel[0])
			strncpy(shortname, "NO NAME    ",11);
		else
			label_name(newLabel, verbose, &mangled, shortname);

		if(have_boot && boot.descr >= 0xf0 &&
		   labelBlock->dos4 == 0x29) {
			strncpy(labelBlock->label, shortname, 11);
			need_write_boot = 1;

		}
	}

	if((set_serial != SER_NONE) & have_boot) {
		if(have_boot && boot.descr >= 0xf0 &&
		   labelBlock->dos4 == 0x29) {
			set_dword(labelBlock->serial, serial);	
			need_write_boot = 1;
		}
	}

	if(need_write_boot) {
		force_write(Fs, (char *)&boot, 0, sizeof(boot));
	}

	FREE(&RootDir);
	exit(result);
}
