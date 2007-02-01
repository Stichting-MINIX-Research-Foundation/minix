/* Function prototypes. */

#include "timers.h"
#include "request.h"

/* Structs used in prototypes must be declared as such first. */
struct filp;		
struct fproc;
struct vmnt;
struct vnode;

/* device.c */
_PROTOTYPE( int dev_open, (Dev_t dev, int proc, int flags)		);
_PROTOTYPE( void dev_close, (Dev_t dev)					);
_PROTOTYPE( int dev_bio, (int op, Dev_t dev, int proc, void *buf,
			off_t pos, int bytes)				);
_PROTOTYPE( int dev_io, (int op, Dev_t dev, int proc, void *buf,
			u64_t pos, int bytes, int flags)		);
_PROTOTYPE( int gen_opcl, (int op, Dev_t dev, int proc, int flags)	);
_PROTOTYPE( int gen_io, (int task_nr, message *mess_ptr)		);
_PROTOTYPE( int no_dev, (int op, Dev_t dev, int proc, int flags)	);
_PROTOTYPE( int no_dev_io, (int, message *)				);
_PROTOTYPE( int tty_opcl, (int op, Dev_t dev, int proc, int flags)	);
_PROTOTYPE( int ctty_opcl, (int op, Dev_t dev, int proc, int flags)	);
_PROTOTYPE( int clone_opcl, (int op, Dev_t dev, int proc, int flags)	);
_PROTOTYPE( int ctty_io, (int task_nr, message *mess_ptr)		);
_PROTOTYPE( int do_ioctl, (void)					);
_PROTOTYPE( void pm_setsid, (int proc_e)				);
_PROTOTYPE( void dev_status, (message *)				);
_PROTOTYPE( void dev_up, (int major)					);

/* dmap.c */
_PROTOTYPE( int do_devctl, (void)					);
_PROTOTYPE( int fs_devctl, (int req, int dev, int proc_nr_e, int style,
	int force)							);
_PROTOTYPE( void build_dmap, (void)					);
_PROTOTYPE( int map_driver, (int major, int proc_nr, int dev_style,
	int force)							);
_PROTOTYPE( int dmap_driver_match, (int proc, int major)		);
_PROTOTYPE( void dmap_unmap_by_endpt, (int proc_nr)			);
_PROTOTYPE( void dmap_endpt_up, (int proc_nr)				);

/* exec.c */
_PROTOTYPE( int pm_exec, (int proc_e, char *path, vir_bytes path_len,
				char *frame, vir_bytes frame_len)	);

/* filedes.c */
_PROTOTYPE( struct filp *find_filp, (struct vnode *vp, mode_t bits)	);
_PROTOTYPE( int get_fd, (int start, mode_t bits, int *k, 
            struct filp **fpt)                                          );
_PROTOTYPE( struct filp *get_filp, (int fild)				);
_PROTOTYPE( struct filp *get_filp2, (struct fproc *rfp, int fild)	);
_PROTOTYPE( int inval_filp, (struct filp *)				);

/* link.c */
_PROTOTYPE( int do_link, (void)						);
_PROTOTYPE( int do_unlink, (void)					);
_PROTOTYPE( int do_rename, (void)					);
_PROTOTYPE( int do_truncate, (void)					);
_PROTOTYPE( int do_ftruncate, (void)					);
_PROTOTYPE( int truncate_vn, (struct vnode *vp, off_t newsize)		);

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
_PROTOTYPE( int do_sync, (void)						);
_PROTOTYPE( int do_fsync, (void)					);
_PROTOTYPE( void pm_reboot, (void)					);
_PROTOTYPE( int do_svrctl, (void)					);
_PROTOTYPE( int do_getsysinfo, (void)					);
_PROTOTYPE( int pm_dumpcore, (int proc_e, struct mem_map *seg_ptr)	);

/* mount.c */
_PROTOTYPE( int do_fslogin, (void)                                      );
_PROTOTYPE( int do_mount, (void)					);
_PROTOTYPE( int do_umount, (void)					);
_PROTOTYPE( int unmount, (Dev_t dev)					);

/* open.c */
_PROTOTYPE( int do_close, (void)					);
_PROTOTYPE( int close_fd, (struct fproc *rfp, int fd_nr)		);
_PROTOTYPE( int do_creat, (void)					);
_PROTOTYPE( int do_lseek, (void)					);
_PROTOTYPE( int do_llseek, (void)					);
_PROTOTYPE( int do_mknod, (void)					);
_PROTOTYPE( int do_mkdir, (void)					);
_PROTOTYPE( int do_open, (void)						);
_PROTOTYPE( int do_slink, (void)                                        );

/* path.c */
_PROTOTYPE( int lookup, (lookup_req_t *request, node_details_t *node)   );
_PROTOTYPE( int lookup_vp, (lookup_req_t *request, struct vnode **vpp)	);
_PROTOTYPE( int Xlookup_vp, (lookup_req_t *request, struct vnode **vpp,
							char **pathrem)	);

/* pipe.c */
_PROTOTYPE( int do_pipe, (void)						);
_PROTOTYPE( int do_unpause, (void)					);
_PROTOTYPE( int unpause, (int proc_nr_e)				);
_PROTOTYPE( int pipe_check, (struct vnode *vp, int rw_flag,
      int oflags, int bytes, u64_t position, int *canwrite, int notouch));
_PROTOTYPE( void release, (struct vnode *vp, int call_nr, int count)	);
_PROTOTYPE( void revive, (int proc_nr, int bytes)			);
_PROTOTYPE( void suspend, (int task)					);
_PROTOTYPE( int select_request_pipe, (struct filp *f, int *ops, int bl)	);
_PROTOTYPE( int select_cancel_pipe, (struct filp *f)			);
_PROTOTYPE( int select_match_pipe, (struct filp *f)			);
_PROTOTYPE( void unsuspend_by_endpt, (int)				);

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

/* request.c */
#define req_getnode(req, res) req_getnode_f(__FILE__, __LINE__, (req), (res))
_PROTOTYPE( int req_getnode_f, (char *file, int line, node_req_t *req, node_details_t *res)     );
_PROTOTYPE( int req_putnode, (int fs_e, ino_t inode_nr, int count)	);
_PROTOTYPE( int req_open, (open_req_t *req, node_details_t *res)        ); 
_PROTOTYPE( int req_create, (int fs_e, ino_t inode_nr, int omode,
		int uid, int gid, char *path, node_details_t *res)	); 
_PROTOTYPE( int req_readwrite, (readwrite_req_t *req, 
            readwrite_res_t *res)                                       );
_PROTOTYPE( int req_pipe, (pipe_req_t *req, node_details_t *res)        );
_PROTOTYPE( int req_clone_opcl, (clone_opcl_req_t *req, 
            node_details_t *res)                                        );
_PROTOTYPE( int req_ftrunc, (ftrunc_req_t *req)                         );
_PROTOTYPE( int req_chown, (chown_req_t *req, int *mode)                );
_PROTOTYPE( int req_chmod, (chmod_req_t *req, int *mode)                );
_PROTOTYPE( int req_access, (access_req_t *req)                         );
_PROTOTYPE( int req_mknod, (mknod_req_t *req)                           );
_PROTOTYPE( int req_mkdir, (mkdir_req_t *req)                           );
_PROTOTYPE( int req_inhibread, (node_req_t *req)                        );
_PROTOTYPE( int req_stat, (int fs_e, ino_t inode_nr, int who_e,
						char *buf, int pos)	);
_PROTOTYPE( int req_fstatfs, (int fs_e, ino_t inode_nr, int who_e,
							char *buf)	);
_PROTOTYPE( int req_unlink, (unlink_req_t *req)                         );
_PROTOTYPE( int req_rmdir, (unlink_req_t *req)                          );
_PROTOTYPE( int req_utime, (utime_req_t *req)                           );
_PROTOTYPE( int req_stime, (endpoint_t fs_e, time_t boottime)           );
_PROTOTYPE( int req_sync, (endpoint_t fs_e)                             );
_PROTOTYPE( int req_link, (link_req_t *req)                             );
_PROTOTYPE( int req_slink, (slink_req_t *req)                           );
_PROTOTYPE( int req_rdlink, (rdlink_req_t *req)                         );
_PROTOTYPE( int req_rename, (rename_req_t *req)                         );
_PROTOTYPE( int req_mountpoint, (mountpoint_req_t *req, 
            node_details_t *res)                                        );
_PROTOTYPE( int req_readsuper, (readsuper_req_t *req, 
            readsuper_res_t *res)                                       );
_PROTOTYPE( int req_unmount, (endpoint_t fs_e)                          );
_PROTOTYPE( int req_trunc, (trunc_req_t *req)                           );
_PROTOTYPE( int req_lookup, (lookup_req_t *req, lookup_res_t *res)      );
_PROTOTYPE( int req_newdriver, (endpoint_t fs_e, Dev_t dev, 
            endpoint_t driver_e)                                        );
_PROTOTYPE( int req_breadwrite, (breadwrite_req_t *req, 
            readwrite_res_t *res)                                       );
_PROTOTYPE( int req_getdents, (endpoint_t fs_e, ino_t inode_nr,
	off_t pos, cp_grant_id_t gid, size_t size, off_t *pos_change)	);
_PROTOTYPE( int req_flush, (endpoint_t fs_e, Dev_t)                     );

/* stadir.c */
_PROTOTYPE( int do_chdir, (void)					);
_PROTOTYPE( int do_fchdir, (void)					);
_PROTOTYPE( int do_chroot, (void)					);
_PROTOTYPE( int do_fstat, (void)					);
_PROTOTYPE( int do_stat, (void)						);
_PROTOTYPE( int do_fstatfs, (void)					);
_PROTOTYPE( int do_rdlink, (void)                                      );
_PROTOTYPE( int do_lstat, (void)                                       );

/* time.c */
_PROTOTYPE( int do_stime, (void)					);
_PROTOTYPE( int do_utime, (void)					);

/* utility.c */
_PROTOTYPE( time_t clock_time, (void)					);
_PROTOTYPE( unsigned conv2, (int norm, int w)				);
_PROTOTYPE( long conv4, (int norm, long x)				);
_PROTOTYPE( int fetch_name, (char *path, int len, int flag)		);
_PROTOTYPE( int no_sys, (void)						);
_PROTOTYPE( int isokendpt_f, (char *f, int l, int e, int *p, int ft));
_PROTOTYPE( void panic, (char *who, char *mess, int num)		);

#define okendpt(e, p) isokendpt_f(__FILE__, __LINE__, (e), (p), 1)
#define isokendpt(e, p) isokendpt_f(__FILE__, __LINE__, (e), (p), 0)

/* vmnt.c */
_PROTOTYPE( struct vmnt *get_free_vmnt, (short *index)                  );
_PROTOTYPE( struct vmnt *find_vmnt, (int fs_e)                          );

/* vnode.c */
_PROTOTYPE( struct vnode *get_free_vnode, (char *file, int line)	);
_PROTOTYPE( struct vnode *find_vnode, (int fs_e, int numb)              );
_PROTOTYPE( void dup_vnode, (struct vnode *vp)                          );
_PROTOTYPE( void put_vnode, (struct vnode *vp)                          );
_PROTOTYPE( void vnode_clean_refs, (struct vnode *vp)                   );
_PROTOTYPE( struct vnode *get_vnode, (int fs_e, int inode_nr)           );
_PROTOTYPE( struct vnode *get_vnode_x, (int fs_e, int inode_nr)		);
#if 0
_PROTOTYPE( void mark_vn, (struct vnode *vp, char *file, int line)	);
_PROTOTYPE( int check_vrefs, (void)					);
#endif

/* write.c */
_PROTOTYPE( int do_write, (void)					);

/* select.c */
_PROTOTYPE( int do_select, (void)					);
_PROTOTYPE( int select_callback, (struct filp *, int ops)		);
_PROTOTYPE( void select_forget, (int fproc)				);
_PROTOTYPE( void select_timeout_check, (timer_t *)			);
_PROTOTYPE( void init_select, (void)					);
_PROTOTYPE( void select_unsuspend_by_endpt, (int proc)			);
_PROTOTYPE( int select_notified, (int major, int minor, int ops)	);

/* timers.c */
_PROTOTYPE( void fs_set_timer, (timer_t *tp, int delta, 
            tmr_func_t watchdog, int arg)                               );
_PROTOTYPE( void fs_expire_timers, (clock_t now)			);
_PROTOTYPE( void fs_cancel_timer, (timer_t *tp)				);
_PROTOTYPE( void fs_init_timer, (timer_t *tp)				);


