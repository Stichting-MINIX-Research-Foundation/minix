#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "installboot.h"

#ifndef DFL_SECSIZE
#define DFL_SECSIZE     512
#endif

#define MFS_FIRST_SUBP_OFFSET	32

enum {
	TYPE_BAD,
	TYPE_PART,
	TYPE_DISK
};

static int
minixfs3_read_mbr(const char* device, char* buf)
{
	int fd;
	int bytes;
	int n;

	fd = open(device, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Can't open %s: %s\n", device, strerror(errno));
		return 1;
	}

	if (lseek(fd, MBR_PART_OFFSET, SEEK_SET) != MBR_PART_OFFSET) {
		fprintf(stderr, "Can't seek in %s to %d: %s\n",
			device, MBR_PART_OFFSET, strerror(errno));
		close(fd);
		return 1;
	}

	bytes = DFL_SECSIZE - MBR_PART_OFFSET;

	if ((n = read(fd, buf, bytes)) != bytes) {
		fprintf(stderr, "Can't read %d bytes from %s, %d read instead"
			": %s\n",
			bytes, device, n, strerror(errno));
		close(fd);
		return 1;
	}

	if ((uint8_t)buf[bytes-2] != 0x55 || (uint8_t)buf[bytes-1] != 0xAA) {
		fprintf(stderr, "No MBR on %s, signature is %x\n",
			device, *(uint16_t*)(&buf[bytes-2]));
		close(fd);
		return 1;
	}

	close(fd);
	return 0;
}

static int
minixfs3_get_dev_type(const char *device, ib_params *params)
{
	int len, type;

	/*
	 * Unless the -f flag is given, we expect to be provided with a primary
	 * partition.  That is, a device name that ends with "pN", N being 0-3.
	 * If the -f flag is given, we assume that anything else is a whole
	 * disk.  If we were given a subpartition, it will fail the subsequent
	 * MBR signature test, so we need not check this explicitly.
	 */
	len = strlen(device);

	if (len > 2 && device[len-2] == 'p' &&
	    (unsigned) (device[len-1] - '0') <= 3) {
		type = TYPE_PART;
	} else {
		type = TYPE_DISK;
	}

	if (type != TYPE_PART && !(params->flags & IB_FORCE)) {
		fprintf(stderr, "Wrong device %s, must be /.../cxdyp[0-3]\n",
			device);
		return TYPE_BAD;
	}

	return type;
}

int
minixfs3_is_minix_partition(ib_params *params)
{
	char buf[DFL_SECSIZE]; /* part table + signature */

	if (minixfs3_get_dev_type(params->filesystem, params) == TYPE_BAD)
		return 0;

	/* MINIX 3 partition with current scheme *must* have subpartitions,
	 * thus MBR has signature. minixfs3_read_mbr checks the signature.
	 */
	if (minixfs3_read_mbr(params->filesystem, buf))
		return 0;
	return 1;
}

/* bootxx from NetBSD is ~8Kb, and old MINIX installations have just
 * 1Kb of space for their bootblock. Check if there is enough space
 * to install bootxx_minixfs3. New installation should have 16Kb before
 * the first subpartition.
 */
int
minixfs3_has_bootblock_space(ib_params *params)
{
	const char *device;
	char buf[DFL_SECSIZE]; /* part table + signature */
	char parent_name[NAME_MAX];
	struct mbr_partition *part;
	uint32_t first_subpartition = (uint32_t) ~0;
	uint32_t parent_partition;
	int i, len, type = 0;

	device = params->filesystem;

	if ((type = minixfs3_get_dev_type(device, params)) == TYPE_BAD)
		exit(1);

	if (minixfs3_read_mbr(device, buf))
		exit(1);

	part = (struct mbr_partition *) buf;

	for (i = 0; i < 4; i++) {
		if (part[i].mbrp_size &&
		    part[i].mbrp_start < first_subpartition)
			first_subpartition = part[i].mbrp_start;
	}

	if (type == TYPE_PART) {
		/* The target is a partition.  Look up its starting offset. */
		len = strlen(device);
		strncpy(parent_name, device, len - 2);
		parent_name[len - 2] = '\0';

		if (minixfs3_read_mbr(parent_name, buf))
			exit(1);

		parent_partition = 0;
		for (i = 0; i < 4; i++) {
			struct mbr_partition *p = &part[i];
			if (p->mbrp_size && p->mbrp_start <= first_subpartition
			    && (p->mbrp_start + p->mbrp_size) >
			    first_subpartition) {
				parent_partition = p->mbrp_start;
				break;
			}
		}
	} else {
		/* The target is a whole disk.  The starting offset is 0. */
		parent_partition = 0;
	}

	if ((first_subpartition - parent_partition) < MFS_FIRST_SUBP_OFFSET)
		return 0;
	else
		return 1;
}
