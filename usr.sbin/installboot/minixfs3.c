#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/bootblock.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "installboot.h"

#ifndef DFL_SECSIZE
#define DFL_SECSIZE     512
#endif

#define MFS_FIRST_SUBP_OFFSET	32

static int minixfs3_read_mbr(const char* device, char* buf)
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


int minixfs3_is_minix_partition(const char* partition)
{
	char buf[DFL_SECSIZE]; /* part table + signature */
	int name_length = strlen(partition);

	/* partition must be 0-3 */
	if (atol(&partition[name_length-1]) >= 4) {
		fprintf(stderr, "Wrong device %s, must be /.../cxdyp[0-3]\n",
			partition);
		return 0;
	}

	/* it should be partition device, not disk */
	if (partition[name_length-2] != 'p') {
		fprintf(stderr, "Wrong device %s, must be /.../cxdyp[0-3]\n",
			partition);
		return 0;
	}

	/* MINIX 3 partition with current scheme *must* have subpartitions,
	 * thus MBR has signature. minixfs3_read_mbr checks the signature.
	 */
	if (minixfs3_read_mbr(partition, buf))
		return 0;
	return 1;
}

/* bootxx from NetBSD is ~8Kb, and old MINIX installations have just
 * 1Kb of space for their bootblock. Check if there is enough space
 * to install bootxx_minixfs3. New installation should have 16Kb before
 * the first subpartition.
 */
int minixfs3_has_bootblock_space(const char* partition)
{
	char buf[DFL_SECSIZE]; /* part table + signature */
	char disk[NAME_MAX];
	struct mbr_partition *part;
	uint32_t first_subpartition = (uint32_t) ~0;
	uint32_t parent_partition = 0;
	int i;
	int name_length = strlen(partition);

	/* partition must be 0-3 */
	if (atol(&partition[name_length-1]) >= 4) {
		fprintf(stderr, "Wrong device %s, must be /.../cxdyp[0-3]\n",
			partition);
		exit(1);
	}
	/* it should be partition device, not disk */
	if (partition[name_length-2] != 'p') {
		fprintf(stderr, "Wrong device %s, must be /.../cxdyp[0-3]\n",
			partition);
		exit(1);
	}

	if (minixfs3_read_mbr(partition, buf))
		exit(1);

	part = (struct mbr_partition *) buf;

	for (i = 0; i < 4; i++) {
		if (part[i].mbrp_size && part[i].mbrp_start < first_subpartition)
			first_subpartition = part[i].mbrp_start;
	}

	strncpy(disk, partition, name_length - 2);
	disk[name_length - 2] = '\0';

	if (minixfs3_read_mbr(disk, buf))
		exit(1);

	for (i = 0; i < 4; i++) {
		struct mbr_partition *p = &part[i];
		if (p->mbrp_size && p->mbrp_start <= first_subpartition
		    && (p->mbrp_start + p->mbrp_size) > first_subpartition) {
			parent_partition = p->mbrp_start;
			break;
		}
	}

	if ((first_subpartition - parent_partition) < MFS_FIRST_SUBP_OFFSET)
		return 0;
	else
		return 1;
}
