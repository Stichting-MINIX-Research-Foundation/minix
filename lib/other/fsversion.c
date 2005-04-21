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

#include "fs/const.h"
#include "fs/type.h"
#include "fs/super.h"

static struct super_block super, *sp;

int fsversion(dev, prog)
char *dev, *prog;
{
  int fd;

  if ((fd = open(dev, O_RDONLY)) < 0) {
	std_err(prog);
	std_err(" cannot open ");
	perror(dev);
	return(-1);
  }

  lseek(fd, (off_t) SUPER_BLOCK_BYTES, SEEK_SET);	/* skip boot block */
  if (read(fd, (char *) &super, (unsigned) SUPER_SIZE) != SUPER_SIZE) {
	std_err(prog);
	std_err(" cannot read super block on ");
	perror(dev);
	close(fd);
	return(-1);
  }
  close(fd);
  sp = &super;
  if (sp->s_magic == SUPER_MAGIC) return(1);
  if (sp->s_magic == SUPER_V2) return(2);
  if (sp->s_magic == SUPER_V3) return(3);
  return(-1);
}
