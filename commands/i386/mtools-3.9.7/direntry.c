#include "sysincludes.h"
#include "msdos.h"
#include "stream.h"
#include "file.h"
#include "mtoolsDirent.h"

void initializeDirentry(direntry_t *entry, Stream_t *Dir)
{
	entry->entry = -1;
/*	entry->parent = getDirentry(Dir);*/
	entry->Dir = Dir;
	entry->beginSlot = 0;
	entry->endSlot = 0;
}

int isNotFound(direntry_t *entry)
{
	return entry->entry == -2;
}

void rewindEntry(direntry_t *entry)
{
	entry->entry = -1;
}


direntry_t *getParent(direntry_t *entry)
{
	return getDirentry(entry->Dir);
}


static int getPathLen(direntry_t *entry)
{
	int length=0;

	while(1) {
		if(entry->entry == -3) /* rootDir */
			return strlen(getDrive(entry->Dir)) + 1 + length + 1;
		
		length += 1 + strlen(entry->name);
		entry = getDirentry(entry->Dir);
	}
}

static char *sprintPwd(direntry_t *entry, char *ptr)
{
	if(entry->entry == -3) {
		strcpy(ptr, getDrive(entry->Dir));
		strcat(ptr, ":/");
		ptr = strchr(ptr, 0);
	} else {
		ptr = sprintPwd(getDirentry(entry->Dir), ptr);
		if(ptr[-1] != '/')
			*ptr++ = '/';
		strcpy(ptr, entry->name);
		ptr += strlen(entry->name);
	}
	return ptr;		
}


#define NEED_ESCAPE "\"$\\"

static void _fprintPwd(FILE *f, direntry_t *entry, int recurs, int escape)
{
	if(entry->entry == -3) {
		fputs(getDrive(entry->Dir), f);
		putc(':', f);
		if(!recurs)
			putc('/', f);
	} else {
		_fprintPwd(f, getDirentry(entry->Dir), 1, escape);
		if (escape && strpbrk(entry->name, NEED_ESCAPE)) {
			char *ptr;
			for(ptr = entry->name; *ptr; ptr++) {
				if (strchr(NEED_ESCAPE, *ptr))
					putc('\\', f);
				putc(*ptr, f);
			}
		} else {
			fprintf(f, "/%s", entry->name);
		}
	}
}

void fprintPwd(FILE *f, direntry_t *entry, int escape)
{
	if (escape)
		putc('"', f);
	_fprintPwd(f, entry, 0, escape);
	if(escape)
		putc('"', f);
}

char *getPwd(direntry_t *entry)
{
	int size;
	char *ret;

	size = getPathLen(entry);
	ret = malloc(size+1);
	if(!ret)
		return 0;
	sprintPwd(entry, ret);
	return ret;
}

int isSubdirOf(Stream_t *inside, Stream_t *outside)
{
	while(1) {
		if(inside == outside) /* both are the same */
			return 1;
		if(getDirentry(inside)->entry == -3) /* root directory */
			return 0;
		/* look further up */
		inside = getDirentry(inside)->Dir;
	}			
}
