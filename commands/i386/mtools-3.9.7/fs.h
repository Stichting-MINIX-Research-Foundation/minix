#ifndef MTOOLS_FS_H
#define MTOOLS_FS_H

#include "stream.h"


typedef struct FsPublic_t {
	Class_t *Class;
	int refs;
	Stream_t *Next;
	Stream_t *Buffer;

	int serialized;
	unsigned long serial_number;
	int cluster_size;
	unsigned int sector_size;
} FsPublic_t;

Stream_t *fs_init(char *drive, int mode);
int fat_free(Stream_t *Dir, unsigned int fat);
int fatFreeWithDir(Stream_t *Dir, struct directory *dir);
int fat_error(Stream_t *Dir);
int fat32RootCluster(Stream_t *Dir);
char *getDrive(Stream_t *Stream);

#endif
