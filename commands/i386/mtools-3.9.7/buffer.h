#ifndef MTOOLS_BUFFER_H
#define MTOOLS_BUFFER_H

#include "stream.h"

Stream_t *buf_init(Stream_t *Next, 
		   int size, 
		   int cylinderSize,
		   int sectorSize);

#endif
