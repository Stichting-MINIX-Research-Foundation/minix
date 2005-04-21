#ifndef MTOOLS_XDFIO_H
#define MTOOLS_XDFIO_H

#include "msdos.h"
#include "stream.h"

struct xdf_info {
  int FatSize;
  int RootDirSize;
  int BadSectors;
};

Stream_t *XdfOpen(struct device *dev, char *name,
		  int mode, char *errmsg, struct xdf_info *info);

#endif
