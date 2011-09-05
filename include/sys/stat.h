/* The <sys/stat.h> header defines a struct that is used in the stat() and
 * fstat functions.  The information in this struct comes from the i-node of
 * some file.  These calls are the only approved way to inspect i-nodes.
 */

#ifndef _STAT_H
#define _STAT_H

#ifndef _TYPES_H
#include <minix/types.h>
#endif


struct stat {
  big_dev_t     st_dev;               /* inode's device */
  big_mode_t    st_mode;              /* inode protection mode */
  ino_t         st_ino;               /* inode's number */
  u32_t         padding;              /* inode's padding */
  big_nlink_t   st_nlink;             /* number of hard links */
  big_uid_t     st_uid;               /* user ID of the file's owner */
  big_gid_t     st_gid;               /* group ID of the file's group */
  big_dev_t     st_rdev;              /* device type */
#if defined(_NETBSD_SOURCE)
  struct    timespec st_atimespec;/* time of last access */
  struct    timespec st_mtimespec;/* time of last data modification */
  struct    timespec st_ctimespec;/* time of last file status change */
  struct    timespec st_birthtimespec; /* time of creation */
#else
  time_t    st_atime;             /* time of last access */
  long      st_atimensec;         /* nsec of last access */
  time_t    st_mtime;             /* time of last data modification */
  long      st_mtimensec;         /* nsec of last data modification */
  time_t    st_ctime;             /* time of last file status change */
  long      st_ctimensec;         /* nsec of last file status change */
  time_t    st_birthtime;         /* time of creation */
  long      st_birthtimensec;     /* nsec of time of creation */
#endif
  off_t st_size;		/* file size, in bytes */
  u32_t     padding2;		/* size padding */
  blkcnt_t  st_blocks;		/* blocks allocated for file */
  u32_t     padding3;		/* blocks padding */
  blksize_t st_blksize;		/* optimal blocksize for I/O */
  u32_t     st_flags;		/* user defined flags for file */
  u32_t     st_gen;		/* file generation number */
  u32_t     st_spare[2];
};


struct minix_prev_stat {
  short st_dev;			/* major/minor device number */
  ino_t st_ino;			/* i-node number */
  mode_t st_mode;		/* file mode, protection bits, etc. */
  nlink_t st_nlink;		/* # links; */
  short st_uid;			/* uid of the file's owner */
  short int st_gid;		/* gid; TEMPORARY HACK: should be gid_t */
  short st_rdev;
  off_t st_size;		/* file size */
  time_t st_atime;		/* time of last access */
  time_t st_mtime;		/* time of last data modification */
  time_t st_ctime;		/* time of last file status change */
};


/* Copy field by field because of st_gid type mismath and
 * difference in order after atime.
 */
#define COPY_PREV_STAT_TO_NEW(dest, src)\
	(dest)->st_dev = (src)->st_dev;\
	(dest)->st_ino = (src)->st_ino;\
	(dest)->st_mode = (src)->st_mode;\
	(dest)->st_nlink = (src)->st_nlink;\
	(dest)->st_uid = (src)->st_uid;\
	(dest)->st_gid = (src)->st_gid;\
	(dest)->st_rdev = (src)->st_rdev;\
	(dest)->st_size = (src)->st_size;\
	(dest)->st_atime = (src)->st_atime;\
	(dest)->st_mtime = (src)->st_mtime;\
	(dest)->st_ctime = (src)->st_ctime

#define S_BLKSIZE	512		/* block size used in the stat struct */

/* Traditional mask definitions for st_mode. */
#define S_IFMT   0170000	/* type of file */
#define S_IFSOCK 0140000        /* socket */
#define S_IFLNK  0120000	/* symbolic link */
#define S_IFREG  0100000	/* regular */
#define S_IFBLK  0060000	/* block special */
#define S_IFDIR  0040000	/* directory */
#define S_IFCHR  0020000	/* character special */
#define S_IFIFO  0010000	/* this is a FIFO */
#define S_ISUID  0004000	/* set user id on execution */
#define S_ISGID  0002000	/* set group id on execution */
#define S_ISVTX  0001000	/* save swapped text even after use */

/* POSIX masks for st_mode. */
#define S_IRWXU   00700		/* owner:  rwx------ */
#define S_IRUSR   00400		/* owner:  r-------- */
#define S_IWUSR   00200		/* owner:  -w------- */
#define S_IXUSR   00100		/* owner:  --x------ */

#define S_IRWXG   00070		/* group:  ---rwx--- */
#define S_IRGRP   00040		/* group:  ---r----- */
#define S_IWGRP   00020		/* group:  ----w---- */
#define S_IXGRP   00010		/* group:  -----x--- */

#define S_IRWXO   00007		/* others: ------rwx */
#define S_IROTH   00004		/* others: ------r-- */ 
#define S_IWOTH   00002		/* others: -------w- */
#define S_IXOTH   00001		/* others: --------x */

/* Synonyms for above. */
#define S_IEXEC		S_IXUSR
#define S_IWRITE	S_IWUSR
#define S_IREAD		S_IRUSR

/* The following macros test st_mode (from POSIX Sec. 5.6.1.1). */
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)	/* is a reg file */
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)	/* is a directory */
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)	/* is a char spec */
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)	/* is a block spec */
#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)	/* is a symlink */
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)	/* is a pipe/FIFO */
#define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)	/* is a socket */

#define DEFFILEMODE     (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#define ALLPERMS        (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)

/* Function Prototypes. */
_PROTOTYPE( int chmod, (const char *_path, mode_t _mode)		);
_PROTOTYPE( int fchmod, (int fd, mode_t _mode)			);
_PROTOTYPE( int fstat, (int _fildes, struct stat *_buf)			);
_PROTOTYPE( int mkdir, (const char *_path, mode_t _mode)		);
_PROTOTYPE( int mkfifo, (const char *_path, mode_t _mode)		);
_PROTOTYPE( int stat, (const char *_path, struct stat *_buf)		);
_PROTOTYPE( mode_t umask, (mode_t _cmask)				);

/* Open Group Base Specifications Issue 6 (not complete) */
_PROTOTYPE( int lstat, (const char *_path, struct stat *_buf)		);

#endif /* _STAT_H */
