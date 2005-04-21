#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"
#include "vfat.h"
#include "file.h"
#include "buffer.h"

/*
 * Find the directory and load a new dir_chain[].  A null directory
 * is OK.  Returns a 1 on error.
 */


void bufferize(Stream_t **Dir)
{
	Stream_t *BDir;

	if(!*Dir)
		return;
	BDir = buf_init(*Dir, 64*16384, 512, MDIR_SIZE);
	if(!BDir){
		FREE(Dir);
		*Dir = NULL;
	} else
		*Dir = BDir;
}
