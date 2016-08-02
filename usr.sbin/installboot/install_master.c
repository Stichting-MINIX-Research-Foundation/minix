/* Based on original installboot from MINIX 3.
 *
 *	installboot 3.0 - Make a device bootable	Author: Kees J. Bot
 *								21 Dec 1991
 */
#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "installboot.h"

#define BOOTBLOCK	0	/* Of course */
#define BOOT_BLOCK_SIZE 1024
#define SIGNATURE	0xAA55	/* Boot block signature. */
#define SIGPOS		510	/* Where to put signature word. */
#define PARTPOS		446	/* Offset to the partition table in a master
				 * boot block.
				 */


static int rawfd;	/* File descriptor to open device. */
static const char *rawdev;	/* Name of device. */


static void report(const char *label)
/* installboot: label: No such file or directory */
{
	fprintf(stderr, "installboot: %s: %s\n", label, strerror(errno));
}

static __dead void fatal(const char *label)
{
	report(label);
	exit(1);
}

static void bread(FILE *f, char *name, void *buf, size_t len)
/* Read len bytes.  Don't dare return without them. */
{
	if (len > 0 && fread(buf, len, 1, f) != 1) {
		if (ferror(f)) fatal(name);
		fprintf(stderr, "installboot: Unexpected EOF on %s\n", name);
		exit(1);
	}
}

static void readblock(off_t blk, char *buf, int block_size)
/* For rawfs, so that it can read blocks. */
{
	int n;

	if (lseek(rawfd, blk * block_size, SEEK_SET) < 0
		|| (n= read(rawfd, buf, block_size)) < 0
	) fatal(rawdev);

	if (n < block_size) {
		fprintf(stderr, "installboot: Unexpected EOF on %s\n", rawdev);
		exit(1);
	}
}

static void writeblock(off_t blk, const char *buf, int block_size)
/* Add a function to write blocks for local use. */
{
	if (lseek(rawfd, blk * block_size, SEEK_SET) < 0
		|| write(rawfd, buf, block_size) < 0
	) fatal(rawdev);
}


/* A temp stub until fdisk is ported to MINIX */
void install_master(const char *device, char *masterboot, char **guide)
/* Booting a hard disk is a two stage process:  The master bootstrap in sector
 * 0 loads the bootstrap from sector 0 of the active partition which in turn
 * starts the operating system.  This code installs such a master bootstrap
 * on a hard disk.  If guide[0] is non-null then the master bootstrap is
 * guided into booting a certain device.
 */
{
	FILE *masf;
	unsigned long size;
	static char buf[BOOT_BLOCK_SIZE];

	/* Open device. */
	if ((rawfd= open(rawdev= device, O_RDWR)) < 0) fatal(device);

	/* Open the master boot code. */
	if ((masf= fopen(masterboot, "r")) == NULL) fatal(masterboot);

	size= PARTPOS;

	/* Read the master boot block, patch it, write. */
	readblock(BOOTBLOCK, buf, BOOT_BLOCK_SIZE);

	memset(buf, 0, PARTPOS);
	(void) bread(masf, masterboot, buf, size);

	/* Install signature. */
	buf[SIGPOS+0]= (SIGNATURE >> 0) & 0xFF;
	buf[SIGPOS+1]= (SIGNATURE >> 8) & 0xFF;

	writeblock(BOOTBLOCK, buf, BOOT_BLOCK_SIZE);
}

int isoption(const char *option, const char *test)
/* Check if the option argument is equals "test".  Also accept -i as short
 * for -image, and the special case -x for -extract.
 */
{
	if (strcmp(option, test) == 0) return 1;
	if (option[0] != '-' && strlen(option) != 2) return 0;
	if (option[1] == test[1]) return 1;
	if (option[1] == 'x' && test[1] == 'e') return 1;
	return 0;
}
