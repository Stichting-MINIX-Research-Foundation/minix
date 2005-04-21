/*	tinyhalt 1.0 - small forerunner			Author: Kees J. Bot
 *
 * Disk space on the root file system is a scarce resource.  This little
 * program sits in /sbin.  It normally calls the real halt/reboot, but if
 * that isn't available then it simply calls reboot().  Can't do any logging
 * of the event anyhow.
 */
#define nil 0
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int flag;
	char *prog;

	/* Try to run the real McCoy. */
#if __minix_vmd
	execv("/usr/sbin/halt", argv);
#else
	execv("/usr/bin/halt", argv);
#endif

	if ((prog = strrchr(*argv,'/')) == nil) prog= argv[0]; else argv++;

	sleep(2);	/* Not too fast. */

	reboot(strcmp(prog, "reboot") == 0 ? RBT_REBOOT : RBT_HALT);

	write(2, "reboot call failed\n", 19);
	return 1;
}
