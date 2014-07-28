/* This file contains some code to guess where we have to load the
 * RAM image device from, if started from CD. (In this case it's hard
 * to tell where this is without diving into BIOS heuristics.)
 *
 * There is some nasty hard-codery in here ( MINIX cd label) that can be
 * improved on.
 *
 * Changes:
 *   Jul 14, 2005   Created (Ben Gras)
 *   Feb 10, 2006   Changed into a standalone program (Philip Homburg)
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

#include "mfs/const.h"

char pvd[CD_SECTOR];

/*===========================================================================*
 *				cdprobe					     *
 *===========================================================================*/
int main(void)
{
	int controller, disk, r, fd, minor, found;
	off_t pos;
	u16_t *magicp;
	char name1[]= "/dev/c0dX";
	char name2[]= "/dev/c0dXpY";
	int probelist[AT_MINORS] = { 2, 3, 1, 0, 6, 7, 5, 4 };

	found= 0;
	for(controller = 0; controller <= 1; controller++) {
	   name1[6] = '0' + controller;
	   name2[6] = '0' + controller;
	   for(disk = 0; disk < AT_MINORS; disk++) {
		name1[8]= '0' + probelist[disk];

		fprintf(stderr, "Trying %s  \r", name1);
		fflush(stderr);

		fd = open(name1, O_RDONLY);
		if (fd < 0)
		{
			if (errno != ENXIO)
			{
				fprintf(stderr, "open '%s' failed: %s\n",
					name1, strerror(errno));
			}
			continue;
		}

		pos= lseek(fd, 16*CD_SECTOR, SEEK_SET);
		if (pos != 16*CD_SECTOR)
		{
			/* Strange, do we need to issue a warning? */
			close(fd);
			continue;
		}
		r = read(fd, pvd, sizeof(pvd));
		if (r != sizeof(pvd))
		{
			fprintf(stderr,
				"error reading CD label from '%s': %s\n",
				name1, strerror(errno));
			close(fd);
			continue;
		}
		close(fd);

		/* Check PVD ID. */
		if (pvd[0] !=  1  || pvd[1] != 'C' || pvd[2] != 'D' ||
		   pvd[3] != '0' || pvd[4] != '0' || pvd[5] != '1' ||
		   pvd[6] != 1 ||
		   strncmp(pvd + 40, "MINIX", 5) != 0) {
		   	continue;
		}

		/* 3. Both cXdYp1 and p2 should have a superblock. */
		found= 1;	/* Assume everything is okay */
		for (minor = 1; minor <= 2; minor++) {
			name2[8]= '0' + probelist[disk];
			name2[10]= '0' + minor;

			fd = open(name2, O_RDONLY);
			if (fd < 0)
			{
				if (errno != ENXIO)
				{
					fprintf(stderr,
						"open '%s' failed: %s\n",
						name2, strerror(errno));
				}
				found= 0;
				break;
			}
			r = read(fd, pvd, sizeof(pvd));
			if (r != sizeof(pvd))
			{
				fprintf(stderr,
				"error reading super block from '%s': %s\n",
					name2, strerror(errno));
				close(fd);
				found= 0;
				break;
			}
			close(fd);

			magicp= (u16_t *)&pvd[SUPER_OFF+MAGIC_OFF];
			if (*magicp != SUPER_V3)
			{
				fprintf(stderr, "bad super block on %s\n",
					name2);
				found= 0;
				break;
			}
		}

		if (found)
		{
			fprintf(stderr, "\nFound.\n");
			printf("%s\n", name1);
			exit(0);
		}
	   }
	}
	fprintf(stderr, "\nNot found.\n");

	return 1;
}

