#ifndef MTOOLS_FILE_H
#define MTOOLS_FILE_H

#include "stream.h"
#include "mtoolsDirent.h"

Stream_t *OpenFileByDirentry(direntry_t *entry);
Stream_t *OpenRoot(Stream_t *Dir);
void printFat(Stream_t *Stream);
direntry_t *getDirentry(Stream_t *Stream);
#endif
