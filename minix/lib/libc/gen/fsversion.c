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

static int check_super(off_t offset, unsigned short magic)
{
	return (memcmp(super + offset, &magic, sizeof(magic)) == 0) ? 1 : 0;
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
