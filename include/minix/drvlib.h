/* IBM device driver definitions			Author: Kees J. Bot
 *								7 Dec 1995
 */

#include <machine/partition.h>

void partition(struct blockdriver *bdr, int device, int style, int
	atapi);

#define DEV_PER_DRIVE	(1 + NR_PARTITIONS)
#define MINOR_t0	64
#define MINOR_r0	120
#define MINOR_d0p0s0	128
#define MINOR_fd0p0	(28<<2)
#define P_FLOPPY	0
#define P_PRIMARY	1
#define P_SUB		2
