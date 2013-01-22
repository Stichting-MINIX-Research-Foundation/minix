/*	$NetBSD: exf.h,v 1.1.1.2 2008/05/18 14:29:45 aymeric Exp $ */

/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	Id: exf.h,v 10.19 2002/03/02 23:36:23 skimo Exp (Berkeley) Date: 2002/03/02 23:36:23
 */
					/* Undo direction. */
/*
 * exf --
 *	The file structure.
 */
struct _exf {
	CIRCLEQ_ENTRY(_exf) q;		/* Linked list of file structures. */
	int	 refcnt;		/* Reference count. */

	CIRCLEQ_HEAD(_escrh, _scr)   scrq;   /* Attached screens */
					/* Underlying database state. */
	DB_ENV	*env;			/* The DB environment. */
	char	*env_path;		/* DB environment directory. */
	DB	*db;			/* File db structure. */
	db_recno_t	 c_nlines;	/* Cached lines in the file. */

	DB	*log;			/* Log db structure. */
	db_recno_t	 l_high;	/* Log last + 1 record number. */
	db_recno_t	 l_cur;		/* Log current record number. */
#ifdef USE_DB4_LOGGING
	DB_LSN	lsn_first;
	DB_LSN	lsn_high;		/* LSN of last record. */
	DB_LSN	lsn_cur;		/* LSN of first record to undo. */
#endif
	MARK	 l_cursor;		/* Log cursor position. */
	dir_t	 lundo;			/* Last undo direction. */
	WIN	*l_win;			/* Window owning transaction. */

	LIST_HEAD(_markh, _lmark) marks;/* Linked list of file MARK's. */

	/*
	 * XXX
	 * Mtime should be a struct timespec, but time_t is more portable.
	 */
	dev_t	 mdev;			/* Device. */
	ino_t	 minode;		/* Inode. */
	time_t	 mtime;			/* Last modification time. */

	int	 fcntl_fd;		/* Fcntl locking fd; see exf.c. */
	int	 fd;			/* File descriptor */

	/*
	 * Recovery in general, and these fields specifically, are described
	 * in recover.c.
	 */
#define	RCV_PERIOD	120		/* Sync every two minutes. */
	char	*rcv_path;		/* Recover file name. */
	char	*rcv_mpath;		/* Recover mail file name. */
	int	 rcv_fd;		/* Locked mail file descriptor. */

	void	*lock;			/* Lock for log. */

#define	F_DEVSET	0x001		/* mdev/minode fields initialized. */
#define	F_FIRSTMODIFY	0x002		/* File not yet modified. */
#define	F_MODIFIED	0x004		/* File is currently dirty. */
#define	F_MULTILOCK	0x008		/* Multiple processes running, lock. */
#define	F_NOLOG		0x010		/* Logging turned off. */
#define	F_RCV_NORM	0x020		/* Don't delete recovery files. */
#define	F_RCV_ON	0x040		/* Recovery is possible. */
#define	F_UNDO		0x080		/* No change since last undo. */
	u_int8_t flags;
};

/* Flags to db_get(). */
#define	DBG_FATAL	0x001	/* If DNE, error message. */
#define	DBG_NOCACHE	0x002	/* Ignore the front-end cache. */

/* Flags to file_init() and file_write(). */
#define	FS_ALL		0x001	/* Write the entire file. */
#define	FS_APPEND	0x002	/* Append to the file. */
#define	FS_FORCE	0x004	/* Force is set. */
#define	FS_OPENERR	0x008	/* Open failed, try it again. */
#define	FS_POSSIBLE	0x010	/* Force could have been set. */
#define	FS_SETALT	0x020	/* Set alternate file name. */

/* Flags to rcv_sync(). */
#define	RCV_EMAIL	0x01	/* Send the user email, IFF file modified. */
#define	RCV_ENDSESSION	0x02	/* End the file session. */
#define	RCV_PRESERVE	0x04	/* Preserve backup file, IFF file modified. */
#define	RCV_SNAPSHOT	0x08	/* Snapshot the recovery, and send email. */
