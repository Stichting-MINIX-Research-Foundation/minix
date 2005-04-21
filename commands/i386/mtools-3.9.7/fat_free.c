#include "sysincludes.h"
#include "msdos.h"
#include "fsP.h"
#include "mtoolsDirent.h"

/*
 * Remove a string of FAT entries (delete the file).  The argument is
 * the beginning of the string.  Does not consider the file length, so
 * if FAT is corrupted, watch out!
 */

int fat_free(Stream_t *Dir, unsigned int fat)
{
	Stream_t *Stream = GetFs(Dir);
	DeclareThis(Fs_t);
	unsigned int next_no_step;
					/* a zero length file? */
	if (fat == 0)
		return(0);

	/* CONSTCOND */
	while (!This->fat_error) {
		/* get next cluster number */
		next_no_step = fatDecode(This,fat);
		/* mark current cluster as empty */
		fatDeallocate(This,fat);
		if (next_no_step >= This->last_fat)
			break;
		fat = next_no_step;
	}
	return(0);
}

int fatFreeWithDir(Stream_t *Dir, struct directory *dir)
{
	unsigned int first;

	if((!strncmp(dir->name,".      ",8) ||
	    !strncmp(dir->name,"..     ",8)) &&
	   !strncmp(dir->ext,"   ",3)) {
		fprintf(stderr,"Trying to remove . or .. entry\n");
		return -1;
	}

	first = START(dir);
  	if(fat32RootCluster(Dir))
		first |= STARTHI(dir) << 16;
	return fat_free(Dir, first);
}

int fatFreeWithDirentry(direntry_t *entry)
{
	return fatFreeWithDir(entry->Dir, &entry->dir);
}
    
