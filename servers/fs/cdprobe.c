/* This file contains some code to guess where we have to load the
 * RAM image device from, if started from CD. (In this case it's hard
 * to tell where this is without diving into BIOS heuristics.)
 *
 * There is some nasty hard-codery in here (e.g. AT minor device numbers,
 * MINIX cd label) that can be improved on.
 *
 * Changes:
 *   Jul 14, 2005   Created (Ben Gras)
 */

#include "fs.h"
#include "super.h"

#include <minix/com.h>
#include <string.h>

/*===========================================================================*
 *				cdprobe					     *
 *===========================================================================*/
PUBLIC int cdprobe(void)
{
#define CD_SECTOR	2048
#define AT_MAJOR	3
#define AT_MINORS	4
	int i, minors[AT_MINORS] = { 0, 5, 10, 15 }, dev = 0, found = 0;
	char pvd[CD_SECTOR];
	printf("\nLooking for boot CD. This may take several minutes.\n"
		"Please ignore any error messages.\n\n");
	for(i = 0; i < AT_MINORS && !found; i++) {
		struct super_block probe_super;
		int r, minor;

		dev = (AT_MAJOR << MAJOR) | minors[i];

		/* 1. The drive should be a CD - which is not write-openable.
		 *    Check for this.
		 */
		if((r = dev_open(dev, FS_PROC_NR, R_BIT|W_BIT)) == OK) {
			printf("%d. no - can open r/w, so no cd\n", i);
			dev_close(dev);
			continue;
		}
		printf("passed no-r/w test ", i);

		/* 2. The drive should be a CD. Open whole drive and 
		 *    check for the PVD.
		 */
		if((r = dev_open(dev, FS_PROC_NR, R_BIT)) != OK) {
			printf("%d. no - can't open readonly\n", i);
			continue;
		}
		printf("%d. passed open-readonly test ", i);

		if((r = dev_io(DEV_READ, dev, FS_PROC_NR, pvd,
			16*CD_SECTOR, sizeof(pvd), 0)) != sizeof(pvd)) {
			printf("%d. no - can't read pvd (%d)\n", i, r);
			dev_close(dev);
			continue;
		}
		dev_close(dev);
		printf("%d. passed read pvd test ", i);

		/* Check PVD ID. */
		if(pvd[0] !=  1  || pvd[1] != 'C' || pvd[2] != 'D' ||
		   pvd[3] != '0' || pvd[4] != '0' || pvd[5] != '1' || pvd[6] != 1 ||
		   strncmp(pvd + 40, "MINIX", 5)) {
			printf("%d. no - cd signature or minix label not found\n", i, r);
		   	continue;
		   }
		printf("%d. pvd id test ", i);

		/* 3. Both c0dXp1 and p2 should have a superblock. */
		for(minor = minors[i]+2; minor <= minors[i]+3; minor++) {
			dev = (AT_MAJOR << MAJOR) | minor;
			if((r = dev_open(dev, FS_PROC_NR, R_BIT)) != OK) {
				printf("%d. no - couldn't open subdev %d\n", i, dev);
				break;
			}
			probe_super.s_dev = dev;
			r = read_super(&probe_super);
			dev_close(dev);
			if(r != OK) {
				printf("%d. subdev %d doesn't contain a superblock\n", i, dev);
				break;
			}
			printf("%d. (%d) passed superblock test ", i, minor);
		}

		if(minor > minors[i]+3) {
			/* Success? Then set dev to p1. */
			dev = (AT_MAJOR << MAJOR) | (minors[i]+2);
			found = 1;
			printf("%d. YES - passed all tests, root is %d\n", i, dev);
			break;
		} else  printf("%d. no superblock(s)\n", i);
	}

	if(!found) {
		return NO_DEV;
	}

	printf("\nCD probing done.\n");

	return dev;
}

