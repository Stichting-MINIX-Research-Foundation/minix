#ifndef _SYS_DIRENT_H_
#define _SYS_DIRENT_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>
#include <minix/dirent.h>

/*
 * The dirent structure defines the format of directory entries returned by
 * the getdents(2) system call.
 */

struct dirent {		/* Largest entry (8 slots) */
	ino_t		d_ino;		/* I-node number */
	off_t 		d_off;		/* Offset in directory */
	unsigned short	d_reclen;	/* Length of this record */
	char		d_name[1];	/* Null terminated name */
};

#if defined(_NETBSD_SOURCE)
#define MAXNAMLEN	511
#define	d_fileno	d_ino
#endif


/*
 * The _DIRENT_ALIGN macro returns the alignment of struct dirent.  It
 * is used to check for bogus pointers and to calculate in advance the
 * memory required to store a dirent.
 * Unfortunately Minix doesn't use any standard alignment in dirents
 * at the moment, so, in order to calculate a safe dirent size, we add
 * an arbitrary number of bytes to the structure (_DIRENT_PAD), and we
 * set _DIRENT_ALIGN to zero to pass the pointers checks.
 * Please, FIXME.
 */  
#define _DIRENT_ALIGN(dp) 0
#define _DIRENT_PAD 64
/*
 * The _DIRENT_NAMEOFF macro returns the offset of the d_name field in 
 * struct dirent
 */
#define _DIRENT_NAMEOFF(dp) \
    ((char *)(void *)&(dp)->d_name - (char *)(void *)dp)
/*
 * The _DIRENT_RECLEN macro gives the minimum record length which will hold
 * a name of size "namlen".
 */
#define _DIRENT_RECLEN(dp, namlen) \
    ((_DIRENT_NAMEOFF(dp) + (namlen) + 1 + _DIRENT_PAD + _DIRENT_ALIGN(dp)) & \
    ~_DIRENT_ALIGN(dp))
/*
 * The _DIRENT_SIZE macro returns the minimum record length required for
 * name name stored in the current record.
 */
#define	_DIRENT_SIZE(dp) _DIRENT_RECLEN(dp, strlen(dp->d_name))
/*
 * The _DIRENT_NEXT macro advances to the next dirent record.
 */
#define _DIRENT_NEXT(dp) ((void *)((char *)(void *)(dp) + (dp)->d_reclen))
/*
 * The _DIRENT_MINSIZE returns the size of an empty (invalid) record.
 */
#define _DIRENT_MINSIZE(dp) _DIRENT_RECLEN(dp, 0)

#endif	/* !_SYS_DIRENT_H_ */
