#ifndef __VFS_PROTO_H__
#define __VFS_PROTO_H__

/* Function prototypes. */

#include <minix/rs.h>
#include <minix/timers.h>

#include "request.h"
#include "threads.h"
#include "tll.h"

/* Structs used in prototypes must be declared as such first. */
struct filp;
struct fproc;
struct timespec;
struct vmnt;
struct vnode;
struct lookup;
struct worker_thread;
struct job;

/* comm.c */
int drv_sendrec(endpoint_t drv_e, message *reqm);
void fs_cancel(struct vmnt *vmp);
int fs_sendrec(endpoint_t fs_e, message *reqm);
int vm_sendrec(message *reqm);
void fs_sendmore(struct vmnt *vmp);
void send_work(void);
int vm_vfs_procctl_handlemem(endpoint_t ep, vir_bytes mem, vir_bytes len, int flags);

/* device.c */
int cdev_open(dev_t dev, int flags);
int cdev_close(dev_t dev);
int cdev_io(int op, dev_t dev, endpoint_t proc_e, vir_bytes buf, off_t pos,
	unsigned long bytes, int flags);
dev_t cdev_map(dev_t dev, struct fproc *rfp);
int cdev_select(dev_t dev, int ops);
int cdev_cancel(dev_t dev);
void cdev_reply(void);
int bdev_open(dev_t dev, int access);
int bdev_close(dev_t dev);
void bdev_reply(void);
void bdev_up(devmajor_t major);
int do_ioctl(void);

/* dmap.c */
void lock_dmap(struct dmap *dp);
void unlock_dmap(struct dmap *dp);
int do_mapdriver(void);
void init_dmap(void);
int dmap_driver_match(endpoint_t proc, devmajor_t major);
void dmap_endpt_up(endpoint_t proc_nr, int is_blk);
struct dmap *get_dmap(endpoint_t proc_e);
struct dmap *get_dmap_by_major(devmajor_t major);
void dmap_unmap_by_endpt(endpoint_t proc_nr);
int map_service(struct rprocpub *rpub);

/* elf_core_dump.c */
void write_elf_core_file(struct filp *f, int csig, char *exe_name);

/* exec.c */
int pm_exec(vir_bytes path, size_t path_len, vir_bytes frame, size_t frame_len,
	vir_bytes *pc, vir_bytes *newsp, vir_bytes *ps_str);

/* filedes.c */
void check_filp_locks(void);
void check_filp_locks_by_me(void);
void init_filps(void);
struct filp *find_filp(struct vnode *vp, mode_t bits);
int get_fd(struct fproc *rfp, int start, mode_t bits, int *k,
	struct filp **fpt);
struct filp *get_filp(int fild, tll_access_t locktype);
struct filp *get_filp2(struct fproc *rfp, int fild, tll_access_t locktype);
void lock_filp(struct filp *filp, tll_access_t locktype);
void unlock_filp(struct filp *filp);
void unlock_filps(struct filp *filp1, struct filp *filp2);
void invalidate_filp(struct filp *);
void invalidate_filp_by_endpt(endpoint_t proc_e);
void invalidate_filp_by_char_major(devmajor_t major);
void close_filp(struct filp *fp);
int do_copyfd(void);

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
void lock_proc(struct fproc *rfp);
void unlock_proc(struct fproc *rfp);
void replycode(endpoint_t whom, int result);
void service_pm_postponed(void);
void thread_cleanup(void);

/* misc.c */
void pm_exit(void);
int do_fcntl(void);
void pm_fork(endpoint_t pproc, endpoint_t cproc, pid_t cpid);
void pm_setgid(endpoint_t proc_e, int egid, int rgid);
void pm_setuid(endpoint_t proc_e, int euid, int ruid);
void pm_setgroups(endpoint_t proc_e, int ngroups, gid_t *addr);
void pm_setsid(endpoint_t proc_e);
int do_sync(void);
int do_fsync(void);
void pm_reboot(void);
int do_svrctl(void);
int do_getsysinfo(void);
int do_vm_call(void);
int pm_dumpcore(int sig, vir_bytes exe_name);
void ds_event(void);
int dupvm(struct fproc *fp, int pfd, int *vmfd, struct filp **f);
int do_getrusage(void);

/* mount.c */
int do_mount(void);
int do_umount(void);
int is_nonedev(dev_t dev);
void mount_pfs(void);
int mount_fs(dev_t dev, char mount_dev[PATH_MAX], char mount_path[PATH_MAX],
	endpoint_t fs_e, int rdonly, char mount_type[FSTYPE_MAX],
	char mount_label[LABEL_MAX]);
int unmount(dev_t dev, char label[LABEL_MAX]);
void unmount_all(int force);

/* open.c */
int do_close(void);
int close_fd(struct fproc *rfp, int fd_nr);
int common_open(char path[PATH_MAX], int oflags, mode_t omode);
int do_creat(void);
int do_lseek(void);
int do_mknod(void);
int do_mkdir(void);
int do_open(void);
int do_creat(void);
int do_slink(void);
int actual_lseek(struct fproc *rfp, int seekfd, int seekwhence, off_t offset,
	off_t *newposp);

/* path.c */
struct vnode *advance(struct vnode *dirp, struct lookup *resolve, struct
	fproc *rfp);
struct vnode *eat_path(struct lookup *resolve, struct fproc *rfp);
struct vnode *last_dir(struct lookup *resolve, struct fproc *rfp);
void lookup_init(struct lookup *resolve, char *path, int flags, struct
	vmnt **vmp, struct vnode **vp);
int get_name(struct vnode *dirp, struct vnode *entry, char *_name);
int canonical_path(char *orig_path, struct fproc *rfp);
int do_checkperms(void);

/* pipe.c */
int do_pipe2(void);
int map_vnode(struct vnode *vp, endpoint_t fs_e);
void unpause(void);
int pipe_check(struct filp *filp, int rw_flag, int oflags, int bytes,
	int notouch);
void release(struct vnode *vp, int op, int count);
void revive(endpoint_t proc_e, int returned);
void suspend(int why);
void pipe_suspend(struct filp *rfilp, vir_bytes buf, size_t size);
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
void check_bsf_lock(void);
int do_read_write_peek(int rw_flag, int fd, vir_bytes buf, size_t bytes);
int actual_read_write_peek(struct fproc *rfp, int rw_flag, int fd, vir_bytes buf,
	size_t bytes);
int read_write(struct fproc *rfp, int rw_flag, struct filp *f, vir_bytes buffer,
	size_t nbytes, endpoint_t for_e);
int rw_pipe(int rw_flag, endpoint_t usr, struct filp *f, vir_bytes buf,
	size_t req_size);

/* request.c */
int req_breadwrite(endpoint_t fs_e, endpoint_t user_e, dev_t dev, off_t pos,
	unsigned int num_of_bytes, vir_bytes user_addr, int rw_flag,
	off_t *new_posp, unsigned int *cum_iop);
int req_chmod(endpoint_t fs_e, ino_t inode_nr, mode_t rmode,
	mode_t *new_modep);
int req_chown(endpoint_t fs_e, ino_t inode_nr, uid_t newuid, gid_t newgid,
	mode_t *new_modep);
int req_create(endpoint_t fs_e, ino_t inode_nr, int omode, uid_t uid,
	gid_t gid, char *path, node_details_t *res);
int req_flush(endpoint_t fs_e, dev_t dev);
int req_statvfs(endpoint_t fs_e, struct statvfs *buf);
int req_ftrunc(endpoint_t fs_e, ino_t inode_nr, off_t start, off_t end);
int req_getdents(endpoint_t fs_e, ino_t inode_nr, off_t pos, vir_bytes buf,
	size_t size, off_t *new_pos, int direct);
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
int req_newnode(endpoint_t fs_e, uid_t uid, gid_t gid, mode_t dmode,
	dev_t dev, struct node_details *res);
int req_putnode(int fs_e, ino_t inode_nr, int count);
int req_rdlink(endpoint_t fs_e, ino_t inode_nr, endpoint_t proc_e,
	vir_bytes buf, size_t len, int direct);
int req_readsuper(struct vmnt *vmp, char *driver_name, dev_t dev, int readonly,
	int isroot, struct node_details *res_nodep, unsigned int *fs_flags);
int req_readwrite(endpoint_t fs_e, ino_t inode_nr, off_t pos, int rw_flag,
	endpoint_t user_e, vir_bytes user_addr, unsigned int num_of_bytes,
	off_t *new_posp, unsigned int *cum_iop);
int req_bpeek(endpoint_t fs_e, dev_t dev, off_t pos, unsigned int num_of_bytes);
int req_peek(endpoint_t fs_e, ino_t inode_nr, off_t pos, unsigned int bytes);
int req_rename(endpoint_t fs_e, ino_t old_dir, char *old_name, ino_t new_dir,
	char *new_name);
int req_rmdir(endpoint_t fs_e, ino_t inode_nr, char *lastc);
int req_slink(endpoint_t fs_e, ino_t inode_nr, char *lastc, endpoint_t proc_e,
	vir_bytes path_addr, size_t path_length, uid_t uid, gid_t gid);
int req_stat(endpoint_t fs_e, ino_t inode_nr, endpoint_t proc_e, vir_bytes buf);
int req_sync(endpoint_t fs_e);
int req_unlink(endpoint_t fs_e, ino_t inode_nr, char *lastc);
int req_unmount(endpoint_t fs_e);
int req_utime(endpoint_t fs_e, ino_t inode_nr, struct timespec * actv,
	struct timespec * modtv);
int req_newdriver(endpoint_t fs_e, dev_t dev, char *label);

/* stadir.c */
int do_chdir(void);
int do_fchdir(void);
int do_chroot(void);
int do_fstat(void);
int do_stat(void);
int do_statvfs(void);
int do_fstatvfs(void);
int do_getvfsstat(void);
int do_rdlink(void);
int do_lstat(void);
int update_statvfs(struct vmnt *vmp, struct statvfs *buf);

/* time.c */
int do_utimens(void);

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
struct timespec clock_timespec(void);
int copy_path(char *dest, size_t size);
int fetch_name(vir_bytes path, size_t len, char *dest);
int isokendpt_f(const char *f, int l, endpoint_t e, int *p, int ft);
int in_group(struct fproc *rfp, gid_t grp);
int sys_datacopy_wrapper(endpoint_t src, vir_bytes srcv, endpoint_t dst, vir_bytes dstv,
	size_t len);

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
void fetch_vmnt_paths(void);
void upgrade_vmnt_lock(struct vmnt *vmp);
void downgrade_vmnt_lock(struct vmnt *vmp);

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
void upgrade_vnode_lock(struct vnode *vp);

/* write.c */
int do_write(void);

/* gcov.c */
int do_gcov_flush(void);
#if ! USE_COVERAGE
#define do_gcov_flush NULL
#endif

/* select.c */
int do_select(void);
void init_select(void);
void select_callback(struct filp *, int ops);
void select_forget(void);
void select_reply1(endpoint_t driver_e, devminor_t minor, int status);
void select_reply2(endpoint_t driver_e, devminor_t minor, int status);
void select_timeout_check(minix_timer_t *);
void select_unsuspend_by_endpt(endpoint_t proc);

/* worker.c */
void worker_init(void);
int worker_available(void);
struct worker_thread *worker_get(thread_t worker_tid);
void worker_signal(struct worker_thread *worker);
int worker_can_start(struct fproc *rfp);
void worker_start(struct fproc *rfp, void (*func)(void), message *m_ptr,
	int use_spare);
void worker_stop(struct worker_thread *worker);
void worker_stop_by_endpt(endpoint_t proc_e);
void worker_wait(void);
struct worker_thread *worker_suspend(void);
void worker_resume(struct worker_thread *org_self);
void worker_set_proc(struct fproc *rfp);
#endif
