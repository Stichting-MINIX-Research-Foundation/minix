/* The <dir.h> header gives the layout of a directory. */

#ifndef _DIR_H
#define _DIR_H

#include <sys/types.h>

#define	DIRBLKSIZ	512	/* size of directory block */

#ifndef DIRSIZ
#define DIRSIZ	60
#endif

struct direct {
  ino_t d_ino;
  char d_name[DIRSIZ];
};

#endif /* _DIR_H */
