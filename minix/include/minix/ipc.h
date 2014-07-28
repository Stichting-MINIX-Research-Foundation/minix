#ifndef _IPC_H
#define _IPC_H

#include <minix/ipcconst.h>
#include <minix/type.h>
#include <minix/const.h>
#include <sys/signal.h>
#include <sys/types.h>

/*==========================================================================* 
 * Types relating to messages. 						    *
 *==========================================================================*/ 

#define M_PATH_STRING_MAX  40

typedef struct {
	uint8_t data[56];
} mess_u8;
_ASSERT_MSG_SIZE(mess_u8);

typedef struct {
	uint16_t data[28];
} mess_u16;
_ASSERT_MSG_SIZE(mess_u16);

typedef struct {
	uint32_t data[14];
} mess_u32;
_ASSERT_MSG_SIZE(mess_u32);

typedef struct {
	uint64_t data[7];
} mess_u64;
_ASSERT_MSG_SIZE(mess_u64);

typedef struct {
	uint64_t m1ull1;
	int m1i1, m1i2, m1i3;
	char *m1p1, *m1p2, *m1p3, *m1p4;
	uint8_t padding[20];
} mess_1;
_ASSERT_MSG_SIZE(mess_1);

typedef struct {
	int64_t m2ll1;
	int m2i1, m2i2, m2i3;
	long m2l1, m2l2;
	char *m2p1;
	sigset_t sigset;
	short m2s1;
	uint8_t padding[6];
} mess_2;
_ASSERT_MSG_SIZE(mess_2);

typedef struct {
	int m3i1, m3i2;
	char *m3p1;
	char m3ca1[44];
} mess_3;
_ASSERT_MSG_SIZE(mess_3);

typedef struct {
	int64_t m4ll1;
	long m4l1, m4l2, m4l3, m4l4, m4l5;
	uint8_t padding[28];
} mess_4;
_ASSERT_MSG_SIZE(mess_4);

typedef struct {
	int m7i1, m7i2, m7i3, m7i4, m7i5;
	char *m7p1, *m7p2;
	uint8_t padding[28];
} mess_7;
_ASSERT_MSG_SIZE(mess_7);

typedef struct {
	uint64_t m9ull1, m9ull2;
	long m9l1, m9l2, m9l3, m9l4, m9l5;
	short m9s1, m9s2, m9s3, m9s4;
	uint8_t padding[12];
} mess_9;
_ASSERT_MSG_SIZE(mess_9);

typedef struct {
	u64_t m10ull1;
	int m10i1, m10i2, m10i3, m10i4;
	long m10l1, m10l2, m10l3;
	uint8_t padding[20];
} mess_10;
_ASSERT_MSG_SIZE(mess_10);

/* Helper union for DS messages */
union	ds_val {
	cp_grant_id_t	grant;
	u32_t		u32;
	endpoint_t	ep;
};

typedef struct {
	union	ds_val 	val_out;
	int	val_len;
	uint8_t padding[48];
} mess_ds_reply;
_ASSERT_MSG_SIZE(mess_ds_reply);

typedef struct {
	cp_grant_id_t	key_grant;
	int		key_len;
	int		flags;
	union ds_val 	val_in;
	int		val_len;
	endpoint_t	owner;
	uint8_t padding[32];
} mess_ds_req;
_ASSERT_MSG_SIZE(mess_ds_req);

typedef struct {
	off_t seek_pos;

	size_t nbytes;

	uint8_t data[44];
} mess_fs_vfs_breadwrite;
_ASSERT_MSG_SIZE(mess_fs_vfs_breadwrite);

typedef struct {
	mode_t mode;

	uint8_t data[52];
} mess_fs_vfs_chmod;
_ASSERT_MSG_SIZE(mess_fs_vfs_chmod);

typedef struct {
	mode_t mode;

	uint8_t data[52];
} mess_fs_vfs_chown;
_ASSERT_MSG_SIZE(mess_fs_vfs_chown);

typedef struct {
	off_t file_size;
	ino_t inode;

	mode_t mode;
	uid_t uid;
	gid_t gid;

	uint8_t data[28];
} mess_fs_vfs_create;
_ASSERT_MSG_SIZE(mess_fs_vfs_create);

typedef struct {
	off_t seek_pos;

	size_t nbytes;

	uint8_t data[44];
} mess_fs_vfs_getdents;
_ASSERT_MSG_SIZE(mess_fs_vfs_getdents);

typedef struct {
	off_t offset;
	off_t file_size;
	dev_t device;
	ino_t inode;

	mode_t mode;
	uid_t uid;
	gid_t gid;

	uint16_t symloop;

	uint8_t data[10];
} mess_fs_vfs_lookup;
_ASSERT_MSG_SIZE(mess_fs_vfs_lookup);

typedef struct {
	off_t file_size;
	dev_t device;
	ino_t inode;

	mode_t mode;
	uid_t uid;
	gid_t gid;

	uint8_t data[20];
} mess_fs_vfs_newnode;
_ASSERT_MSG_SIZE(mess_fs_vfs_newnode);

typedef struct {
	size_t nbytes;

	uint8_t data[52];
} mess_fs_vfs_rdlink;
_ASSERT_MSG_SIZE(mess_fs_vfs_rdlink);

typedef struct {
	off_t file_size;
	dev_t device;
	ino_t inode;

	uint32_t flags;
	mode_t mode;
	uid_t uid;
	gid_t gid;

	uint16_t con_reqs;

	uint8_t data[14];
} mess_fs_vfs_readsuper;
_ASSERT_MSG_SIZE(mess_fs_vfs_readsuper);

typedef struct {
	off_t seek_pos;

	size_t nbytes;

	uint8_t data[44];
} mess_fs_vfs_readwrite;
_ASSERT_MSG_SIZE(mess_fs_vfs_readwrite);

typedef struct {
	uint8_t padding[56];
} mess_i2c_li2cdriver_busc_i2c_exec;
_ASSERT_MSG_SIZE(mess_i2c_li2cdriver_busc_i2c_exec);

typedef struct {
	uint8_t padding[56];
} mess_i2c_li2cdriver_busc_i2c_reserve;
_ASSERT_MSG_SIZE(mess_i2c_li2cdriver_busc_i2c_reserve);

typedef struct {
	int kbd_id;
	int mouse_id;
	int rsvd1_id;
	int rsvd2_id;

	uint8_t padding[40];
} mess_input_linputdriver_input_conf;
_ASSERT_MSG_SIZE(mess_input_linputdriver_input_conf);

typedef struct {
	uint32_t led_mask;

	uint8_t padding[52];
} mess_input_linputdriver_setleds;
_ASSERT_MSG_SIZE(mess_input_linputdriver_setleds);

typedef struct {
	int id;
	int page;
	int code;
	int value;
	int flags;

	uint8_t padding[36];
} mess_input_tty_event;
_ASSERT_MSG_SIZE(mess_input_tty_event);

typedef struct {
	time_t acnt_queue;

	unsigned long acnt_deqs;
	unsigned long acnt_ipc_sync;
	unsigned long acnt_ipc_async;
	unsigned long acnt_preempt;
	uint32_t acnt_cpu;
	uint32_t acnt_cpu_load;

	uint8_t padding[24];
} mess_krn_lsys_schedule;
_ASSERT_MSG_SIZE(mess_krn_lsys_schedule);

typedef struct {
	uint32_t value;

	uint8_t padding[52];
} mess_krn_lsys_sys_devio;
_ASSERT_MSG_SIZE(mess_krn_lsys_sys_devio);

typedef struct {
	endpoint_t endpt;
	vir_bytes msgaddr;

	uint8_t padding[48];
} mess_krn_lsys_sys_fork;
_ASSERT_MSG_SIZE(mess_krn_lsys_sys_fork);

typedef struct {
	endpoint_t endpt;
	int privflags;
	char name[48];

} mess_krn_lsys_sys_getwhoami;
_ASSERT_MSG_SIZE(mess_krn_lsys_sys_getwhoami);

typedef struct {
	int hook_id;

	uint8_t padding[52];
} mess_krn_lsys_sys_irqctl;
_ASSERT_MSG_SIZE(mess_krn_lsys_sys_irqctl);

typedef struct {
	clock_t real_ticks;
	clock_t boot_ticks;
	clock_t boot_time;
	clock_t user_time;
	clock_t system_time;

	uint8_t padding[36];
} mess_krn_lsys_sys_times;
_ASSERT_MSG_SIZE(mess_krn_lsys_sys_times);

typedef struct {
	long int data;

	uint8_t padding[52];
} mess_krn_lsys_sys_trace;
_ASSERT_MSG_SIZE(mess_krn_lsys_sys_trace);

typedef struct {
	phys_bytes dst_addr;

	uint8_t padding[52];
} mess_krn_lsys_sys_umap;
_ASSERT_MSG_SIZE(mess_krn_lsys_sys_umap);

typedef struct {
	int pcount;

	uint8_t padding[52];
} mess_krn_lsys_sys_vumap;
_ASSERT_MSG_SIZE(mess_krn_lsys_sys_vumap);

typedef struct {
	off_t pos;

	int minor;
	int id;
	int access;

	int count;
	cp_grant_id_t grant;
	int flags;

	endpoint_t user;
	int request;

	uint8_t padding[16];
} mess_lbdev_lblockdriver_msg;
_ASSERT_MSG_SIZE(mess_lbdev_lblockdriver_msg);

typedef struct {
	int status;
	int id;

	uint8_t padding[48];
} mess_lblockdriver_lbdev_reply;
_ASSERT_MSG_SIZE(mess_lblockdriver_lbdev_reply);

typedef struct {
	int		id;
	int		num;
	int		cmd;
	int		opt;
	int		ret;
	uint8_t		padding[36];
} mess_lc_ipc_semctl;
_ASSERT_MSG_SIZE(mess_lc_ipc_semctl);

typedef struct {
	key_t		key;
	int		nr;
	int		flag;
	int		retid;
	uint8_t		padding[40];
} mess_lc_ipc_semget;
_ASSERT_MSG_SIZE(mess_lc_ipc_semget);

typedef struct {
	int		id;
	void		*ops;
	unsigned int	size;
	uint8_t		padding[42];
} mess_lc_ipc_semop;
_ASSERT_MSG_SIZE(mess_lc_ipc_semop);

typedef struct {
	int		id;
	const void	*addr;
	int		flag;
	void		*retaddr;
	uint8_t		padding[40];
} mess_lc_ipc_shmat;
_ASSERT_MSG_SIZE(mess_lc_ipc_shmat);

typedef struct {
	int		id;
	int		cmd;
	void		*buf;
	int		ret;
	uint8_t		padding[40];
} mess_lc_ipc_shmctl;
_ASSERT_MSG_SIZE(mess_lc_ipc_shmctl);

typedef struct {
	const void	*addr;
	uint8_t		padding[52];
} mess_lc_ipc_shmdt;
_ASSERT_MSG_SIZE(mess_lc_ipc_shmdt);

typedef struct {
	key_t		key;
	size_t		size;
	int		flag;
	int		retid;
	uint8_t		padding[40];
} mess_lc_ipc_shmget;
_ASSERT_MSG_SIZE(mess_lc_ipc_shmget);

typedef struct {
	int action;
	vir_bytes ctl_ptr;
	vir_bytes mem_ptr;
	size_t mem_size;

	uint8_t padding[40];
} mess_lc_pm_cprof;
_ASSERT_MSG_SIZE(mess_lc_pm_cprof);

typedef struct {
	vir_bytes name;
	size_t namelen;
	vir_bytes frame;
	size_t framelen;
	vir_bytes ps_str;

	uint8_t padding[36];
} mess_lc_pm_exec;
_ASSERT_MSG_SIZE(mess_lc_pm_exec);

typedef struct {
	int status;

	uint8_t padding[52];
} mess_lc_pm_exit;
_ASSERT_MSG_SIZE(mess_lc_pm_exit);

typedef struct {
	pid_t pid;

	uint8_t padding[52];
} mess_lc_pm_getsid;
_ASSERT_MSG_SIZE(mess_lc_pm_getsid);

typedef struct {
	int num;
	vir_bytes ptr;		/* gid_t * */

	uint8_t padding[48];
} mess_lc_pm_groups;
_ASSERT_MSG_SIZE(mess_lc_pm_groups);

typedef struct {
	int which;
	vir_bytes value;	/* const struct itimerval * */
	vir_bytes ovalue;	/* struct itimerval * */

	uint8_t padding[44];
} mess_lc_pm_itimer;
_ASSERT_MSG_SIZE(mess_lc_pm_itimer);

typedef struct {
	vir_bytes ctx;		/* mcontext_t * */

	uint8_t padding[52];
} mess_lc_pm_mcontext;
_ASSERT_MSG_SIZE(mess_lc_pm_mcontext);

typedef struct {
	int which;
	int who;
	int prio;

	uint8_t padding[44];
} mess_lc_pm_priority;
_ASSERT_MSG_SIZE(mess_lc_pm_priority);

typedef struct {
	pid_t pid;
	int req;
	vir_bytes addr;
	long data;

	uint8_t padding[40];
} mess_lc_pm_ptrace;
_ASSERT_MSG_SIZE(mess_lc_pm_ptrace);

typedef struct {
	int how;

	uint8_t padding[52];
} mess_lc_pm_reboot;
_ASSERT_MSG_SIZE(mess_lc_pm_reboot);

typedef struct {
	endpoint_t who;
	vir_bytes addr;

	uint8_t padding[48];
} mess_lc_pm_rusage;
_ASSERT_MSG_SIZE(mess_lc_pm_rusage);

typedef struct {
	gid_t gid;

	uint8_t padding[52];
} mess_lc_pm_setgid;
_ASSERT_MSG_SIZE(mess_lc_pm_setgid);

typedef struct {
	uid_t uid;

	uint8_t padding[52];
} mess_lc_pm_setuid;
_ASSERT_MSG_SIZE(mess_lc_pm_setuid);

typedef struct {
	pid_t pid;
	int nr;
	vir_bytes act;		/* const struct sigaction * */
	vir_bytes oact;		/* struct sigaction * */
	vir_bytes ret;		/* int (*)(void) */

	uint8_t padding[36];
} mess_lc_pm_sig;
_ASSERT_MSG_SIZE(mess_lc_pm_sig);

typedef struct {
	int how;
	vir_bytes ctx;
	sigset_t set;

	uint8_t padding[32];
} mess_lc_pm_sigset;
_ASSERT_MSG_SIZE(mess_lc_pm_sigset);

typedef struct {
	int action;
	int freq;
	int intr_type;
	vir_bytes ctl_ptr;
	vir_bytes mem_ptr;
	size_t mem_size;

	uint8_t padding[32];
} mess_lc_pm_sprof;
_ASSERT_MSG_SIZE(mess_lc_pm_sprof);

typedef struct {
	int req;
	int field;
	size_t len;
	vir_bytes value;

	uint8_t padding[40];
} mess_lc_pm_sysuname;
_ASSERT_MSG_SIZE(mess_lc_pm_sysuname);

typedef struct {
	time_t sec;

	clockid_t clk_id;
	int now;
	long nsec;

	uint8_t padding[36];
} mess_lc_pm_time;
_ASSERT_MSG_SIZE(mess_lc_pm_time);

typedef struct {
	pid_t pid;
	int options;

	uint8_t padding[48];
} mess_lc_pm_waitpid;
_ASSERT_MSG_SIZE(mess_lc_pm_waitpid);

typedef struct {
	cp_grant_id_t grant;
	vir_bytes tm;			/* struct tm * */
	int flags;

	uint8_t padding[44];
} mess_lc_readclock_rtcdev;
_ASSERT_MSG_SIZE(mess_lc_readclock_rtcdev);

typedef struct {
	vir_bytes name;
	size_t len;
	int fd;
	uid_t owner;
	gid_t group;

	uint8_t padding[36];
} mess_lc_vfs_chown;
_ASSERT_MSG_SIZE(mess_lc_vfs_chown);

typedef struct {
	int fd;

	uint8_t padding[52];
} mess_lc_vfs_close;
_ASSERT_MSG_SIZE(mess_lc_vfs_close);

typedef struct {
	vir_bytes name;
	size_t len;
	int flags;
	mode_t mode;

	uint8_t padding[40];
} mess_lc_vfs_creat;
_ASSERT_MSG_SIZE(mess_lc_vfs_creat);

typedef struct {
	int fd;

	uint8_t padding[52];
} mess_lc_vfs_fchdir;
_ASSERT_MSG_SIZE(mess_lc_vfs_fchdir);

typedef struct {
	int fd;
	mode_t mode;

	uint8_t padding[48];
} mess_lc_vfs_fchmod;
_ASSERT_MSG_SIZE(mess_lc_vfs_fchmod);

typedef struct {
	int fd;
	int cmd;
	int arg_int;
	vir_bytes arg_ptr;	/* struct flock * */

	uint8_t padding[40];
} mess_lc_vfs_fcntl;
_ASSERT_MSG_SIZE(mess_lc_vfs_fcntl);

typedef struct {
	int fd;
	vir_bytes buf;		/* struct stat * */

	uint8_t padding[48];
} mess_lc_vfs_fstat;
_ASSERT_MSG_SIZE(mess_lc_vfs_fstat);

typedef struct {
	int fd;

	uint8_t padding[52];
} mess_lc_vfs_fsync;
_ASSERT_MSG_SIZE(mess_lc_vfs_fsync);

typedef struct {
	cp_grant_id_t grant;
	int pid;
	vir_bytes buff_p;
	size_t buff_sz;

	uint8_t padding[40];
} mess_lc_vfs_gcov;
_ASSERT_MSG_SIZE(mess_lc_vfs_gcov);

typedef struct {
	int32_t flags;
	size_t len;
	vir_bytes buf;		/* struct statvfs */

	uint8_t padding[44];
} mess_lc_vfs_getvfsstat;
_ASSERT_MSG_SIZE(mess_lc_vfs_getvfsstat);

typedef struct {
	int fd;
	unsigned long req;
	vir_bytes arg;

	uint8_t padding[44];
} mess_lc_vfs_ioctl;
_ASSERT_MSG_SIZE(mess_lc_vfs_ioctl);

typedef struct {
	vir_bytes name1;
	vir_bytes name2;
	size_t len1;
	size_t len2;

	uint8_t padding[40];
} mess_lc_vfs_link;
_ASSERT_MSG_SIZE(mess_lc_vfs_link);

typedef struct {
	off_t offset;

	int fd;
	int whence;

	uint8_t padding[40];
} mess_lc_vfs_lseek;
_ASSERT_MSG_SIZE(mess_lc_vfs_lseek);

typedef struct {
	dev_t device;

	vir_bytes name;
	size_t len;
	mode_t mode;

	uint8_t padding[36];
} mess_lc_vfs_mknod;
_ASSERT_MSG_SIZE(mess_lc_vfs_mknod);

typedef struct {
	int flags;
	size_t devlen;
	size_t pathlen;
	size_t typelen;
	size_t labellen;
	vir_bytes dev;
	vir_bytes path;
	vir_bytes type;
	vir_bytes label;

	uint8_t padding[20];
} mess_lc_vfs_mount;
_ASSERT_MSG_SIZE(mess_lc_vfs_mount);

typedef struct {
	vir_bytes name;
	size_t len;
	int flags;
	mode_t mode;
	char buf[M_PATH_STRING_MAX];
} mess_lc_vfs_path;
_ASSERT_MSG_SIZE(mess_lc_vfs_path);

typedef struct {
	int fd0;
	int fd1;
	int flags;

	uint8_t padding[44];
} mess_lc_vfs_pipe2;
_ASSERT_MSG_SIZE(mess_lc_vfs_pipe2);

typedef struct {
	vir_bytes name;		/* const char * */
	size_t namelen;
	vir_bytes buf;
	size_t bufsize;

	uint8_t padding[40];
} mess_lc_vfs_readlink;
_ASSERT_MSG_SIZE(mess_lc_vfs_readlink);

typedef struct {
	int fd;
	vir_bytes buf;
	size_t len;

	uint8_t padding[44];
} mess_lc_vfs_readwrite;
_ASSERT_MSG_SIZE(mess_lc_vfs_readwrite);

typedef struct {
	vir_bytes addr;

	uint8_t padding[52];
} mess_lc_vfs_rusage;
_ASSERT_MSG_SIZE(mess_lc_vfs_rusage);

typedef struct {
	uint32_t nfds;
	fd_set *readfds;
	fd_set *writefds;
	fd_set *errorfds;
	vir_bytes timeout;	/* user-provided 'struct timeval *' */

	uint8_t padding[36];
} mess_lc_vfs_select;
_ASSERT_MSG_SIZE(mess_lc_vfs_select);

typedef struct {
	size_t len;
	vir_bytes name;		/* const char * */
	vir_bytes buf;		/* struct stat * */

	uint8_t padding[44];
} mess_lc_vfs_stat;
_ASSERT_MSG_SIZE(mess_lc_vfs_stat);

typedef struct {
	int fd;
	int flags;
	size_t len;
	vir_bytes name;
	vir_bytes buf;

	uint8_t padding[36];
} mess_lc_vfs_statvfs1;
_ASSERT_MSG_SIZE(mess_lc_vfs_statvfs1);

typedef struct {
	off_t offset;

	int fd;
	vir_bytes name;
	size_t len;

	uint8_t padding[36];
} mess_lc_vfs_truncate;
_ASSERT_MSG_SIZE(mess_lc_vfs_truncate);

typedef struct {
	mode_t mask;

	uint8_t padding[52];
} mess_lc_vfs_umask;
_ASSERT_MSG_SIZE(mess_lc_vfs_umask);

typedef struct {
	vir_bytes name;
	size_t namelen;
	vir_bytes label;
	size_t labellen;

	uint8_t padding[40];
} mess_lc_vfs_umount;
_ASSERT_MSG_SIZE(mess_lc_vfs_umount);

typedef struct {
	void		*addr;
	uint8_t		padding[52];
} mess_lc_vm_brk;
_ASSERT_MSG_SIZE(mess_lc_vm_brk);

typedef struct {
	endpoint_t	endpt;
	void		*addr;
	void		*ret_addr;
	uint8_t		padding[44];
} mess_lc_vm_getphys;
_ASSERT_MSG_SIZE(mess_lc_vm_getphys);

typedef struct {
	vir_bytes addr;

	uint8_t padding[52];
} mess_lc_vm_rusage;
_ASSERT_MSG_SIZE(mess_lc_vm_rusage);

typedef struct {
	endpoint_t	forwhom;
	void		*addr;
	uint8_t		padding[48];
} mess_lc_vm_shm_unmap;
_ASSERT_MSG_SIZE(mess_lc_vm_shm_unmap);

typedef struct {
	int status;
	uint32_t id;		/* should be cdev_id_t */

	uint8_t padding[48];
} mess_lchardriver_vfs_reply;
_ASSERT_MSG_SIZE(mess_lchardriver_vfs_reply);

typedef struct {
	int status;
	int32_t minor;

	uint8_t padding[48];
} mess_lchardriver_vfs_sel1;
_ASSERT_MSG_SIZE(mess_lchardriver_vfs_sel1);

typedef struct {
	int status;
	int32_t minor;

	uint8_t padding[48];
} mess_lchardriver_vfs_sel2;
_ASSERT_MSG_SIZE(mess_lchardriver_vfs_sel2);

typedef struct {
	endpoint_t endpt;
	vir_bytes ptr;		/* struct exec_info * */

	uint8_t padding[48];
} mess_lexec_pm_exec_new;
_ASSERT_MSG_SIZE(mess_lexec_pm_exec_new);

typedef struct {
	cp_grant_id_t grant;

	uint8_t padding[52];
} mess_li2cdriver_i2c_busc_i2c_exec;
_ASSERT_MSG_SIZE(mess_li2cdriver_i2c_busc_i2c_exec);

typedef struct {
	uint16_t addr; /* FIXME: strictly speaking this is an i2c_addr_t, but
			  to get it I would need to include
			  sys/dev/i2c/i2c_io.h, which I am not sure is a good
			  idea to have everywhere. */

	uint8_t padding[54];
} mess_li2cdriver_i2c_busc_i2c_reserve;
_ASSERT_MSG_SIZE(mess_li2cdriver_i2c_busc_i2c_reserve);

typedef struct {
	int id;
	int page;
	int code;
	int value;
	int flags;

	uint8_t padding[36];
} mess_linputdriver_input_event;
_ASSERT_MSG_SIZE(mess_linputdriver_input_event);

typedef struct {
        cp_grant_id_t gid;
	size_t size;

        uint8_t padding[48];
} mess_lsys_fi_ctl;
_ASSERT_MSG_SIZE(mess_lsys_fi_ctl);

typedef struct {
        int status;

        uint8_t padding[52];
} mess_lsys_fi_reply;
_ASSERT_MSG_SIZE(mess_lsys_fi_reply);

typedef struct {
	int what;
	vir_bytes where;
	size_t size;

	uint8_t padding[44];
} mess_lsys_getsysinfo;
_ASSERT_MSG_SIZE(mess_lsys_getsysinfo);

typedef struct {
	size_t size;
	phys_bytes addr;
	vir_bytes buf;

	uint8_t padding[44];
} mess_lsys_krn_readbios;
_ASSERT_MSG_SIZE(mess_lsys_krn_readbios);

typedef struct {
	endpoint_t	from_to;
	cp_grant_id_t	gid;
	size_t		offset;
	void		*address;
	size_t		bytes;
	uint8_t padding[36];
} mess_lsys_kern_safecopy;
_ASSERT_MSG_SIZE(mess_lsys_kern_safecopy);

typedef struct {
	uint32_t flags;
	endpoint_t endpoint;
	int priority;
	int quantum;
	int cpu;

	uint8_t padding[36];
} mess_lsys_krn_schedctl;
_ASSERT_MSG_SIZE(mess_lsys_krn_schedctl);

typedef struct {
	endpoint_t endpoint;
	int quantum;
	int priority;
	int cpu;

	uint8_t padding[40];
} mess_lsys_krn_schedule;
_ASSERT_MSG_SIZE(mess_lsys_krn_schedule);

typedef struct {
	int how;

	uint8_t padding[52];
} mess_lsys_krn_sys_abort;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_abort);

typedef struct {
	endpoint_t endpt;

	uint8_t padding[52];
} mess_lsys_krn_sys_clear;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_clear);

typedef struct {
	endpoint_t src_endpt;
	vir_bytes src_addr;
	endpoint_t dst_endpt;
	vir_bytes dst_addr;
	phys_bytes nr_bytes;
	int flags;

	uint8_t padding[32];
} mess_lsys_krn_sys_copy;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_copy);

typedef struct {
	endpoint_t endpt;
	int action;
	vir_bytes ctl_ptr;
	vir_bytes mem_ptr;
	size_t mem_size;

	uint8_t padding[36];
} mess_lsys_krn_sys_cprof;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_cprof);

typedef struct {
	int request;
	int port;
	uint32_t value;

	uint8_t padding[44];
} mess_lsys_krn_sys_devio;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_devio);

typedef struct {
	int code;
	vir_bytes buf;
	int len;
	endpoint_t endpt;

	uint8_t padding[40];
} mess_lsys_krn_sys_diagctl;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_diagctl);

typedef struct {
	endpoint_t endpt;
	vir_bytes ip;
	vir_bytes stack;
	vir_bytes name;
	vir_bytes ps_str;

	uint8_t padding[36];
} mess_lsys_krn_sys_exec;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_exec);

typedef struct {
	endpoint_t endpt;
	endpoint_t slot;
	uint32_t flags;

	uint8_t padding[44];
} mess_lsys_krn_sys_fork;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_fork);

typedef struct {
	int request;
	endpoint_t endpt;
	vir_bytes val_ptr;
	int val_len;
	vir_bytes val_ptr2;
	int val_len2_e;

	uint8_t padding[32];
} mess_lsys_krn_sys_getinfo;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_getinfo);

typedef struct {
	endpoint_t endpt;
	vir_bytes ctx_ptr;

	uint8_t padding[48];
} mess_lsys_krn_sys_getmcontext;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_getmcontext);

typedef struct {
	endpoint_t endpt;

	uint8_t padding[52];
} mess_lsys_krn_sys_iopenable;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_iopenable);

typedef struct {
	int request;
	int vector;
	int policy;
	int hook_id;

	uint8_t padding[40];
} mess_lsys_krn_sys_irqctl;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_irqctl);

typedef struct {
	phys_bytes base;
	phys_bytes count;
	unsigned long pattern;
	endpoint_t process;

	uint8_t padding[40];
} mess_lsys_krn_sys_memset;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_memset);

typedef struct {
	int request;
	endpoint_t endpt;
	vir_bytes arg_ptr;
	phys_bytes phys_start;
	phys_bytes phys_len;

	uint8_t padding[36];
} mess_lsys_krn_sys_privctl;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_privctl);

typedef struct {
	vir_bytes ctl_ptr;
	vir_bytes mem_ptr;

	uint8_t padding[48];
} mess_lsys_krn_sys_profbuf;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_profbuf);

typedef struct {
	int request;
	long int port;
	endpoint_t vec_endpt;
	phys_bytes vec_addr;
	vir_bytes vec_size;
	vir_bytes offset;

	uint8_t padding[32];
} mess_lsys_krn_sys_sdevio;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_sdevio);

typedef struct {
	clock_t exp_time;
	clock_t time_left;
	int abs_time;

	uint8_t padding[44];
} mess_lsys_krn_sys_setalarm;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_setalarm);

typedef struct {
	vir_bytes addr;			/* cp_grant_t * */
	int size;

	uint8_t padding[48];
} mess_lsys_krn_sys_setgrant;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_setgrant);

typedef struct {
	endpoint_t endpt;
	vir_bytes ctx_ptr;

	uint8_t padding[48];
} mess_lsys_krn_sys_setmcontext;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_setmcontext);

typedef struct {
	time_t sec;		/* time in seconds since 1970 */
	long int nsec;
	int now;		/* non-zero for immediate, 0 for adjtime */
	clockid_t clock_id;

	uint8_t padding[36];
} mess_lsys_krn_sys_settime;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_settime);

typedef struct {
	int action;
	int freq;
	int intr_type;
	endpoint_t endpt;
	vir_bytes ctl_ptr;
	vir_bytes mem_ptr;
	size_t mem_size;

	uint8_t padding[28];
} mess_lsys_krn_sys_sprof;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_sprof);

typedef struct {
	int request;

	uint8_t padding[52];
} mess_lsys_krn_sys_statectl;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_statectl);

typedef struct {
	time_t boot_time;

	uint8_t padding[48];
} mess_lsys_krn_sys_stime;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_stime);

typedef struct {
	endpoint_t endpt;

	uint8_t padding[52];
} mess_lsys_krn_sys_times;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_times);

typedef struct {
	int request;
	endpoint_t endpt;
	vir_bytes address;
	long int data;

	uint8_t padding[40];
} mess_lsys_krn_sys_trace;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_trace);

typedef struct {
	endpoint_t src_endpt;
	int segment;
	vir_bytes src_addr;
	endpoint_t dst_endpt;
	int nr_bytes;

	uint8_t padding[36];
} mess_lsys_krn_sys_umap;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_umap);


typedef struct {
	int request;
	int vec_size;
	vir_bytes vec_addr;		/* pv{b,w,l}_pair_t * */

	uint8_t padding[44];
} mess_lsys_krn_sys_vdevio;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_vdevio);

typedef struct {
	endpoint_t endpt;
	vir_bytes vaddr;			/* struct vumap_vir * */
	int vcount;
	vir_bytes paddr;			/* struct vumap_phys * */
	int pmax;
	int access;
	size_t offset;

	uint8_t padding[28];
} mess_lsys_krn_sys_vumap;
_ASSERT_MSG_SIZE(mess_lsys_krn_sys_vumap);

typedef struct {
	void		*vec_addr;
	int		vec_size;
	uint8_t padding[48];
} mess_lsys_kern_vsafecopy;
_ASSERT_MSG_SIZE(mess_lsys_kern_vsafecopy);

typedef struct {
	int devind;
	int port;

	uint8_t padding[48];
} mess_lsys_pci_busc_get_bar;
_ASSERT_MSG_SIZE(mess_lsys_pci_busc_get_bar);

typedef struct {
	endpoint_t endpt;

	uint8_t padding[52];
} mess_lsys_pm_getepinfo;
_ASSERT_MSG_SIZE(mess_lsys_pm_getepinfo);

typedef struct {
	pid_t pid;

	uint8_t padding[52];
} mess_lsys_pm_getprocnr;
_ASSERT_MSG_SIZE(mess_lsys_pm_getprocnr);

typedef struct {
	uid_t uid;
	gid_t gid;

	uint8_t padding[48];
} mess_lsys_pm_srv_fork;
_ASSERT_MSG_SIZE(mess_lsys_pm_srv_fork);

typedef struct {
	endpoint_t endpoint;
	endpoint_t parent;
	int maxprio;
	int quantum;

	uint8_t padding[40];
} mess_lsys_sched_scheduling_start;
_ASSERT_MSG_SIZE(mess_lsys_sched_scheduling_start);

typedef struct {
	endpoint_t endpoint;

	uint8_t padding[52];
} mess_lsys_sched_scheduling_stop;
_ASSERT_MSG_SIZE(mess_lsys_sched_scheduling_stop);

typedef struct {
	int request;
	vir_bytes arg;

	uint8_t padding[48];
} mess_lsys_svrctl;
_ASSERT_MSG_SIZE(mess_lsys_svrctl);

typedef struct {
	int request;
	int fkeys;
	int sfkeys;

	uint8_t padding[44];
} mess_lsys_tty_fkey_ctl;
_ASSERT_MSG_SIZE(mess_lsys_tty_fkey_ctl);

typedef struct {
	endpoint_t endpt;
	cp_grant_id_t grant;
	size_t count;

	uint8_t padding[44];
} mess_lsys_vfs_checkperms;
_ASSERT_MSG_SIZE(mess_lsys_vfs_checkperms);

typedef struct {
	endpoint_t endpt;
	int fd;
	int what;

	uint8_t padding[44];
} mess_lsys_vfs_copyfd;
_ASSERT_MSG_SIZE(mess_lsys_vfs_copyfd);

typedef struct {
	devmajor_t major;
	size_t labellen;
	vir_bytes label;

	uint8_t padding[44];
} mess_lsys_vfs_mapdriver;
_ASSERT_MSG_SIZE(mess_lsys_vfs_mapdriver);

typedef struct {
	endpoint_t	endpt;
	void		*addr;
	int		retc;
	uint8_t		padding[44];
} mess_lsys_vm_getref;
_ASSERT_MSG_SIZE(mess_lsys_vm_getref);

typedef struct {
	int		what;
	endpoint_t	ep;
	int		count;
	void		*ptr;
	vir_bytes	next;
	uint8_t		padding[36];
} mess_lsys_vm_info;
_ASSERT_MSG_SIZE(mess_lsys_vm_info);

typedef struct {
	endpoint_t	ep;
	phys_bytes	phaddr;
	size_t		len;
	void		*reply;
	uint8_t		padding[40];
} mess_lsys_vm_map_phys;
_ASSERT_MSG_SIZE(mess_lsys_vm_map_phys);

typedef struct {
	endpoint_t	ret_pt;
	int		is_more;
	uint8_t		padding[48];
} mess_lsys_vm_query_exit;
_ASSERT_MSG_SIZE(mess_lsys_vm_query_exit);

typedef struct {
	endpoint_t	ep;
	void		*vaddr;
	uint8_t		padding[48];
} mess_lsys_vm_unmap_phys;
_ASSERT_MSG_SIZE(mess_lsys_vm_unmap_phys);

typedef struct {
	endpoint_t src;
	endpoint_t dst;
	uint8_t		padding[48];
} mess_lsys_vm_update;
_ASSERT_MSG_SIZE(mess_lsys_vm_update);

typedef struct {
	endpoint_t	destination;
	endpoint_t	source;
	void		*dest_addr;
	void		*src_addr;
	size_t		size;
	void		*ret_addr;
	uint8_t		padding[32];
} mess_lsys_vm_vmremap;
_ASSERT_MSG_SIZE(mess_lsys_vm_vmremap);

typedef struct {
	endpoint_t	ep;
	uint8_t		padding[52];
} mess_lsys_vm_watch_exit;
_ASSERT_MSG_SIZE(mess_lsys_vm_watch_exit);

typedef struct {
	off_t offset;
	void *addr;
	size_t len;
	int prot;
	int flags;
	int fd;
	endpoint_t forwhom;
	void *retaddr;
	u32_t padding[5];
} mess_mmap;
_ASSERT_MSG_SIZE(mess_mmap);

typedef struct {
	int mode;

	uint8_t padding[52];
} mess_net_netdrv_dl_conf;
_ASSERT_MSG_SIZE(mess_net_netdrv_dl_conf);

typedef struct {
	cp_grant_id_t grant;

	uint8_t padding[52];
} mess_net_netdrv_dl_getstat_s;
_ASSERT_MSG_SIZE(mess_net_netdrv_dl_getstat_s);

typedef struct {
	cp_grant_id_t grant;
	int count;

	uint8_t padding[48];
} mess_net_netdrv_dl_readv_s;
_ASSERT_MSG_SIZE(mess_net_netdrv_dl_readv_s);

typedef struct {
	cp_grant_id_t grant;
	int count;

	uint8_t padding[48];
} mess_net_netdrv_dl_writev_s;
_ASSERT_MSG_SIZE(mess_net_netdrv_dl_writev_s);

typedef struct {
	int stat;
	uint8_t hw_addr[6];

	uint8_t padding[46];
} mess_netdrv_net_dl_conf;
_ASSERT_MSG_SIZE(mess_netdrv_net_dl_conf);

typedef struct {
	int count;
	uint32_t flags;

	uint8_t padding[48];
} mess_netdrv_net_dl_task;
_ASSERT_MSG_SIZE(mess_netdrv_net_dl_task);

typedef struct {
	u64_t timestamp;	/* valid for every notify msg */
	u64_t interrupts;	/* raised interrupts; valid if from HARDWARE */
	sigset_t sigset;	/* raised signals; valid if from SYSTEM */
	uint8_t padding[24];
} mess_notify;
_ASSERT_MSG_SIZE(mess_notify);

typedef struct {
	int base;
	size_t size;
	uint32_t flags;

	uint8_t padding[44];
} mess_pci_lsys_busc_get_bar;
_ASSERT_MSG_SIZE(mess_pci_lsys_busc_get_bar);

typedef struct {
	uid_t egid;

	uint8_t padding[52];
} mess_pm_lc_getgid;
_ASSERT_MSG_SIZE(mess_pm_lc_getgid);

typedef struct {
	pid_t parent_pid;

	uint8_t padding[52];
} mess_pm_lc_getpid;
_ASSERT_MSG_SIZE(mess_pm_lc_getpid);

typedef struct {
	uid_t euid;

	uint8_t padding[52];
} mess_pm_lc_getuid;
_ASSERT_MSG_SIZE(mess_pm_lc_getuid);

typedef struct {
	long data;

	uint8_t padding[52];
} mess_pm_lc_ptrace;
_ASSERT_MSG_SIZE(mess_pm_lc_ptrace);

typedef struct {
	sigset_t set;

	uint8_t padding[40];
} mess_pm_lc_sigset;
_ASSERT_MSG_SIZE(mess_pm_lc_sigset);

typedef struct {
	time_t sec;

	long nsec;

	uint8_t padding[44];
} mess_pm_lc_time;
_ASSERT_MSG_SIZE(mess_pm_lc_time);

typedef struct {
	int status;

	uint8_t padding[52];
} mess_pm_lc_waitpid;
_ASSERT_MSG_SIZE(mess_pm_lc_waitpid);

typedef struct {
	int suid;

	uint8_t padding[52];
} mess_pm_lexec_exec_new;
_ASSERT_MSG_SIZE(mess_pm_lexec_exec_new);

typedef struct {
	uid_t uid;
	gid_t gid;

	uint8_t padding[48];
} mess_pm_lsys_getepinfo;
_ASSERT_MSG_SIZE(mess_pm_lsys_getepinfo);

typedef struct {
	endpoint_t endpt;

	uint8_t padding[52];
} mess_pm_lsys_getprocnr;
_ASSERT_MSG_SIZE(mess_pm_lsys_getprocnr);

typedef struct {
	int num;

	uint8_t padding[52];
} mess_pm_lsys_sigs_signal;
_ASSERT_MSG_SIZE(mess_pm_lsys_sigs_signal);

typedef struct {
	endpoint_t endpoint;
	uint32_t maxprio;

	uint8_t padding[48];
} mess_pm_sched_scheduling_set_nice;
_ASSERT_MSG_SIZE(mess_pm_sched_scheduling_set_nice);

typedef struct {
	int status;

	uint8_t padding[52];
} mess_readclock_lc_rtcdev;
_ASSERT_MSG_SIZE(mess_readclock_lc_rtcdev);

typedef struct {
	int		result;
	int		type;
	cp_grant_id_t	rproctab_gid;
	endpoint_t	old_endpoint;
	uint8_t padding[40];
} mess_rs_init;
_ASSERT_MSG_SIZE(mess_rs_init);

typedef struct {
	endpoint_t endpt;
	int result;
	vir_bytes pc;
	vir_bytes ps_str;

	uint8_t padding[40];
} mess_rs_pm_exec_restart;
_ASSERT_MSG_SIZE(mess_rs_pm_exec_restart);

typedef struct {
	pid_t pid;
	int nr;

	uint8_t padding[48];
} mess_rs_pm_srv_kill;
_ASSERT_MSG_SIZE(mess_rs_pm_srv_kill);

typedef struct {
	int		len;
	int		name_len;
	endpoint_t	endpoint;
	void		*addr;
	const char	*name;
	uint8_t padding[36];
} mess_rs_req;
_ASSERT_MSG_SIZE(mess_rs_req);

typedef struct {
	int		result;
	int		state;
	int		prepare_maxtime;
	uint8_t padding[44];
} mess_rs_update;
_ASSERT_MSG_SIZE(mess_rs_update);

typedef struct {
	endpoint_t scheduler;

	uint8_t padding[52];
} mess_sched_lsys_scheduling_start;
_ASSERT_MSG_SIZE(mess_sched_lsys_scheduling_start);

/* For SYS_GETKSIG, _ENDKSIG, _KILL, _SIGSEND, _SIGRETURN. */
typedef struct {
	sigset_t map;		/* used to pass signal bit map */
	endpoint_t endpt;	/* process number for inform */
	int sig;		/* signal number to send */
	void *sigctx;		/* pointer to signal context */
	uint8_t padding[28];
} mess_sigcalls;
_ASSERT_MSG_SIZE(mess_sigcalls);

typedef struct {
	int fkeys;
	int sfkeys;

	uint8_t padding[48];
} mess_tty_lsys_fkey_ctl;
_ASSERT_MSG_SIZE(mess_tty_lsys_fkey_ctl);

typedef struct {
	dev_t device;
	off_t seek_pos;

	cp_grant_id_t grant;
	size_t nbytes;

	uint8_t data[32];
} mess_vfs_fs_breadwrite;
_ASSERT_MSG_SIZE(mess_vfs_fs_breadwrite);

typedef struct {
	ino_t inode;

	mode_t mode;

	uint8_t data[44];
} mess_vfs_fs_chmod;
_ASSERT_MSG_SIZE(mess_vfs_fs_chmod);

typedef struct {
	ino_t inode;

	uid_t uid;
	gid_t gid;

	uint8_t data[40];
} mess_vfs_fs_chown;
_ASSERT_MSG_SIZE(mess_vfs_fs_chown);

typedef struct {
	ino_t inode;

	mode_t mode;
	uid_t uid;
	gid_t gid;
	cp_grant_id_t grant;
	size_t path_len;

	uint8_t data[28];
} mess_vfs_fs_create;
_ASSERT_MSG_SIZE(mess_vfs_fs_create);

typedef struct {
	dev_t device;

	uint8_t data[48];
} mess_vfs_fs_flush;
_ASSERT_MSG_SIZE(mess_vfs_fs_flush);

typedef struct {
	ino_t inode;
	off_t trc_start;
	off_t trc_end;

	uint8_t data[32];
} mess_vfs_fs_ftrunc;
_ASSERT_MSG_SIZE(mess_vfs_fs_ftrunc);

typedef struct {
	ino_t inode;
	off_t seek_pos;

	cp_grant_id_t grant;
	size_t mem_size;

	uint8_t data[32];
} mess_vfs_fs_getdents;
_ASSERT_MSG_SIZE(mess_vfs_fs_getdents);

typedef struct {
	ino_t inode;

	uint8_t data[48];
} mess_vfs_fs_inhibread;
_ASSERT_MSG_SIZE(mess_vfs_fs_inhibread);

typedef struct {
	ino_t inode;
	ino_t dir_ino;

	cp_grant_id_t grant;
	size_t path_len;

	uint8_t data[32];
} mess_vfs_fs_link;
_ASSERT_MSG_SIZE(mess_vfs_fs_link);

typedef struct {
	ino_t dir_ino;
	ino_t root_ino;

	uint32_t flags;
	size_t path_len;
	size_t path_size;
	size_t ucred_size;
	cp_grant_id_t grant_path;
	cp_grant_id_t grant_ucred;
	uid_t uid;
	gid_t gid;

	uint8_t data[8];
} mess_vfs_fs_lookup;
_ASSERT_MSG_SIZE(mess_vfs_fs_lookup);

typedef struct {
	ino_t inode;

	mode_t mode;
	uid_t uid;
	gid_t gid;
	cp_grant_id_t grant;
	size_t path_len;

	uint8_t data[28];
} mess_vfs_fs_mkdir;
_ASSERT_MSG_SIZE(mess_vfs_fs_mkdir);

typedef struct {
	dev_t device;
	ino_t inode;

	mode_t mode;
	uid_t uid;
	gid_t gid;
	cp_grant_id_t grant;
	size_t path_len;

	uint8_t data[20];
} mess_vfs_fs_mknod;
_ASSERT_MSG_SIZE(mess_vfs_fs_mknod);

typedef struct {
	ino_t inode;

	uint8_t data[48];
} mess_vfs_fs_mountpoint;
_ASSERT_MSG_SIZE(mess_vfs_fs_mountpoint);

typedef struct {
	dev_t device;

	cp_grant_id_t grant;
	size_t path_len;

	uint8_t data[40];
} mess_vfs_fs_new_driver;
_ASSERT_MSG_SIZE(mess_vfs_fs_new_driver);

typedef struct {
	dev_t device;

	mode_t mode;
	uid_t uid;
	gid_t gid;

	uint8_t data[36];
} mess_vfs_fs_newnode;
_ASSERT_MSG_SIZE(mess_vfs_fs_newnode);

typedef struct {
	uint64_t count;
	ino_t inode;

	uint8_t data[40];
} mess_vfs_fs_putnode;
_ASSERT_MSG_SIZE(mess_vfs_fs_putnode);

typedef struct {
	ino_t inode;

	cp_grant_id_t grant;
	size_t mem_size;

	uint8_t data[40];
} mess_vfs_fs_rdlink;
_ASSERT_MSG_SIZE(mess_vfs_fs_rdlink);

typedef struct {
	dev_t device;

	uint32_t flags;
	size_t path_len;
	cp_grant_id_t grant;

	uint8_t data[36];
} mess_vfs_fs_readsuper;
_ASSERT_MSG_SIZE(mess_vfs_fs_readsuper);

typedef struct {
	ino_t inode;
	off_t seek_pos;

	cp_grant_id_t grant;
	size_t nbytes;

	uint8_t data[32];
} mess_vfs_fs_readwrite;
_ASSERT_MSG_SIZE(mess_vfs_fs_readwrite);

typedef struct {
	ino_t dir_old;
	ino_t dir_new;

	size_t len_old;
	size_t len_new;
	cp_grant_id_t grant_old;
	cp_grant_id_t grant_new;

	uint8_t data[24];
} mess_vfs_fs_rename;
_ASSERT_MSG_SIZE(mess_vfs_fs_rename);

typedef struct {
	ino_t inode;

	size_t path_len;
	size_t mem_size;
	cp_grant_id_t grant_path;
	cp_grant_id_t grant_target;
	uid_t uid;
	gid_t gid;

	uint8_t data[24];
} mess_vfs_fs_slink;
_ASSERT_MSG_SIZE(mess_vfs_fs_slink);

typedef struct {
	ino_t inode;

	cp_grant_id_t grant;

	uint8_t data[44];
} mess_vfs_fs_stat;
_ASSERT_MSG_SIZE(mess_vfs_fs_stat);

typedef struct {
	cp_grant_id_t grant;

	uint8_t data[52];
} mess_vfs_fs_statvfs;
_ASSERT_MSG_SIZE(mess_vfs_fs_statvfs);

typedef struct {
	ino_t inode;

	cp_grant_id_t grant;
	size_t path_len;

	uint8_t data[40];
} mess_vfs_fs_unlink;
_ASSERT_MSG_SIZE(mess_vfs_fs_unlink);

typedef struct {
	ino_t inode;
	time_t actime;
	time_t modtime;

	uint32_t acnsec;
	uint32_t modnsec;

	uint8_t data[24];
} mess_vfs_fs_utime;
_ASSERT_MSG_SIZE(mess_vfs_fs_utime);

typedef struct {
	off_t offset;

	uint8_t padding[48];
} mess_vfs_lc_lseek;
_ASSERT_MSG_SIZE(mess_vfs_lc_lseek);

typedef struct {
	endpoint_t id;
	devminor_t minor;

	uint8_t padding[48];
} mess_vfs_lchardriver_cancel;
_ASSERT_MSG_SIZE(mess_vfs_lchardriver_cancel);

typedef struct {
	endpoint_t id;
	endpoint_t user;
	devminor_t minor;
	int access;

	uint8_t padding[40];
} mess_vfs_lchardriver_openclose;
_ASSERT_MSG_SIZE(mess_vfs_lchardriver_openclose);

typedef struct {
	off_t pos;
	cp_grant_id_t grant;
	size_t count;
	int request;
	int flags;
	endpoint_t id;
	endpoint_t user;
	devminor_t minor;

	uint8_t padding[20];
} mess_vfs_lchardriver_readwrite;
_ASSERT_MSG_SIZE(mess_vfs_lchardriver_readwrite);

typedef struct {
	devminor_t minor;
	int ops;

	uint8_t padding[48];
} mess_vfs_lchardriver_select;
_ASSERT_MSG_SIZE(mess_vfs_lchardriver_select);

typedef struct {
	time_t atime;
	time_t mtime;
	long ansec;
	long mnsec;
	size_t len;
	char *name;
	int fd;
	int flags;
	uint8_t padding[16];
} mess_vfs_utimens;
_ASSERT_MSG_SIZE(mess_vfs_utimens);

typedef struct {
	off_t offset;
	dev_t dev;
	ino_t ino;
	endpoint_t who;
	u32_t vaddr;
	u32_t len;
	u32_t flags;
	u32_t fd;
	u16_t clearend;
	uint8_t padding[8];
} mess_vm_vfs_mmap;
_ASSERT_MSG_SIZE(mess_vm_vfs_mmap);

typedef struct {
	dev_t dev;	/* 64bits long. */
	off_t dev_offset;
	off_t ino_offset;
	ino_t ino;
	void *block;
	u32_t *flags_ptr;
	u8_t pages;
	u8_t flags;
	uint8_t padding[12];
} mess_vmmcp;
_ASSERT_MSG_SIZE(mess_vmmcp);

typedef struct {
	void *addr;
	u8_t flags;
	uint8_t padding[51];
} mess_vmmcp_reply;
_ASSERT_MSG_SIZE(mess_vmmcp_reply);

typedef struct {
	endpoint_t m_source;		/* who sent the message */
	int m_type;			/* what kind of message is it */
	union {
		mess_u8			m_u8;
		mess_u16		m_u16;
		mess_u32		m_u32;
		mess_u64		m_u64;

		mess_1			m_m1;
		mess_2			m_m2;
		mess_3			m_m3;
		mess_4			m_m4;
		mess_7			m_m7;
		mess_9			m_m9;
		mess_10			m_m10;

		mess_ds_reply		m_ds_reply;
		mess_ds_req		m_ds_req;
		mess_fs_vfs_breadwrite	m_fs_vfs_breadwrite;
		mess_fs_vfs_chmod	m_fs_vfs_chmod;
		mess_fs_vfs_chown	m_fs_vfs_chown;
		mess_fs_vfs_create	m_fs_vfs_create;
		mess_fs_vfs_getdents	m_fs_vfs_getdents;
		mess_fs_vfs_lookup	m_fs_vfs_lookup;
		mess_fs_vfs_newnode	m_fs_vfs_newnode;
		mess_fs_vfs_rdlink	m_fs_vfs_rdlink;
		mess_fs_vfs_readsuper	m_fs_vfs_readsuper;
		mess_fs_vfs_readwrite	m_fs_vfs_readwrite;
		mess_i2c_li2cdriver_busc_i2c_exec m_i2c_li2cdriver_busc_i2c_exec;
		mess_i2c_li2cdriver_busc_i2c_reserve m_i2c_li2cdriver_busc_i2c_reserve;
		mess_input_linputdriver_input_conf m_input_linputdriver_input_conf;
		mess_input_linputdriver_setleds m_input_linputdriver_setleds;
		mess_input_tty_event	m_input_tty_event;
		mess_krn_lsys_schedule	m_krn_lsys_schedule;
		mess_krn_lsys_sys_devio m_krn_lsys_sys_devio;
		mess_krn_lsys_sys_fork m_krn_lsys_sys_fork;
		mess_krn_lsys_sys_getwhoami m_krn_lsys_sys_getwhoami;
		mess_krn_lsys_sys_irqctl m_krn_lsys_sys_irqctl;
		mess_krn_lsys_sys_times	m_krn_lsys_sys_times;
		mess_krn_lsys_sys_trace	m_krn_lsys_sys_trace;
		mess_krn_lsys_sys_umap	m_krn_lsys_sys_umap;
		mess_krn_lsys_sys_vumap	m_krn_lsys_sys_vumap;
		mess_lbdev_lblockdriver_msg m_lbdev_lblockdriver_msg;
		mess_lblockdriver_lbdev_reply m_lblockdriver_lbdev_reply;
		mess_lc_ipc_semctl	m_lc_ipc_semctl;
		mess_lc_ipc_semget	m_lc_ipc_semget;
		mess_lc_ipc_semop	m_lc_ipc_semop;
		mess_lc_ipc_shmat	m_lc_ipc_shmat;
		mess_lc_ipc_shmctl	m_lc_ipc_shmctl;
		mess_lc_ipc_shmdt	m_lc_ipc_shmdt;
		mess_lc_ipc_shmget	m_lc_ipc_shmget;
		mess_lc_pm_cprof	m_lc_pm_cprof;
		mess_lc_pm_exec		m_lc_pm_exec;
		mess_lc_pm_exit		m_lc_pm_exit;
		mess_lc_pm_getsid	m_lc_pm_getsid;
		mess_lc_pm_groups	m_lc_pm_groups;
		mess_lc_pm_itimer	m_lc_pm_itimer;
		mess_lc_pm_mcontext	m_lc_pm_mcontext;
		mess_lc_pm_priority	m_lc_pm_priority;
		mess_lc_pm_ptrace	m_lc_pm_ptrace;
		mess_lc_pm_reboot	m_lc_pm_reboot;
		mess_lc_pm_rusage	m_lc_pm_rusage;
		mess_lc_pm_setgid	m_lc_pm_setgid;
		mess_lc_pm_setuid	m_lc_pm_setuid;
		mess_lc_pm_sig		m_lc_pm_sig;
		mess_lc_pm_sigset	m_lc_pm_sigset;
		mess_lc_pm_sprof	m_lc_pm_sprof;
		mess_lc_pm_sysuname	m_lc_pm_sysuname;
		mess_lc_pm_time		m_lc_pm_time;
		mess_lc_pm_waitpid	m_lc_pm_waitpid;
		mess_lc_readclock_rtcdev m_lc_readclock_rtcdev;
		mess_lc_vfs_chown	m_lc_vfs_chown;
		mess_lc_vfs_close	m_lc_vfs_close;
		mess_lc_vfs_creat	m_lc_vfs_creat;
		mess_lc_vfs_fchdir	m_lc_vfs_fchdir;
		mess_lc_vfs_fchmod	m_lc_vfs_fchmod;
		mess_lc_vfs_fcntl	m_lc_vfs_fcntl;
		mess_lc_vfs_fstat	m_lc_vfs_fstat;
		mess_lc_vfs_fsync	m_lc_vfs_fsync;
		mess_lc_vfs_gcov	m_lc_vfs_gcov;
		mess_lc_vfs_getvfsstat	m_lc_vfs_getvfsstat;
		mess_lc_vfs_ioctl	m_lc_vfs_ioctl;
		mess_lc_vfs_link	m_lc_vfs_link;
		mess_lc_vfs_lseek	m_lc_vfs_lseek;
		mess_lc_vfs_mknod	m_lc_vfs_mknod;
		mess_lc_vfs_mount	m_lc_vfs_mount;
		mess_lc_vfs_path	m_lc_vfs_path;
		mess_lc_vfs_pipe2	m_lc_vfs_pipe2;
		mess_lc_vfs_readlink	m_lc_vfs_readlink;
		mess_lc_vfs_readwrite	m_lc_vfs_readwrite;
		mess_lc_vfs_rusage	m_lc_vfs_rusage;
		mess_lc_vfs_select	m_lc_vfs_select;
		mess_lc_vfs_stat	m_lc_vfs_stat;
		mess_lc_vfs_statvfs1	m_lc_vfs_statvfs1;
		mess_lc_vfs_truncate	m_lc_vfs_truncate;
		mess_lc_vfs_umask	m_lc_vfs_umask;
		mess_lc_vfs_umount	m_lc_vfs_umount;
		mess_lc_vm_brk		m_lc_vm_brk;
		mess_lc_vm_getphys	m_lc_vm_getphys;
		mess_lc_vm_rusage	m_lc_vm_rusage;
		mess_lc_vm_shm_unmap	m_lc_vm_shm_unmap;
		mess_lchardriver_vfs_reply m_lchardriver_vfs_reply;
		mess_lchardriver_vfs_sel1 m_lchardriver_vfs_sel1;
		mess_lchardriver_vfs_sel2 m_lchardriver_vfs_sel2;
		mess_lexec_pm_exec_new	m_lexec_pm_exec_new;
		mess_li2cdriver_i2c_busc_i2c_exec m_li2cdriver_i2c_busc_i2c_exec;
		mess_li2cdriver_i2c_busc_i2c_reserve m_li2cdriver_i2c_busc_i2c_reserve;
		mess_linputdriver_input_event m_linputdriver_input_event;
		mess_lsys_fi_ctl	m_lsys_fi_ctl;
		mess_lsys_fi_reply	m_lsys_fi_reply;
		mess_lsys_getsysinfo	m_lsys_getsysinfo;
		mess_lsys_krn_readbios	m_lsys_krn_readbios;
		mess_lsys_kern_safecopy	m_lsys_kern_safecopy;
		mess_lsys_krn_schedctl	m_lsys_krn_schedctl;
		mess_lsys_krn_schedule	m_lsys_krn_schedule;
		mess_lsys_krn_sys_abort m_lsys_krn_sys_abort;
		mess_lsys_krn_sys_clear m_lsys_krn_sys_clear;
		mess_lsys_krn_sys_copy	m_lsys_krn_sys_copy;
		mess_lsys_krn_sys_cprof m_lsys_krn_sys_cprof;
		mess_lsys_krn_sys_devio m_lsys_krn_sys_devio;
		mess_lsys_krn_sys_diagctl m_lsys_krn_sys_diagctl;
		mess_lsys_krn_sys_exec	m_lsys_krn_sys_exec;
		mess_lsys_krn_sys_fork	m_lsys_krn_sys_fork;
		mess_lsys_krn_sys_getinfo m_lsys_krn_sys_getinfo;
		mess_lsys_krn_sys_getmcontext m_lsys_krn_sys_getmcontext;
		mess_lsys_krn_sys_iopenable m_lsys_krn_sys_iopenable;
		mess_lsys_krn_sys_irqctl m_lsys_krn_sys_irqctl;
		mess_lsys_krn_sys_memset m_lsys_krn_sys_memset;
		mess_lsys_krn_sys_privctl m_lsys_krn_sys_privctl;
		mess_lsys_krn_sys_profbuf m_lsys_krn_sys_profbuf;
		mess_lsys_krn_sys_sdevio m_lsys_krn_sys_sdevio;
		mess_lsys_krn_sys_setalarm m_lsys_krn_sys_setalarm;
		mess_lsys_krn_sys_setgrant m_lsys_krn_sys_setgrant;
		mess_lsys_krn_sys_setmcontext m_lsys_krn_sys_setmcontext;
		mess_lsys_krn_sys_settime m_lsys_krn_sys_settime;
		mess_lsys_krn_sys_sprof m_lsys_krn_sys_sprof;
		mess_lsys_krn_sys_statectl m_lsys_krn_sys_statectl;
		mess_lsys_krn_sys_stime	m_lsys_krn_sys_stime;
		mess_lsys_krn_sys_times	m_lsys_krn_sys_times;
		mess_lsys_krn_sys_trace	m_lsys_krn_sys_trace;
		mess_lsys_krn_sys_umap	m_lsys_krn_sys_umap;
		mess_lsys_krn_sys_vdevio m_lsys_krn_sys_vdevio;
		mess_lsys_krn_sys_vumap m_lsys_krn_sys_vumap;
		mess_lsys_kern_vsafecopy m_lsys_kern_vsafecopy;
		mess_lsys_pci_busc_get_bar m_lsys_pci_busc_get_bar;
		mess_lsys_pm_getepinfo	m_lsys_pm_getepinfo;
		mess_lsys_pm_getprocnr	m_lsys_pm_getprocnr;
		mess_lsys_pm_srv_fork	m_lsys_pm_srv_fork;
		mess_lsys_sched_scheduling_start m_lsys_sched_scheduling_start;
		mess_lsys_sched_scheduling_stop m_lsys_sched_scheduling_stop;
		mess_lsys_svrctl	m_lsys_svrctl;
		mess_lsys_tty_fkey_ctl	m_lsys_tty_fkey_ctl;
		mess_lsys_vfs_checkperms m_lsys_vfs_checkperms;
		mess_lsys_vfs_copyfd	m_lsys_vfs_copyfd;
		mess_lsys_vfs_mapdriver	m_lsys_vfs_mapdriver;
		mess_lsys_vm_getref	m_lsys_vm_getref;
		mess_lsys_vm_info	m_lsys_vm_info;
		mess_lsys_vm_map_phys	m_lsys_vm_map_phys;
		mess_lsys_vm_query_exit	m_lsys_vm_query_exit;
		mess_lsys_vm_unmap_phys	m_lsys_vm_unmap_phys;
		mess_lsys_vm_update	m_lsys_vm_update;
		mess_lsys_vm_vmremap	m_lsys_vm_vmremap;
		mess_lsys_vm_watch_exit	m_lsys_vm_watch_exit;
		mess_mmap		m_mmap;
		mess_net_netdrv_dl_conf m_net_netdrv_dl_conf;
		mess_net_netdrv_dl_getstat_s m_net_netdrv_dl_getstat_s;
		mess_net_netdrv_dl_readv_s m_net_netdrv_dl_readv_s;
		mess_net_netdrv_dl_writev_s m_net_netdrv_dl_writev_s;
		mess_netdrv_net_dl_conf m_netdrv_net_dl_conf;
		mess_netdrv_net_dl_task m_netdrv_net_dl_task;
		mess_notify		m_notify;
		mess_pci_lsys_busc_get_bar m_pci_lsys_busc_get_bar;
		mess_pm_lc_getgid	m_pm_lc_getgid;
		mess_pm_lc_getpid	m_pm_lc_getpid;
		mess_pm_lc_getuid	m_pm_lc_getuid;
		mess_pm_lc_ptrace	m_pm_lc_ptrace;
		mess_pm_lc_sigset	m_pm_lc_sigset;
		mess_pm_lc_time		m_pm_lc_time;
		mess_pm_lc_waitpid	m_pm_lc_waitpid;
		mess_pm_lexec_exec_new	m_pm_lexec_exec_new;
		mess_pm_lsys_getepinfo	m_pm_lsys_getepinfo;
		mess_pm_lsys_getprocnr	m_pm_lsys_getprocnr;
		mess_pm_lsys_sigs_signal m_pm_lsys_sigs_signal;
		mess_pm_sched_scheduling_set_nice m_pm_sched_scheduling_set_nice;
		mess_readclock_lc_rtcdev m_readclock_lc_rtcdev;
		mess_rs_init		m_rs_init;
		mess_rs_pm_exec_restart	m_rs_pm_exec_restart;
		mess_rs_pm_srv_kill	m_rs_pm_srv_kill;
		mess_rs_req		m_rs_req;
		mess_rs_update		m_rs_update;
		mess_sched_lsys_scheduling_start m_sched_lsys_scheduling_start;
		mess_sigcalls		m_sigcalls;
		mess_tty_lsys_fkey_ctl	m_tty_lsys_fkey_ctl;
		mess_vfs_fs_breadwrite	m_vfs_fs_breadwrite;
		mess_vfs_fs_chmod	m_vfs_fs_chmod;
		mess_vfs_fs_chown	m_vfs_fs_chown;
		mess_vfs_fs_create	m_vfs_fs_create;
		mess_vfs_fs_flush	m_vfs_fs_flush;
		mess_vfs_fs_ftrunc	m_vfs_fs_ftrunc;
		mess_vfs_fs_getdents	m_vfs_fs_getdents;
		mess_vfs_fs_inhibread	m_vfs_fs_inhibread;
		mess_vfs_fs_link	m_vfs_fs_link;
		mess_vfs_fs_lookup	m_vfs_fs_lookup;
		mess_vfs_fs_mkdir	m_vfs_fs_mkdir;
		mess_vfs_fs_mknod	m_vfs_fs_mknod;
		mess_vfs_fs_mountpoint	m_vfs_fs_mountpoint;
		mess_vfs_fs_new_driver	m_vfs_fs_new_driver;
		mess_vfs_fs_newnode	m_vfs_fs_newnode;
		mess_vfs_fs_putnode	m_vfs_fs_putnode;
		mess_vfs_fs_rdlink	m_vfs_fs_rdlink;
		mess_vfs_fs_readsuper	m_vfs_fs_readsuper;
		mess_vfs_fs_readwrite	m_vfs_fs_readwrite;
		mess_vfs_fs_rename	m_vfs_fs_rename;
		mess_vfs_fs_slink	m_vfs_fs_slink;
		mess_vfs_fs_stat	m_vfs_fs_stat;
		mess_vfs_fs_statvfs	m_vfs_fs_statvfs;
		mess_vfs_fs_unlink	m_vfs_fs_unlink;
		mess_vfs_fs_utime	m_vfs_fs_utime;
		mess_vfs_lc_lseek	m_vfs_lc_lseek;
		mess_vfs_lchardriver_cancel	m_vfs_lchardriver_cancel;
		mess_vfs_lchardriver_openclose	m_vfs_lchardriver_openclose;
		mess_vfs_lchardriver_readwrite	m_vfs_lchardriver_readwrite;
		mess_vfs_lchardriver_select	m_vfs_lchardriver_select;
		mess_vfs_utimens	m_vfs_utimens;
		mess_vm_vfs_mmap	m_vm_vfs_mmap;
		mess_vmmcp		m_vmmcp;
		mess_vmmcp_reply	m_vmmcp_reply;

		u8_t size[56];	/* message payload may have 56 bytes at most */
	};
} message __aligned(16);

/* Ensure the complete union respects the IPC assumptions. */
typedef int _ASSERT_message[/* CONSTCOND */sizeof(message) == 64 ? 1 : -1];

/* The following defines provide names for useful members. */
#define m1_i1  m_m1.m1i1
#define m1_i2  m_m1.m1i2
#define m1_i3  m_m1.m1i3
#define m1_p1  m_m1.m1p1
#define m1_p2  m_m1.m1p2
#define m1_p3  m_m1.m1p3
#define m1_p4  m_m1.m1p4
#define m1_ull1  m_m1.m1ull1

#define m2_ll1  m_m2.m2ll1
#define m2_i1  m_m2.m2i1
#define m2_i2  m_m2.m2i2
#define m2_i3  m_m2.m2i3
#define m2_l1  m_m2.m2l1
#define m2_l2  m_m2.m2l2
#define m2_p1  m_m2.m2p1
#define m2_sigset  m_m2.sigset

#define m2_s1  m_m2.m2s1

#define m3_i1  m_m3.m3i1
#define m3_i2  m_m3.m3i2
#define m3_p1  m_m3.m3p1
#define m3_ca1 m_m3.m3ca1

#define m4_ll1  m_m4.m4ll1
#define m4_l1  m_m4.m4l1
#define m4_l2  m_m4.m4l2
#define m4_l3  m_m4.m4l3
#define m4_l4  m_m4.m4l4
#define m4_l5  m_m4.m4l5

#define m7_i1  m_m7.m7i1
#define m7_i2  m_m7.m7i2
#define m7_i3  m_m7.m7i3
#define m7_i4  m_m7.m7i4
#define m7_i5  m_m7.m7i5
#define m7_p1  m_m7.m7p1
#define m7_p2  m_m7.m7p2

#define m9_l1  m_m9.m9l1
#define m9_l2  m_m9.m9l2
#define m9_l3  m_m9.m9l3
#define m9_l4  m_m9.m9l4
#define m9_l5  m_m9.m9l5
#define m9_s1  m_m9.m9s1
#define m9_s2  m_m9.m9s2
#define m9_s3  m_m9.m9s3
#define m9_s4  m_m9.m9s4
#define m9_ull1  m_m9.m9ull1
#define m9_ull2  m_m9.m9ull2

#define m10_i1 m_m10.m10i1
#define m10_i2 m_m10.m10i2
#define m10_i3 m_m10.m10i3
#define m10_i4 m_m10.m10i4
#define m10_l1 m_m10.m10l1
#define m10_l2 m_m10.m10l2
#define m10_l3 m_m10.m10l3
#define m10_ull1 m_m10.m10ull1


/*==========================================================================* 
 * Minix run-time system (IPC). 					    *
 *==========================================================================*/ 

/* Datastructure for asynchronous sends */
typedef struct asynmsg
{
	unsigned flags;
	endpoint_t dst;
	int result;
	message msg;
} asynmsg_t;

/* Defines for flags field */
#define AMF_EMPTY	000	/* slot is not inuse */
#define AMF_VALID	001	/* slot contains message */
#define AMF_DONE	002	/* Kernel has processed the message. The
				 * result is stored in 'result'
				 */
#define AMF_NOTIFY	004	/* Send a notification when AMF_DONE is set */
#define AMF_NOREPLY	010	/* Not a reply message for a SENDREC */
#define AMF_NOTIFY_ERR	020	/* Send a notification when AMF_DONE is set and
				 * delivery of the message failed */

int _ipc_send_intr(endpoint_t dest, message *m_ptr);
int _ipc_receive_intr(endpoint_t src, message *m_ptr, int *status_ptr);
int _ipc_sendrec_intr(endpoint_t src_dest, message *m_ptr);
int _ipc_sendnb_intr(endpoint_t dest, message *m_ptr);
int _ipc_notify_intr(endpoint_t dest);
int _ipc_senda_intr(asynmsg_t *table, size_t count);

int _do_kernel_call_intr(message *m_ptr);

int get_minix_kerninfo(struct minix_kerninfo **);

/* Hide names to avoid name space pollution. */
#define ipc_notify	_ipc_notify
#define ipc_sendrec	_ipc_sendrec
#define ipc_receive	_ipc_receive
#define ipc_receivenb	_ipc_receivenb
#define ipc_send	_ipc_send
#define ipc_sendnb	_ipc_sendnb
#define ipc_senda	_ipc_senda

#define do_kernel_call	_do_kernel_call

struct minix_ipcvecs {
	int (*send)(endpoint_t dest, message *m_ptr);
	int (*receive)(endpoint_t src, message *m_ptr, int *st);
	int (*sendrec)(endpoint_t src_dest, message *m_ptr);
	int (*sendnb)(endpoint_t dest, message *m_ptr);
	int (*notify)(endpoint_t dest);
	int (*do_kernel_call)(message *m_ptr);
	int (*senda)(asynmsg_t *table, size_t count);
};

/* kernel-set IPC vectors retrieved by a constructor in
 * minix/lib/libc/sys/init.c */
extern struct minix_ipcvecs _minix_ipcvecs;

static inline int _ipc_send(endpoint_t dest, message *m_ptr)
{
	return _minix_ipcvecs.send(dest, m_ptr);
}

static inline int _ipc_receive(endpoint_t src, message *m_ptr, int *st)
{
	return _minix_ipcvecs.receive(src, m_ptr, st);
}

static inline int _ipc_sendrec(endpoint_t src_dest, message *m_ptr)
{
	return _minix_ipcvecs.sendrec(src_dest, m_ptr);
}

static inline int _ipc_sendnb(endpoint_t dest, message *m_ptr)
{
	return _minix_ipcvecs.sendnb(dest, m_ptr);
}

static inline int _ipc_notify(endpoint_t dest)
{
	return _minix_ipcvecs.notify(dest);
}

static inline int _do_kernel_call(message *m_ptr)
{
	return _minix_ipcvecs.do_kernel_call(m_ptr);
}

static inline int _ipc_senda(asynmsg_t *table, size_t count)
{
	return _minix_ipcvecs.senda(table, count);
}

#endif /* _IPC_H */
