/*
 * This file contains some code to guess where we have to load the
 * RAM image device from, if started from CD. (In this case it's hard
 * to tell where this is without diving into BIOS heuristics.)
 *
 * There is some nasty hard-codery in here ( MINIX cd label) that can be
 * improved on.
 *
 * Changes:
 *   Jul 14, 2005   Created (Ben Gras)
 *   Feb 10, 2006   Changed into a standalone program (Philip Homburg)
 *   May 25, 2015   Installation CD overhaul (Jean-Baptiste Boric)
 */

#define CD_SECTOR	2048
#define SUPER_OFF	1024
#define AT_MINORS	8
#define MAGIC_OFF	24

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	const int probelist[AT_MINORS] = { 2, 3, 1, 0, 6, 7, 5, 4 };
	int controller, disk, r, fd;
	off_t pos;
	char name[] = "/dev/c0dX";
	char pvd[CD_SECTOR];

	for(controller = 0; controller <= 1; controller++) {
		name[6] = '0' + controller;
		for(disk = 0; disk < AT_MINORS; disk++) {
			name[8]= '0' + probelist[disk];

			fprintf(stderr, "Trying %s  \r", name);
			fflush(stderr);

			fd = open(name, O_RDONLY);
			if ((fd < 0) && (errno != ENXIO)) {
				fprintf(stderr, "open '%s' failed: %s\n",
				        name, strerror(errno));
				continue;
			}

			/* Try to read PVD. */
			pos = lseek(fd, 16*CD_SECTOR, SEEK_SET);
			if (pos != 16*CD_SECTOR) {
				close(fd);
				continue;
			}
			r = read(fd, pvd, sizeof(pvd));
			close(fd);
			if (r != sizeof(pvd)) {
				continue;
			}

			/* Check PVD ID. */
			if (pvd[0] !=  1  || pvd[1] != 'C' || pvd[2] != 'D' ||
			    pvd[3] != '0' || pvd[4] != '0' || pvd[5] != '1' ||
			    pvd[6] != 1 ||
			    strncmp(pvd + 40, "MINIX", 5) != 0) {
				continue;
			}

			fprintf(stderr, "\nFound.\n");
			printf("%s\n", name);
			return 0;
		}
	}

	fprintf(stderr, "\nNot found.\n");
	return 1;
}
