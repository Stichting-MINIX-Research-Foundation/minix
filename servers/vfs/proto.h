#ifndef __VFS_PROTO_H__
#define __VFS_PROTO_H__

/* Function prototypes. */

#include "timers.h"
#include "request.h"
#include "tll.h"
#include "threads.h"
#include <minix/rs.h>

/* Structs used in prototypes must be declared as such first. */
struct filp;
struct fproc;
struct vmnt;
struct vnode;
struct lookup;
struct worker_thread;
struct job;

typedef struct filp * filp_id_t;

/* comm.c */
int drv_sendrec(endpoint_t drv_e, message *reqm);
void fs_cancel(struct vmnt *vmp);
int fs_sendrec(endpoint_t fs_e, message *reqm);
void fs_sendmore(struct vmnt *vmp);
void send_work(void);

/* device.c */
int dev_open(dev_t dev, endpoint_t proc_e, int flags);
int dev_reopen(dev_t dev, int filp_no, int flags);
void dev_reply(struct dmap *dp);
int dev_close(dev_t dev, int filp_no);
int bdev_open(dev_t dev, int access);
int bdev_close(dev_t dev);
int dev_io(int op, dev_t dev, endpoint_t proc_e, void *buf, u64_t pos,
	size_t bytes, int flags, int suspend_reopen);
int gen_opcl(int op, dev_t dev, endpoint_t task_nr, int flags);
int gen_io(endpoint_t driver_e, message *mess_ptr);
int asyn_io(endpoint_t drv_e, message *mess_ptr);
int no_dev(int op, dev_t dev, int proc, int flags);
int no_dev_io(int, message *);
int tty_opcl(int op, dev_t dev, endpoint_t proc, int flags);
int ctty_opcl(int op, dev_t dev, endpoint_t proc, int flags);
int clone_opcl(int op, dev_t dev, int proc, int flags);
int ctty_io(int task_nr, message *mess_ptr);
int do_ioctl(void);
void pm_setsid(endpoint_t proc_e);
void dev_status(endpoint_t drv_e);
void bdev_up(int major);
void cdev_up(int major);
endpoint_t find_suspended_ep(endpoint_t driver, cp_grant_id_t g);
void reopen_reply(void);
void open_reply(void);

/* dmap.c */
void lock_dmap(struct dmap *dp);
void unlock_dmap(struct dmap *dp);
int do_mapdriver(void);
void init_dmap(void);
void init_dmap_locks(void);
int dmap_driver_match(endpoint_t proc, int major);
void dmap_endpt_up(int proc_nr, int is_blk);
void dmap_unmap_by_endpt(int proc_nr);
struct dmap *get_dmap(endpoint_t proc_e);
struct dmap *get_dmap_by_major(int major);
int do_mapdriver(void);
int map_service(struct rprocpub *rpub);
void dmap_unmap_by_endpt(int proc_nr);
int map_driver(const char *label, int major, endpoint_t proc_nr, int
	dev_style, int flags);
int map_service(struct rprocpub *rpub);

/* elf_core_dump.c */
void write_elf_core_file(struct filp *f, int csig, char *exe_name);

/* exec.c */
int pm_exec(endpoint_t proc_e, vir_bytes path, size_t path_len, vir_bytes frame,
	size_t frame_len, vir_bytes *pc, vir_bytes *newsp, int flags);
#define check_bsf_lock() do {						\
	assert(mutex_trylock(&bsf_lock) == 0);				\
	unlock_bsf();							\
			} while(0)

/* filedes.c */
void *do_filp_gc(void *arg);
void check_filp_locks(void);
void check_filp_locks_by_me(void);
void init_filps(void);
struct filp *find_filp(struct vnode *vp, mode_t bits);
int get_fd(int start, mode_t bits, int *k, struct filp **fpt);
struct filp *get_filp(int fild, tll_access_t locktype);
struct filp *get_filp2(struct fproc *rfp, int fild, tll_access_t
	locktype);
void lock_filp(struct filp *filp, tll_access_t locktype);
void unlock_filp(struct filp *filp);
void unlock_filps(struct filp *filp1, struct filp *filp2);
int invalidate_filp(struct filp *);
void invalidate_filp_by_endpt(endpoint_t proc_e);
void invalidate_filp_by_char_major(int major);
int do_verify_fd(void);
int set_filp(filp_id_t sfilp);
int do_set_filp(void);
int copy_filp(endpoint_t to_ep, filp_id_t cfilp);
int do_copy_filp(void);
int put_filp(filp_id_t pfilp);
int do_put_filp(void);
int cancel_fd(endpoint_t ep, int fd);
int do_cancel_fd(void);
void close_filp(struct filp *fp);

/* fscall.c */
void nested_fs_call(message *m);

/* link.c */
int do_link(void);
int do_unlink(void);
int do_rename(void);
int do_truncate(void);
int do_ftruncate(void);
int truncate_vnode(struct vnode *vp, off_t newsize);
int rdlink_direct(char *orig_path, char *link_path, struct fproc *rfp);

/* lock.c */
int lock_op(struct filp *f, int req);
void lock_revive(void);

/* main.c */
int main(void);
void lock_proc(struct fproc *rfp, int force_lock);
void reply(endpoint_t whom, int result);
void thread_cleanup(struct fproc *rfp);
void unlock_proc(struct fproc *rfp);

/* misc.c */
int do_dup(void);
void pm_exit(int proc);
int do_fcntl(void);
void pm_fork(int pproc, int cproc, int cpid);
void pm_setgid(int proc_e, int egid, int rgid);
void pm_setuid(int proc_e, int euid, int ruid);
void pm_setgroups(int proc_e, int ngroups, gid_t *addr);
int do_sync(void);
int do_fsync(void);
void pm_reboot(void);
int do_svrctl(void);
int do_getsysinfo(void);
int pm_dumpcore(endpoint_t proc_e, int sig, vir_bytes exe_name);
void * ds_event(void *arg);

/* mount.c */
int do_fsready(void);
int do_mount(void);
int do_umount(void);
int is_nonedev(dev_t dev);
void mount_pfs(void);
int mount_fs(dev_t dev, char fullpath[PATH_MAX+1], endpoint_t fs_e, int
	rdonly, char mount_label[LABEL_MAX]);
int unmount(dev_t dev, char label[LABEL_MAX]);
void unmount_all(void);

/* open.c */
int do_close(void);
int close_fd(struct fproc *rfp, int fd_nr);
void close_reply(void);
int common_open(char path[PATH_MAX], int oflags, mode_t omode);
int do_creat(void);
int do_lseek(void);
int do_llseek(void);
int do_mknod(void);
int do_mkdir(void);
int do_open(void);
int do_slink(void);
int do_vm_open(void);
int do_vm_close(void);

/* path.c */
struct vnode *advance(struct vnode *dirp, struct lookup *resolve, struct
	fproc *rfp);
struct vnode *eat_path(struct lookup *resolve, struct fproc *rfp);
struct vnode *last_dir(struct lookup *resolve, struct fproc *rfp);
void lookup_init(struct lookup *resolve, char *path, int flags, struct
	vmnt **vmp, struct vnode **vp);
int get_name(struct vnode *dirp, struct vnode *entry, char *_name);
int canonical_path(char *orig_path, struct fproc *rfp);
int do_check_perms(void);

/* pipe.c */
int do_pipe(void);
int map_vnode(struct vnode *vp, endpoint_t fs_e);
void unpause(endpoint_t proc_e);
int pipe_check(struct vnode *vp, int rw_flag, int oflags, int bytes,
	u64_t position, int notouch);
void release(struct vnode *vp, int op, int count);
void revive(endpoint_t proc_e, int returned);
void suspend(int why);
void pipe_suspend(struct filp *rfilp, char *buf, size_t size);
void unsuspend_by_endpt(endpoint_t proc_e);
void wait_for(endpoint_t proc_e);

/* protect.c */
int do_access(void);
int do_chmod(void);
int do_chown(void);
int do_umask(void);
int forbidden(struct fproc *rfp, struct vnode *vp, mode_t
	access_desired);
int read_only(struct vnode *vp);

/* read.c */
int do_read(void);
int do_getdents(void);
void lock_bsf(void);
void unlock_bsf(void);
int do_read_write(int rw_flag);
int read_write(int rw_flag, struct filp *f, char *buffer, size_t nbytes,
	endpoint_t for_e);
int rw_pipe(int rw_flag, endpoint_t usr, struct filp *f, char *buf,
	size_t req_size);

/* request.c */
int req_breadwrite(endpoint_t fs_e, endpoint_t user_e, dev_t dev, u64_t pos,
	unsigned int num_of_bytes, char *user_addr, int rw_flag,
	u64_t *new_posp, unsigned int *cum_iop);
int req_chmod(int fs_e, ino_t inode_nr, mode_t rmode, mode_t *new_modep);
int req_chown(endpoint_t fs_e, ino_t inode_nr, uid_t newuid, gid_t newgid,
	mode_t *new_modep);
int req_create(int fs_e, ino_t inode_nr, int omode, uid_t uid, gid_t gid,
	char *path, node_details_t *res);
int req_flush(endpoint_t fs_e, dev_t dev);
int req_fstatfs(endpoint_t fs_e, endpoint_t proc_e, vir_bytes buf);
int req_statvfs(endpoint_t fs_e, endpoint_t proc_e, vir_bytes buf);
int req_ftrunc(endpoint_t fs_e, ino_t inode_nr, off_t start, off_t end);
int req_getdents(endpoint_t fs_e, ino_t inode_nr, u64_t pos, char *buf,
	size_t size, u64_t *new_pos, int direct);
int req_inhibread(endpoint_t fs_e, ino_t inode_nr);
int req_link(endpoint_t fs_e, ino_t link_parent, char *lastc,
	ino_t linked_file);
int req_lookup(endpoint_t fs_e, ino_t dir_ino, ino_t root_ino, uid_t uid,
	gid_t gid, struct lookup *resolve, lookup_res_t *res,
	struct fproc *rfp);
int req_mkdir(endpoint_t fs_e, ino_t inode_nr, char *lastc, uid_t uid,
	gid_t gid, mode_t dmode);
int req_mknod(endpoint_t fs_e, ino_t inode_nr, char *lastc, uid_t uid,
	gid_t gid, mode_t dmode, dev_t dev);
int req_mountpoint(endpoint_t fs_e, ino_t inode_nr);
int req_newnode(endpoint_t fs_e, uid_t uid, gid_t gid, mode_t dmode, dev_t dev,
	struct node_details *res);
int req_putnode(int fs_e, ino_t inode_nr, int count);
int req_rdlink(endpoint_t fs_e, ino_t inode_nr, endpoint_t proc_e,
	vir_bytes buf, size_t len, int direct);
int req_readsuper(endpoint_t fs_e, char *driver_name, dev_t dev, int readonly,
	int isroot, struct node_details *res_nodep, int *con_reqs);
int req_readwrite(endpoint_t fs_e, ino_t inode_nr, u64_t pos, int rw_flag,
	endpoint_t user_e, char *user_addr, unsigned int num_of_bytes,
	u64_t *new_posp, unsigned int *cum_iop);
int req_rename(endpoint_t fs_e, ino_t old_dir, char *old_name, ino_t new_dir,
	char *new_name);
int req_rmdir(endpoint_t fs_e, ino_t inode_nr, char *lastc);
int req_slink(endpoint_t fs_e, ino_t inode_nr, char *lastc, endpoint_t proc_e,
	vir_bytes path_addr, size_t path_length, uid_t uid, gid_t gid);
int req_stat(endpoint_t fs_e, ino_t inode_nr, endpoint_t proc_e, vir_bytes buf,
	int old_stat);
int req_sync(endpoint_t fs_e);
int req_unlink(endpoint_t fs_e, ino_t inode_nr, char *lastc);
int req_unmount(endpoint_t fs_e);
int req_utime(endpoint_t fs_e, ino_t inode_nr, time_t actime, time_t modtime);
int req_newdriver(endpoint_t fs_e, dev_t dev, char *label);

/* stadir.c */
int do_chdir(void);
int do_fchdir(void);
int do_chroot(void);
int do_fstat(void);
int do_stat(void);
int do_fstatfs(void);
int do_statvfs(void);
int do_fstatvfs(void);
int do_rdlink(void);
int do_lstat(void);

/* time.c */
int do_utime(void);

/* tll.c */
void tll_downgrade(tll_t *tllp);
int tll_haspendinglock(tll_t *tllp);
void tll_init(tll_t *tllp);
int tll_islocked(tll_t *tllp);
int tll_lock(tll_t *tllp, tll_access_t locktype);
int tll_locked_by_me(tll_t *tllp);
void tll_lockstat(tll_t *tllp);
int tll_unlock(tll_t *tllp);
void tll_upgrade(tll_t *tllp);

/* utility.c */
time_t clock_time(void);
unsigned conv2(int norm, int w);
long conv4(int norm, long x);
int copy_name(size_t len, char *dest);
int fetch_name(vir_bytes path, size_t len, char *dest);
int no_sys(void);
int isokendpt_f(char *f, int l, endpoint_t e, int *p, int ft);
int in_group(struct fproc *rfp, gid_t grp);

#define okendpt(e, p) isokendpt_f(__FILE__, __LINE__, (e), (p), 1)
#define isokendpt(e, p) isokendpt_f(__FILE__, __LINE__, (e), (p), 0)

/* vmnt.c */
void check_vmnt_locks(void);
void check_vmnt_locks_by_me(struct fproc *rfp);
void mark_vmnt_free(struct vmnt *vmp);
struct vmnt *get_free_vmnt(void);
struct vmnt *find_vmnt(endpoint_t fs_e);
struct vmnt *get_locked_vmnt(struct fproc *rfp);
void init_vmnts(void);
int lock_vmnt(struct vmnt *vp, tll_access_t locktype);
void unlock_vmnt(struct vmnt *vp);
void vmnt_unmap_by_endpt(endpoint_t proc_e);

/* vnode.c */
void check_vnode_locks(void);
void check_vnode_locks_by_me(struct fproc *rfp);
struct vnode *get_free_vnode(void);
struct vnode *find_vnode(int fs_e, ino_t inode);
void init_vnodes(void);
int is_vnode_locked(struct vnode *vp);
int lock_vnode(struct vnode *vp, tll_access_t locktype);
void unlock_vnode(struct vnode *vp);
void dup_vnode(struct vnode *vp);
void put_vnode(struct vnode *vp);
void vnode_clean_refs(struct vnode *vp);

/* write.c */
int do_write(void);

/* gcov.c */
int do_gcov_flush(void);
#if ! USE_COVERAGE
#define do_gcov_flush no_sys
#endif

/* select.c */
int do_select(void);
void init_select(void);
void select_callback(struct filp *, int ops);
void select_forget(endpoint_t proc_e);
void select_reply1(endpoint_t driver_e, int minor, int status);
void select_reply2(endpoint_t driver_e, int minor, int status);
void select_timeout_check(timer_t *);
void select_unsuspend_by_endpt(endpoint_t proc);

/* worker.c */
int worker_available(void);
struct worker_thread *worker_get(thread_t worker_tid);
struct job *worker_getjob(thread_t worker_tid);
void worker_init(struct worker_thread *worker);
void worker_signal(struct worker_thread *worker);
void worker_start(void *(*func)(void *arg));
void worker_stop(struct worker_thread *worker);
void worker_stop_by_endpt(endpoint_t proc_e);
void worker_wait(void);
void sys_worker_start(void *(*func)(void *arg));
void dl_worker_start(void *(*func)(void *arg));
#endif
