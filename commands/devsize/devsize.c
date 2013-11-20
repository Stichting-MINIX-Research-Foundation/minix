/* Ben Gras
 *
 * Based on sizeup() in mkfs.c.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <machine/partition.h>
#include <minix/partition.h>
#include <sys/ioc_disk.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

unsigned long sizeup(char *);

int main(int argc, char *argv[])
{
  if(argc != 2) {
	fprintf(stderr, "Usage: %s <device>\n", argv[0]);
	return 1;
  }

  printf("%lu\n", sizeup(argv[1]));
  return 0;
}	


unsigned long sizeup(char *device)
{
  int fd;
  struct part_geom entry;
  unsigned long d;

  if ((fd = open(device, O_RDONLY)) == -1) {
  	perror("sizeup open");
  	exit(1);
  }
  if (ioctl(fd, DIOCGETP, &entry) == -1) {
  	perror("sizeup ioctl");
  	exit(1);
  }
  close(fd);
  d = entry.size / 512;
  return d;
}
