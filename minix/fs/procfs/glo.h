#ifndef _PROCFS_GLO_H
#define _PROCFS_GLO_H

#include <minix/param.h>

/* pid.c */
extern struct file pid_files[];

/* root.c */
extern struct file root_files[];

/* tree.c */
extern struct proc proc[NR_PROCS + NR_TASKS];	/* process table from kernel */
extern struct mproc mproc[NR_PROCS];		/* process table from PM */
extern struct fproc fproc[NR_PROCS];		/* process table from VFS */

#endif /* _PROCFS_GLO_H */
