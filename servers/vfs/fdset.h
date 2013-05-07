#ifndef _FDSET_H
#define _FDSET_H 1

#include "const.h"
#ifdef FD_SETSIZE
#error FD_SETSIZE already set
#endif
#define FD_SETSIZE      FDS_PER_PROCESS
#include <sys/fd_set.h>

#endif
