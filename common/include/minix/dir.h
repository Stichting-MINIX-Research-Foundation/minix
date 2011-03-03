/* The <dir.h> header gives the layout of a directory. */

#ifndef _DIR_H
#define _DIR_H

#ifdef __NBSD_LIBC
#include <sys/cdefs.h>
#endif
#include <minix/types.h>

#define	DIRBLKSIZ	512	/* size of directory block */

#ifndef DIRSIZ
#define DIRSIZ	60
#endif

struct direct {
  ino_t d_ino;
  char d_name[DIRSIZ];
#ifdef __NBSD_LIBC 
} __packed;
#else
};
#endif

#endif /* _DIR_H */
