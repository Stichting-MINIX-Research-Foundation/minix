/*
 * Mount an MSDOS disk
 *
 * written by:
 *
 * Alain L. Knaff			
 * alain@linux.lu
 *
 */

#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"

#ifdef OS_linux
#include <sys/wait.h>
#include "mainloop.h"
#include "fs.h"

extern int errno;

void mmount(int argc, char **argv, int type)
{
	char drive;
	int pid;
	int status;
	struct device dev;
	char name[EXPAND_BUF];
	int media;
	struct bootsector boot;
	Stream_t *Stream;
	
	if (argc<2 || !argv[1][0]  || argv[1][1] != ':' || argv[1][2]){
		fprintf(stderr,"Usage: %s -V drive:\n", argv[0]);
		exit(1);
	}
	drive = toupper(argv[1][0]);
	Stream = find_device(drive, O_RDONLY, &dev, &boot, name, &media, 0);
	if(!Stream)
		exit(1);
	FREE(&Stream);

	destroy_privs();

	if ( dev.partition ) {
		char part_name[4];
		sprintf(part_name, "%d", dev.partition %1000);
		strcat(name, part_name); 
	}

	/* and finally mount it */
	switch((pid=fork())){
	case -1:
		fprintf(stderr,"fork failed\n");
		exit(1);
	case 0:
		close(2);
		open("/dev/null", O_RDWR);
		argv[1] = strdup("mount");
		if ( argc > 2 )
			execvp("mount", argv + 1 );
		else
			execlp("mount", "mount", name, 0);
		perror("exec mount");
		exit(1);
	default:
		while ( wait(&status) != pid );
	}	
	if ( WEXITSTATUS(status) == 0 )
		exit(0);
	argv[0] = strdup("mount");
	argv[1] = strdup("-r");
	if(!argv[0] || !argv[1]){
		printOom();
		exit(1);
	}
	if ( argc > 2 )
		execvp("mount", argv);
	else
		execlp("mount", "mount","-r", name, 0);
	exit(1);
}

#endif /* linux */

