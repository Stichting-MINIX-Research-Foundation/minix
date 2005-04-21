#ifndef MTOOLS_DIRENTRY_H
#define MTOOLS_DIRENTRY_H

#include "sysincludes.h"
#include "vfat.h"

typedef struct direntry_t {
	struct Stream_t *Dir;
	/* struct direntry_t *parent; parent level */	
	int entry; /* slot in parent directory (-3 if root) */
	struct directory dir; /* descriptor in parent directory (random if 
			       * root)*/
	char name[MAX_VNAMELEN+1]; /* name in its parent directory, or 
				    * NULL if root */
	int beginSlot; /* begin and end slot, for delete */
	int endSlot;
} direntry_t;

#include "stream.h"

int vfat_lookup(direntry_t *entry, const char *filename, int length,
		int flags, char *shortname, char *longname);

struct directory *dir_read(direntry_t *entry, int *error);

void initializeDirentry(direntry_t *entry, struct Stream_t *Dir);
int isNotFound(direntry_t *entry);
direntry_t *getParent(direntry_t *entry);
void dir_write(direntry_t *entry);
void low_level_dir_write(direntry_t *entry);
int fatFreeWithDirentry(direntry_t *entry);
int labelit(char *dosname,
	    char *longname,
	    void *arg0,
	    direntry_t *entry);
int isSubdirOf(Stream_t *inside, Stream_t *outside);
char *getPwd(direntry_t *entry);
void fprintPwd(FILE *f, direntry_t *entry, int escape);
int write_vfat(Stream_t *, char *, char *, int, direntry_t *);
#endif
