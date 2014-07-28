#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <fcntl.h>
#include <unistd.h>

int dup(fd)
int fd;
{
  return(fcntl(fd, F_DUPFD, 0));
}
