#ifndef _SFFS_INC_H
#define _SFFS_INC_H

#include <minix/drivers.h>
#include <minix/fsdriver.h>
#include <minix/vfsif.h>
#include <minix/optset.h>
#include <minix/sffs.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <assert.h>

#if DEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#include "const.h"
#include "proto.h"
#include "glo.h"
#include "inode.h"

#endif /* _SFFS_INC_H */
