/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include <minix/drivers.h>
#include <minix/sffs.h>
#include <minix/hgfs.h>

#define PREFIX(x) __libhgfs_##x

#include "type.h"
#include "const.h"
#include "proto.h"
#include "glo.h"
