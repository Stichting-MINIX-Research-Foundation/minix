/*	$NetBSD: dirent_private.h,v 1.4 2010/09/26 02:26:59 yamt Exp $	*/

/*
 * One struct _dirpos is malloced to describe the current directory
 * position each time telldir is called. It records the current magic 
 * cookie returned by getdents and the offset within the buffer associated
 * with that return value.
 */
struct dirpos {
	struct dirpos *dp_next;	/* next structure in list */
	off_t	dp_seek;	/* magic cookie returned by getdents */
	long	dp_loc;		/* offset of entry in buffer */
};

struct _dirdesc;
void _seekdir_unlocked(struct _dirdesc *, long);
long _telldir_unlocked(struct _dirdesc *);
int _initdir(DIR *, int, const char *);
void _finidir(DIR *);
#ifndef __LIBC12_SOURCE__
struct dirent;
struct dirent *_readdir_unlocked(struct _dirdesc *, int)
    __RENAME(___readdir_unlocked50);
#endif
