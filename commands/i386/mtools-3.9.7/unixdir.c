#include "sysincludes.h"
#include "msdos.h"
#include "stream.h"
#include "mtools.h"
#include "fsP.h"
#include "file.h"
#include "htable.h"
#include "mainloop.h"
#include <dirent.h>

typedef struct Dir_t {
	Class_t *Class;
	int refs;
	Stream_t *Next;
	Stream_t *Buffer;

	struct stat stat;
	char *pathname;
	DIR *dir;
#ifdef HAVE_FCHDIR
	int fd;
#endif
} Dir_t;

/*#define FCHDIR_MODE*/

static int get_dir_data(Stream_t *Stream, time_t *date, mt_size_t *size,
			int *type, int *address)
{
	DeclareThis(Dir_t);

	if(date)
		*date = This->stat.st_mtime;
	if(size)
		*size = (mt_size_t) This->stat.st_size;
	if(type)
		*type = 1;
	if(address)
		*address = 0;
	return 0;
}

static int dir_free(Stream_t *Stream)
{
	DeclareThis(Dir_t);

	Free(This->pathname);
	closedir(This->dir);
	return 0;
}

static Class_t DirClass = { 
	0, /* read */
	0, /* write */
	0, /* flush */
	dir_free, /* free */
	0, /* get_geom */
	get_dir_data ,
	0 /* pre-allocate */
};

#ifdef HAVE_FCHDIR
#define FCHDIR_MODE
#endif

int unix_dir_loop(Stream_t *Stream, MainParam_t *mp); 
int unix_loop(Stream_t *Stream, MainParam_t *mp, char *arg, 
	      int follow_dir_link);

int unix_dir_loop(Stream_t *Stream, MainParam_t *mp)
{
	DeclareThis(Dir_t);
	struct dirent *entry;
	char *newName;
	int ret=0;

#ifdef FCHDIR_MODE
	int fd;

	fd = open(".", O_RDONLY);
	chdir(This->pathname);
#endif
	while((entry=readdir(This->dir)) != NULL) {
		if(got_signal)
			break;
		if(isSpecial(entry->d_name))
			continue;
#ifndef FCHDIR_MODE
		newName = malloc(strlen(This->pathname) + 1 + 
				 strlen(entry->d_name) + 1);
		if(!newName) {
			ret = ERROR_ONE;
			break;
		}
		strcpy(newName, This->pathname);
		strcat(newName, "/");
		strcat(newName, entry->d_name);
#else
		newName = entry->d_name;
#endif
		ret |= unix_loop(Stream, mp, newName, 0);
#ifndef FCHDIR_MODE
		free(newName);
#endif
	}
#ifdef FCHDIR_MODE
	fchdir(fd);
	close(fd);
#endif
	return ret;
}

Stream_t *OpenDir(Stream_t *Stream, const char *filename)
{
	Dir_t *This;

	This = New(Dir_t);
	
	This->Class = &DirClass;
	This->Next = 0;
	This->refs = 1;
	This->Buffer = 0;
	This->pathname = malloc(strlen(filename)+1);
	if(This->pathname == NULL) {
		Free(This);
		return NULL;
	}
	strcpy(This->pathname, filename);

	if(stat(filename, &This->stat) < 0) {
		Free(This->pathname);
		Free(This);
		return NULL;
	}

	This->dir = opendir(filename);
	if(!This->dir) {
		Free(This->pathname);
		Free(This);
		return NULL;
	}

	return (Stream_t *) This;
}
