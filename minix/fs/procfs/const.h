#ifndef _PROCFS_CONST_H
#define _PROCFS_CONST_H

/*
 * The minimum number of inodes depends on a number of factors:
 * - Each statically created inode (e.g., /proc/hz) needs an inode.  As of
 *   writing, this requires about a dozen inodes.
 * - Deleted inodes that are still in use by VFS must be retained.  For deleted
 *   directories, all their containing directories up to the root must be
 *   retained as well (to allow the user to "cd .." out).  VTreeFS already
 *   takes care of this.  In the case of ProcFS, only PID-based directories can
 *   be deleted; no other directories are dynamically created.  These
 *   directories currently do not contain subdirectories, either.  Hence, for
 *   deleted open inodes, we need to reserve at most NR_VNODES inodes in the
 *   worst case.
 * - In order for getdents to be able to return all PID-based directories,
 *   inodes must not be recycled while generating the list of these PID-based
 *   directories.  In the worst case, this means (NR_TASKS + NR_PROCS) extra
 *   inodes.
 * The sum of these is the bare minimum for correct operation in all possible
 * circumstances.  In practice, not all open files will be deleted files in
 * ProcFS, and not all process slots will be in use either, so the average use
 * will be a lot less.  However, setting the value too low allows for a
 * potential denial-of-service attack by a non-root user.
 *
 * For the moment, we simply set this value to something reasonable.
 */
#define NR_INODES	((NR_TASKS + NR_PROCS) * 4)

/* Various file modes. */
#define REG_ALL_MODE	(S_IFREG | 0444)	/* world-readable regular */
#define DIR_ALL_MODE	(S_IFDIR | 0555)	/* world-accessible directory */
#define LNK_ALL_MODE	(S_IFLNK | 0777)	/* symbolic link */

/* Size of the I/O buffer. */
#define BUF_SIZE	4097			/* 4KB+1 (see buf.c) */

#endif /* _PROCFS_CONST_H */
