/* This procedure examines a file system and figures out whether it is
 * version 1 or version 2.  It returns the result as an int.  If the
 * file system is neither, it returns -1.  A typical call is:
 *
 *	n = fsversion("/dev/hd1", "df");
 *
 * The first argument is the special file for the file system.
 * The second is the program name, which is used in error messages.
 */

#include <sys/types.h>
#include <stdint.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "mfs/const.h"

static char super[SUPER_BLOCK_BYTES];

#define MAGIC_OFFSET_MFS	0x18
#define MAGIC_OFFSET_EXT	0x38
#define MAGIC_OFFSET_ISO9660	0x8000
#define MAGIC_VALUE_EXT2	0xef53

/* UFS1/UFS2 FFS (native little-endian only, as used by the MINIX ffs server).
 * The UFS2 superblock lives at byte offset 65536 (standard newfs) or 8192
 * (nbmakefs -t ffs -o version=2); the UFS1 superblock lives at 8192.  fs_magic
 * is at offset 1372 and fs_sblockloc at offset 1000 within "struct fs".
 */
#define SBLOCK_UFS2		65536
#define SBLOCK_UFS1		8192
#define FFS_MAGIC_OFFSET	1372
#define FFS_SBLOCKLOC_OFFSET	1000
#define FS_UFS1_MAGIC		0x011954
#define FS_UFS2_MAGIC		0x19540119
#define FS_UFS2EA_MAGIC		0x19012038

static int check_super(off_t offset, unsigned short magic)
{
	return (memcmp(super + offset, &magic, sizeof(magic)) == 0) ? 1 : 0;
}

static int check_ffs(int fd)
{
	static const off_t sblock_try[] = { SBLOCK_UFS2, SBLOCK_UFS1, -1 };
	int32_t magic;
	int64_t sblockloc;
	int i;

	for (i = 0; sblock_try[i] != -1; i++) {
		off_t off = sblock_try[i];

		if (lseek(fd, off + FFS_MAGIC_OFFSET, SEEK_SET) < 0)
			continue;
		if (read(fd, &magic, sizeof(magic)) != sizeof(magic))
			continue;
		if (magic != FS_UFS2_MAGIC && magic != FS_UFS2EA_MAGIC &&
		    magic != FS_UFS1_MAGIC)
			continue;

		/* Confirm by checking that fs_sblockloc records this offset,
		 * to avoid false positives from stray magic-like bytes.
		 */
		if (lseek(fd, off + FFS_SBLOCKLOC_OFFSET, SEEK_SET) < 0)
			continue;
		if (read(fd, &sblockloc, sizeof(sblockloc)) != sizeof(sblockloc))
			continue;
		if (sblockloc != off)
			continue;

		return 1;
	}
	return 0;
}

int fsversion(char *dev, char *prog)
{
	int result = -1, fd;

	if ((fd = open(dev, O_RDONLY)) < 0) {
		std_err(prog);
		std_err(" cannot open ");
		perror(dev);
		return(-1);
	}

	lseek(fd, (off_t) SUPER_BLOCK_BYTES, SEEK_SET);	/* skip boot block */
	if (read(fd, (char *) &super, sizeof(super)) != sizeof(super)) {
		std_err(prog);
		std_err(" cannot read super block on ");
		perror(dev);
		close(fd);
		return(-1);
	}

	/* first check MFS, a valid MFS may look like EXT but not vice versa */
	if (check_super(MAGIC_OFFSET_MFS, SUPER_MAGIC)) {
		result = FSVERSION_MFS1;
		goto done;
	}
	else if (check_super(MAGIC_OFFSET_MFS, SUPER_V2)) {
		result = FSVERSION_MFS2;
		goto done;
	}
	else if (check_super(MAGIC_OFFSET_MFS, SUPER_V3)) {
		result = FSVERSION_MFS3;
		goto done;
	}

	/* check ext2 */
	if (check_super(MAGIC_OFFSET_EXT, MAGIC_VALUE_EXT2)) {
		result = FSVERSION_EXT2;
		goto done;
	}

	/* check UFS2/FFS */
	if (check_ffs(fd)) {
		result = FSVERSION_FFS;
		goto done;
	}

	/* check ISO 9660 */
	lseek(fd, (off_t) MAGIC_OFFSET_ISO9660, SEEK_SET);
	if (read(fd, (char *) &super, sizeof(super)) == sizeof(super)) {
		if (memcmp(super+1, "CD001", 5) == 0) {
			result = FSVERSION_ISO9660;
			goto done;
		}
	}

done:
	close(fd);
	return result;
}
