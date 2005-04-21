/*
 * Do filename expansion with the shell.
 */

#define EXPAND_BUF	2048

#include "sysincludes.h"
#include "mtools.h"

void precmd(struct device *dev)
{
	int status;
	pid_t pid;

	if(!dev || !dev->precmd)
		return;
	
	switch((pid=fork())){
		case -1:
			perror("Could not fork");
			exit(1);
			break;
		case 0: /* the son */
			execl("/bin/sh", "sh", "-c", dev->precmd, 0);
			break;
		default:
			wait(&status);
			break;
	}
}
		
