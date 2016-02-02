#ifndef _PROCFS_GLO_H
#define _PROCFS_GLO_H

/* pid.c */
extern struct file pid_files[];

/* root.c */
extern struct file root_files[];

/* tree.c */
extern struct minix_proc_list proc_list[NR_PROCS];

#endif /* _PROCFS_GLO_H */
