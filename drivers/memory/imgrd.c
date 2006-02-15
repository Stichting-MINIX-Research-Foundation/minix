/*
Ramdisk that is part of the image
*/

#include <stddef.h>

#include "local.h"

unsigned char imgrd[]=
{
#include "ramdisk/image.c"
};

size_t imgrd_size= sizeof(imgrd);
