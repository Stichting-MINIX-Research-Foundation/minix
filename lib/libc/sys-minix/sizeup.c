#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>
#include <minix/u64.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/minlib.h>
#include <minix/partition.h>
#include <sys/ioc_disk.h>

#include <unistd.h>

/*================================================================
 *                    minix_sizeup  -  determine device size
 *===============================================================*/
int minix_sizeup(device, bytes)
char *device;
u64_t *bytes;
{
  int fd;
  struct partition entry;
  struct stat st;

  if ((fd = open(device, O_RDONLY)) == -1) {
        if (errno != ENOENT)
                perror("sizeup open");
        return -1;
  }
  if (ioctl(fd, DIOCGETP, &entry) == -1) {
        perror("sizeup ioctl");
        if(fstat(fd, &st) < 0) {
                perror("fstat");
                entry.size = cvu64(0);
        } else {
                entry.size = cvu64(st.st_size);
        }
  }
  close(fd);
  *bytes = entry.size;
  return 0;
}
