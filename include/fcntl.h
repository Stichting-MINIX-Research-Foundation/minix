/* The <fcntl.h> header is needed by the open() and fcntl() system calls,
 * which  have a variety of parameters and flags.  They are described here.  
 * The formats of the calls to each of these are:
 *
 *	open(path, oflag [,mode])	open a file
 *	fcntl(fd, cmd [,arg])		get or set file attributes
 * 
 */

#ifndef _FCNTL_H
#define _FCNTL_H

#ifndef _TYPES_H
#include <sys/types.h>
#endif

/* These values are used for cmd in fcntl().  POSIX Table 6-1.  */
#define F_DUPFD            0	/* duplicate file descriptor */
#define F_GETFD	           1	/* get file descriptor flags */
#define F_SETFD            2	/* set file descriptor flags */
#define F_GETFL            3	/* get file status flags */
#define F_SETFL            4	/* set file status flags */
#define F_GETLK            5	/* get record locking information */
#define F_SETLK            6	/* set record locking information */
#define F_SETLKW           7	/* set record locking info; wait if blocked */

/* File descriptor flags used for fcntl().  POSIX Table 6-2. */
#define FD_CLOEXEC         1	/* close on exec flag for third arg of fcntl */

/* L_type values for record locking with fcntl().  POSIX Table 6-3. */
#define F_RDLCK            1	/* shared or read lock */
#define F_WRLCK            2	/* exclusive or write lock */
#define F_UNLCK            3	/* unlock */

/* Oflag values for open().  POSIX Table 6-4. */
#define O_CREAT        00100	/* creat file if it doesn't exist */
#define O_EXCL         00200	/* exclusive use flag */
#define O_NOCTTY       00400	/* do not assign a controlling terminal */
#define O_TRUNC        01000	/* truncate flag */

/* File status flags for open() and fcntl().  POSIX Table 6-5. */
#define O_APPEND       02000	/* set append mode */
#define O_NONBLOCK     04000	/* no delay */

/* File access modes for open() and fcntl().  POSIX Table 6-6. */
#define O_RDONLY           0	/* open(name, O_RDONLY) opens read only */
#define O_WRONLY           1	/* open(name, O_WRONLY) opens write only */
#define O_RDWR             2	/* open(name, O_RDWR) opens read/write */

/* Mask for use with file access modes.  POSIX Table 6-7. */
#define O_ACCMODE         03	/* mask for file access modes */

/* Struct used for locking.  POSIX Table 6-8. */
struct flock {
  short l_type;			/* type: F_RDLCK, F_WRLCK, or F_UNLCK */
  short l_whence;		/* flag for starting offset */
  off_t l_start;		/* relative offset in bytes */
  off_t l_len;			/* size; if 0, then until EOF */
  pid_t l_pid;			/* process id of the locks' owner */
};


/* Function Prototypes. */
_PROTOTYPE( int creat, (const char *_path, Mode_t _mode)		);
_PROTOTYPE( int fcntl, (int _filedes, int _cmd, ...)	  		);
_PROTOTYPE( int open,  (const char *_path, int _oflag, ...) 		);

#endif /* _FCNTL_H */
