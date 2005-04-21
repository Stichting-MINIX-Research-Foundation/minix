/*
 * mcd.c: Change MSDOS directories
 */

#include "sysincludes.h"
#include "msdos.h"
#include "mainloop.h"
#include "mtools.h"


static int mcd_callback(direntry_t *entry, MainParam_t *mp)
{
	FILE *fp;

	if (!(fp = open_mcwd("w"))){
		fprintf(stderr,"mcd: Can't open mcwd .file for writing\n");
		return ERROR_ONE;
	}
	
	fprintPwd(fp, entry,0);
	fprintf(fp, "\n");
	fclose(fp);
	return GOT_ONE | STOP_NOW;
}


void mcd(int argc, char **argv, int type)
{
	struct MainParam_t mp;

	if (argc > 2) {
		fprintf(stderr, "Mtools version %s, dated %s\n", 
			mversion, mdate);
		fprintf(stderr, "Usage: %s: msdosdirectory\n", argv[0]);
		exit(1);
	}

	init_mp(&mp);
	mp.lookupflags = ACCEPT_DIR | NO_DOTS;
	mp.dirCallback = mcd_callback;
	if (argc == 1) {
		printf("%s\n", mp.mcwd);
		exit(0);
	} else 
		exit(main_loop(&mp, argv + 1, 1));
}
