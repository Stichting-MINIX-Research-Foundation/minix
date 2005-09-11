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
	printf("\nLooking for boot CD. This may take a minute.\n"
		"Please ignore any error messages.\n\n");
	for(i = 0; i < AT_MINORS && !found; i++) {
		struct super_block probe_super;
		int r, minor;

		dev = (AT_MAJOR << MAJOR) | minors[i];

		/* Open device readonly. (This fails if the device
		 * is also writable, which a CD isn't.)
		 */
		if ((r = dev_open(dev, FS_PROC_NR, RO_BIT)) != OK) {
			continue;
		}

		if ((r = dev_io(DEV_READ, dev, FS_PROC_NR, pvd,
			16*CD_SECTOR, sizeof(pvd), 0)) != sizeof(pvd)) {
			dev_close(dev);
			continue;
		}
		dev_close(dev);

		/* Check PVD ID. */
		if (pvd[0] !=  1  || pvd[1] != 'C' || pvd[2] != 'D' ||
		   pvd[3] != '0' || pvd[4] != '0' || pvd[5] != '1' || pvd[6] != 1 ||
		   strncmp(pvd + 40, "MINIX", 5)) {
		   	continue;
		   }

		/* 3. Both c0dXp1 and p2 should have a superblock. */
		for(minor = minors[i]+2; minor <= minors[i]+3; minor++) {
			dev = (AT_MAJOR << MAJOR) | minor;
			if ((r = dev_open(dev, FS_PROC_NR, R_BIT)) != OK) {
				break;
			}
			probe_super.s_dev = dev;
			r = read_super(&probe_super);
			dev_close(dev);
			if (r != OK) {
				break;
			}
		}

		if (minor > minors[i]+3) {
			/* Success? Then set dev to p1. */
			dev = (AT_MAJOR << MAJOR) | (minors[i]+2);
			found = 1;
			break;
		}
	}

	if (!found) return NO_DEV;

	return dev;
}

