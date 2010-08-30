/* Function prototypes. */

#include "timers.h"
#include "request.h"
#include <minix/rs.h>

/* Structs used in prototypes must be declared as such first. */
struct filp;		
struct fproc;
struct vmnt;
struct vnode;

typedef struct filp * filp_id_t;

/* device.c */
_PROTOTYPE( int dev_open, (dev_t dev, int proc, int flags)		);
_PROTOTYPE( int dev_reopen, (dev_t dev, int filp_no, int flags)		);
_PROTOTYPE( int dev_close, (dev_t dev, int filp_no)			);
_PROTOTYPE( int dev_io, (int op, dev_t dev, int proc, void *buf,
		u64_t pos, int bytes, int flags, int suspend_reopen)	);
_PROTOTYPE( int gen_opcl, (int op, dev_t dev, int proc, int flags)	);
_PROTOTYPE( int gen_io, (int task_nr, message *mess_ptr)		);
_PROTOTYPE( int asyn_io, (int task_nr, message *mess_ptr)		);
_PROTOTYPE( int no_dev, (int op, dev_t dev, int proc, int flags)	);
_PROTOTYPE( int no_dev_io, (int, message *)				);
_PROTOTYPE( int tty_opcl, (int op, dev_t dev, int proc, int flags)	);
_PROTOTYPE( int ctty_opcl, (int op, dev_t dev, int proc, int flags)	);
_PROTOTYPE( int clone_opcl, (int op, dev_t dev, int proc, int flags)	);
_PROTOTYPE( int ctty_io, (int task_nr, message *mess_ptr)		);
_PROTOTYPE( int do_ioctl, (void)					);
_PROTOTYPE( void pm_setsid, (int proc_e)				);
_PROTOTYPE( void dev_status, (message *)				);
_PROTOTYPE( void dev_up, (int major)					);
_PROTOTYPE( endpoint_t suspended_ep, (endpoint_t driver,
						cp_grant_id_t g)	);
_PROTOTYPE( void reopen_reply, (void)					);

/* dmap.c */
_PROTOTYPE( int do_mapdriver, (void)					);
_PROTOTYPE( int map_service, (struct rprocpub *rpub)			);
_PROTOTYPE( void build_dmap, (void)					);
_PROTOTYPE( int map_driver, (const char *label, int major, endpoint_t proc_nr,
	int dev_style, int flags)					);
_PROTOTYPE( int dmap_driver_match, (endpoint_t proc, int major)		);
_PROTOTYPE( void dmap_unmap_by_endpt, (int proc_nr)			);
_PROTOTYPE( void dmap_endpt_up, (int proc_nr)				);

/* exec.c */
_PROTOTYPE( int pm_exec, (int proc_e, char *path, vir_bytes path_len,
			  char *frame, vir_bytes frame_len, vir_bytes *pc));

/* filedes.c */
_PROTOTYPE( struct filp *find_filp, (struct vnode *vp, mode_t bits)	);
_PROTOTYPE( int get_fd, (int start, mode_t bits, int *k, 
            struct filp **fpt)						);
_PROTOTYPE( struct filp *get_filp, (int fild)				);
_PROTOTYPE( struct filp *get_filp2, (struct fproc *rfp, int fild)	);
_PROTOTYPE( int invalidate, (struct filp *)				);
_PROTOTYPE( filp_id_t verify_fd, (endpoint_t ep, int fd)		);
_PROTOTYPE( int do_verify_fd, (void)					);
_PROTOTYPE( int set_filp, (filp_id_t sfilp)				);
_PROTOTYPE( int do_set_filp, (void)					);
_PROTOTYPE( int copy_filp, (endpoint_t to_ep, filp_id_t cfilp)		);
_PROTOTYPE( int do_copy_filp, (void)					);
_PROTOTYPE( int put_filp, (filp_id_t pfilp)				);
_PROTOTYPE( int do_put_filp, (void)					);
_PROTOTYPE( int cancel_fd, (endpoint_t ep, int fd)			);
_PROTOTYPE( int do_cancel_fd, (void)					);
_PROTOTYPE( void close_filp, (struct filp *fp)				);

/* fscall.c */
_PROTOTYPE( void nested_fs_call, (message *m)				);
_PROTOTYPE( void nested_dev_call, (message *m)				);

/* link.c */
_PROTOTYPE( int do_link, (void)						);
_PROTOTYPE( int do_unlink, (void)					);
_PROTOTYPE( int do_rename, (void)					);
_PROTOTYPE( int do_truncate, (void)					);
_PROTOTYPE( int do_ftruncate, (void)					);
_PROTOTYPE( int truncate_vnode, (struct vnode *vp, off_t newsize)	);
_PROTOTYPE( int rdlink_direct, (char *orig_path, char *link_path,
						struct fproc *rfp)	);

/* lock.c */
_PROTOTYPE( int lock_op, (struct filp *f, int req)			);
_PROTOTYPE( void lock_revive, (void)					);

/* main.c */
_PROTOTYPE( int main, (void)						);
_PROTOTYPE( void reply, (int whom, int result)				);

/* misc.c */
_PROTOTYPE( int do_dup, (void)						);
_PROTOTYPE( void pm_exit, (int proc)					);
_PROTOTYPE( int do_fcntl, (void)					);
_PROTOTYPE( void pm_fork, (int pproc, int cproc, int cpid)		);
_PROTOTYPE( void pm_setgid, (int proc_e, int egid, int rgid)		);
_PROTOTYPE( void pm_setuid, (int proc_e, int euid, int ruid)		);
_PROTOTYPE( void pm_setgroups, (int proc_e, int ngroups, gid_t *addr)	);
_PROTOTYPE( int do_sync, (void)						);
_PROTOTYPE( int do_fsync, (void)					);
_PROTOTYPE( void pm_reboot, (void)					);
_PROTOTYPE( int do_svrctl, (void)					);
_PROTOTYPE( int do_getsysinfo, (void)					);
_PROTOTYPE( int pm_dumpcore, (int proc_e, struct mem_map *seg_ptr)	);
_PROTOTYPE( void ds_event, (void)					);

/* mount.c */
_PROTOTYPE( int do_fslogin, (void)                                      );
_PROTOTYPE( int do_mount, (void)					);
_PROTOTYPE( int do_umount, (void)					);
_PROTOTYPE( int unmount, (dev_t dev, char *label)			);

/* open.c */
_PROTOTYPE( int do_close, (void)					);
_PROTOTYPE( int close_fd, (struct fproc *rfp, int fd_nr)		);
_PROTOTYPE( void close_reply, (void)					);
_PROTOTYPE( int do_creat, (void)					);
_PROTOTYPE( int do_lseek, (void)					);
_PROTOTYPE( int do_llseek, (void)					);
_PROTOTYPE( int do_mknod, (void)					);
_PROTOTYPE( int do_mkdir, (void)					);
_PROTOTYPE( int do_open, (void)						);
_PROTOTYPE( int do_slink, (void)                                        );
_PROTOTYPE( int do_vm_open, (void)					);
_PROTOTYPE( int do_vm_close, (void)					);

/* path.c */
_PROTOTYPE( struct vnode *advance, (struct vnode *dirp, int flags,
						struct fproc *rfp)	);
_PROTOTYPE( struct vnode *eat_path, (int flags, struct fproc *rfp)	);
_PROTOTYPE( struct vnode *last_dir, (struct fproc *rfp)			);
_PROTOTYPE( int get_name, (struct vnode *dirp, struct vnode *entry,
							char *_name)	);
_PROTOTYPE( int canonical_path, (char *orig_path, char *canon_path,
						struct fproc *rfp)	);
_PROTOTYPE( int check_perms, (endpoint_t ep, cp_grant_id_t gid,
							int strlen)	);
_PROTOTYPE( int do_check_perms, (void)					);

/* pipe.c */
_PROTOTYPE( int do_pipe, (void)						);
_PROTOTYPE( int map_vnode, (struct vnode *vp)				);
_PROTOTYPE( void unpause, (int proc_nr_e)				);
_PROTOTYPE( int pipe_check, (struct vnode *vp, int rw_flag,
	      int oflags, int bytes, u64_t position, int notouch)	);
_PROTOTYPE( void release, (struct vnode *vp, int call_nr, int count)	);
_PROTOTYPE( void revive, (int proc_nr, int bytes)			);
_PROTOTYPE( void suspend, (int task)					);
_PROTOTYPE( void pipe_suspend, (int rw_flag, int fd_nr, char *buf,
							size_t size)	);
_PROTOTYPE( int select_request_pipe, (struct filp *f, int *ops, int bl)	);
_PROTOTYPE( int select_match_pipe, (struct filp *f)			);
_PROTOTYPE( void unsuspend_by_endpt, (endpoint_t)			);
_PROTOTYPE( void select_reply1, (void)					);
_PROTOTYPE( void select_reply2, (void)					);
_PROTOTYPE( void wait_for, (endpoint_t)					);
#if DO_SANITYCHECKS
_PROTOTYPE( int check_pipe, (void)					);
#endif

/* protect.c */
_PROTOTYPE( int do_access, (void)					);
_PROTOTYPE( int do_chmod, (void)					);
_PROTOTYPE( int do_chown, (void)					);
_PROTOTYPE( int do_umask, (void)					);
_PROTOTYPE( int forbidden, (struct vnode *vp, mode_t access_desired)	);
_PROTOTYPE( int read_only, (struct vnode *vp)				);

/* read.c */
_PROTOTYPE( int do_read, (void)						);
_PROTOTYPE( int do_getdents, (void)					);
_PROTOTYPE( int read_write, (int rw_flag)				);
_PROTOTYPE( int rw_pipe, (int rw_flag, endpoint_t usr,
		int fd_nr, struct filp *f, char *buf, size_t req_size)	);

/* request.c */
_PROTOTYPE( int req_breadwrite, (endpoint_t fs_e, endpoint_t user_e,
			dev_t dev, u64_t pos, unsigned int num_of_bytes,
			char *user_addr, int rw_flag,
			u64_t *new_posp, unsigned int *cum_iop)		);
_PROTOTYPE( int req_chmod, (int fs_e, ino_t inode_nr, mode_t rmode,
						mode_t *new_modep)	);
_PROTOTYPE( int req_chown, (endpoint_t fs_e, ino_t inode_nr,
	uid_t newuid, gid_t newgid, mode_t *new_modep)	);
_PROTOTYPE( int req_create, (int fs_e, ino_t inode_nr, int omode,
		uid_t uid, gid_t gid, char *path, node_details_t *res)	); 
_PROTOTYPE( int req_flush, (endpoint_t fs_e, dev_t dev)			);
_PROTOTYPE( int req_fstatfs, (int fs_e, int who_e, char *buf)		);
_PROTOTYPE( int req_statvfs, (int fs_e, int who_e, char *buf)		);
_PROTOTYPE( int req_ftrunc, (endpoint_t fs_e, ino_t inode_nr,
						off_t start, off_t end)	);
_PROTOTYPE( int req_getdents, (endpoint_t fs_e, ino_t inode_nr,
			u64_t pos, char *buf, size_t size,
			u64_t *new_pos, int direct)			);
_PROTOTYPE( int req_inhibread, (endpoint_t fs_e, ino_t inode_nr)	);
_PROTOTYPE( int req_link, (endpoint_t fs_e, ino_t link_parent,
					char *lastc, ino_t linked_file)	);
_PROTOTYPE( int req_lookup, (endpoint_t fs_e, ino_t dir_ino, ino_t root_ino,
			     uid_t uid, gid_t gid, int flags,
			     lookup_res_t *res, struct fproc *rfp)	);
_PROTOTYPE( int req_mkdir, (endpoint_t fs_e, ino_t inode_nr,
	char *lastc, uid_t uid, gid_t gid, mode_t dmode)		);
_PROTOTYPE( int req_mknod, (endpoint_t fs_e, ino_t inode_nr,
			char *lastc, uid_t uid, gid_t gid,
			mode_t dmode, dev_t dev)			);
_PROTOTYPE( int req_mountpoint, (endpoint_t fs_e, ino_t inode_nr)	);
_PROTOTYPE( int req_newnode, (endpoint_t fs_e, uid_t uid,
				gid_t gid, mode_t dmode,
				dev_t dev, struct node_details *res)	);
_PROTOTYPE( int req_putnode, (int fs_e, ino_t inode_nr, int count)	);
_PROTOTYPE( int req_rdlink, (endpoint_t fs_e, ino_t inode_nr,
				endpoint_t who_e, char *buf, size_t len, 
				int direct)				);
_PROTOTYPE( int req_readsuper, (endpoint_t fs_e, char *driver_name,
				dev_t dev, int readonly, int isroot,
				struct node_details *res_nodep)		);
_PROTOTYPE( int req_readwrite, (endpoint_t fs_e, ino_t inode_nr,
				u64_t pos, int rw_flag,
				endpoint_t user_e, char *user_addr,
				unsigned int num_of_bytes, u64_t *new_posp,
				unsigned int *cum_iop)					); 
_PROTOTYPE( int req_rename, (endpoint_t fs_e, ino_t old_dir,
			char *old_name, ino_t new_dir, char *new_name)	);
_PROTOTYPE( int req_rmdir, (endpoint_t fs_e, ino_t inode_nr,
							char *lastc)	);
_PROTOTYPE(int req_slink, (endpoint_t fs_e, ino_t inode_nr, char *lastc,
		endpoint_t who_e, char *path_addr,
		unsigned short path_length, uid_t uid, gid_t gid)	);
_PROTOTYPE( int req_stat, (int fs_e, ino_t inode_nr, int who_e,
						char *buf, int pos)	);
_PROTOTYPE( int req_sync, (endpoint_t fs_e)                             );
_PROTOTYPE( int req_unlink, (endpoint_t fs_e, ino_t inode_nr,
							char *lastc)	);
_PROTOTYPE( int req_unmount, (endpoint_t fs_e)                          );
_PROTOTYPE( int req_utime, (endpoint_t fs_e, ino_t inode_nr,
					time_t actime, time_t modtime)	);
_PROTOTYPE( int req_newdriver, (endpoint_t fs_e, dev_t dev, 
            endpoint_t driver_e)                                        );

/* stadir.c */
_PROTOTYPE( int do_chdir, (void)					);
_PROTOTYPE( int do_fchdir, (void)					);
_PROTOTYPE( int do_chroot, (void)					);
_PROTOTYPE( int do_fstat, (void)					);
_PROTOTYPE( int do_stat, (void)						);
_PROTOTYPE( int do_fstatfs, (void)					);
_PROTOTYPE( int do_statvfs, (void)					);
_PROTOTYPE( int do_fstatvfs, (void)					);
_PROTOTYPE( int do_rdlink, (void)                                      );
_PROTOTYPE( int do_lstat, (void)                                       );

/* time.c */
_PROTOTYPE( int do_utime, (void)					);

/* utility.c */
_PROTOTYPE( time_t clock_time, (void)					);
_PROTOTYPE( unsigned conv2, (int norm, int w)				);
_PROTOTYPE( long conv4, (int norm, long x)				);
_PROTOTYPE( int fetch_name, (char *path, int len, int flag)		);
_PROTOTYPE( int no_sys, (void)						);
_PROTOTYPE( int isokendpt_f, (char *f, int l, endpoint_t e, int *p, int ft));
_PROTOTYPE( int in_group, (struct fproc *rfp, gid_t grp)		);

#define okendpt(e, p) isokendpt_f(__FILE__, __LINE__, (e), (p), 1)
#define isokendpt(e, p) isokendpt_f(__FILE__, __LINE__, (e), (p), 0)

/* vmnt.c */
_PROTOTYPE( struct vmnt *get_free_vmnt, (short *index)                  );
_PROTOTYPE( struct vmnt *find_vmnt, (int fs_e)                          );

/* vnode.c */
_PROTOTYPE( struct vnode *get_free_vnode, (void)			);
_PROTOTYPE( struct vnode *find_vnode, (int fs_e, int numb)              );
_PROTOTYPE( void dup_vnode, (struct vnode *vp)                          );
_PROTOTYPE( void put_vnode, (struct vnode *vp)                          );
_PROTOTYPE( void vnode_clean_refs, (struct vnode *vp)                   );
#if DO_SANITYCHECKS
_PROTOTYPE( int check_vrefs, (void)			);
#endif

/* write.c */
_PROTOTYPE( int do_write, (void)					);

/* gcov.c */
_PROTOTYPE( int do_gcov_flush, (void)					);

/* select.c */
_PROTOTYPE( int do_select, (void)					);
_PROTOTYPE( int select_callback, (struct filp *, int ops)		);
_PROTOTYPE( void select_forget, (int fproc)				);
_PROTOTYPE( void select_timeout_check, (timer_t *)			);
_PROTOTYPE( void init_select, (void)					);
_PROTOTYPE( void select_unsuspend_by_endpt, (endpoint_t proc)		);
_PROTOTYPE( int select_notified, (int major, int minor, int ops)	);
