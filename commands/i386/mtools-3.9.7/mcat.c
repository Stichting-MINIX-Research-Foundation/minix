/*
 * mcat.c
 * Same thing as cat /dev/fd0 or cat file >/dev/fd0
 * Something, that isn't possible with floppyd anymore.
 */

#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"
#include "mainloop.h"
#include "fsP.h"
#include "xdf_io.h"
#include "floppyd_io.h"
#include "plain_io.h"

void usage(void) 
{
	fprintf(stderr, "Mtools version %s, dated %s\n", 
		mversion, mdate);
	fprintf(stderr, "Usage: mcat [-w] device\n");
	fprintf(stderr, "       -w write on device else read\n");
	exit(1);
}

#define BUF_SIZE 16000

void mcat(int argc, char **argv, int type)
{
	struct device *dev;
	struct device out_dev;
	char *drive, name[EXPAND_BUF];
        char errmsg[200];
        Stream_t *Stream;
	char buf[BUF_SIZE];

	mt_off_t address = 0;

	char mode = O_RDONLY;
	int optindex = 1;
	size_t len;

	noPrivileges = 1;

	if (argc < 2) {
		usage();
	}

	if (argv[1][0] == '-') {
		if (argv[1][1] != 'w') {
			usage();
		}
		mode = O_WRONLY;
		optindex++;
	}

	if (argc - optindex < 1)
		usage();


	if (skip_drive(argv[optindex]) == argv[optindex])
		usage();

        drive = get_drive(argv[optindex], NULL);

        /* check out a drive whose letter and parameters match */       
        sprintf(errmsg, "Drive '%s:' not supported", drive);    
        Stream = NULL;
        for (dev=devices; dev->name; dev++) {
                FREE(&Stream);
                if (strcmp(dev->drive, drive) != 0)
                        continue;
                out_dev = *dev;
                expand(dev->name,name);
#ifdef USING_NEW_VOLD
                strcpy(name, getVoldName(dev, name));
#endif

                Stream = 0;
#ifdef USE_XDF
                Stream = XdfOpen(&out_dev, name, mode, errmsg, 0);
				if(Stream)
                        out_dev.use_2m = 0x7f;

#endif

#ifdef USE_FLOPPYD
                if(!Stream)
                        Stream = FloppydOpen(&out_dev, dev, name, 
					     mode, errmsg, 0, 1);
#endif


                if (!Stream)
                        Stream = SimpleFileOpen(&out_dev, dev, name, mode,
						errmsg, 0, 1, 0);

                if( !Stream)
                        continue;
                break;
        }

        /* print error msg if needed */ 
        if ( dev->drive == 0 ){
                FREE(&Stream);
                fprintf(stderr,"%s\n",errmsg);
                exit(1);
        }

	if (mode == O_WRONLY) {
		while ((len = fread(buf, 1, BUF_SIZE, stdin)) 
		       == BUF_SIZE) {
			WRITES(Stream, buf, address, BUF_SIZE);
			address += BUF_SIZE;
		}
		if (len)
			WRITES(Stream, buf, address, len);
	} else {
		while ((len = READS(Stream, buf, address, BUF_SIZE)) 
		       == BUF_SIZE) {
			fwrite(buf, 1, BUF_SIZE, stdout);
			address += BUF_SIZE;
		}
		if (len)
			fwrite(buf, 1, len, stdout);
	}

	FREE(&Stream);
	exit(0);
}
