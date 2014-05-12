#ifndef _IPC_H
#define _IPC_H

#include <minix/ipcconst.h>
#include <minix/type.h>
#include <minix/const.h>
#include <sys/signal.h>

/*==========================================================================* 
 * Types relating to messages. 						    *
 *==========================================================================*/ 

#define M1                 1
#define M3                 3
#define M4                 4
#define M3_STRING         44	/* legacy m3_ca1 size (must not be changed) */
#define M3_LONG_STRING    44	/* current m3_ca1 size (may be increased) */

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
	char m3ca1[M3_LONG_STRING];
} mess_3;
_ASSERT_MSG_SIZE(mess_3);

typedef struct {
	int64_t m4ll1;
	long m4l1, m4l2, m4l3, m4l4, m4l5;
	uint8_t padding[28];
} mess_4;
_ASSERT_MSG_SIZE(mess_4);

typedef struct {
	short m5s1, m5s2;
	int m5i1, m5i2;
	long m5l1, m5l2, m5l3;
	uint8_t padding[32];
} mess_5;
_ASSERT_MSG_SIZE(mess_5);

typedef struct {
	int m7i1, m7i2, m7i3, m7i4, m7i5;
	char *m7p1, *m7p2;
	uint8_t padding[28];
} mess_7;
_ASSERT_MSG_SIZE(mess_7);

typedef struct {
	int m8i1, m8i2;
	char *m8p1, *m8p2, *m8p3, *m8p4;
	uint8_t padding[32];
} mess_8;
_ASSERT_MSG_SIZE(mess_8);

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

typedef struct {
	int m11i1;
	short m11s1, m11s2, m11s3, m11s4;
	char *m11p1, *m11p2, *m11p3, *m11p4;
	uint8_t padding[28];
} mess_11;
_ASSERT_MSG_SIZE(mess_11);

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
	u64_t timestamp;	/* valid for every notify msg */
	u64_t interrupts;	/* raised interrupts; valid if from HARDWARE */
	sigset_t sigset;	/* raised signals; valid if from SYSTEM */
	uint8_t padding[24];
} mess_notify;
_ASSERT_MSG_SIZE(mess_notify);

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
	int fd;

	uint8_t padding[52];
} mess_lc_vfs_fsync;
_ASSERT_MSG_SIZE(mess_lc_vfs_fsync);

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
	void *arg;

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
	off_t offset;

	uint8_t padding[48];
} mess_vfs_lc_lseek;
_ASSERT_MSG_SIZE(mess_vfs_lc_lseek);

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
	int fd0;
	int fd1;
	int flags;

	uint8_t padding[44];
} mess_lc_vfs_pipe2;
_ASSERT_MSG_SIZE(mess_lc_vfs_pipe2);

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
	dev_t device;
	off_t seek_pos;

	cp_grant_id_t grant;
	size_t nbytes;

	uint8_t data[32];
} mess_vfs_fs_breadwrite;
_ASSERT_MSG_SIZE(mess_vfs_fs_breadwrite);

typedef struct {
	off_t seek_pos;

	size_t nbytes;

	uint8_t data[44];
} mess_fs_vfs_breadwrite;
_ASSERT_MSG_SIZE(mess_fs_vfs_breadwrite);

typedef struct {
	ino_t inode;

	mode_t mode;

	uint8_t data[44];
} mess_vfs_fs_chmod;
_ASSERT_MSG_SIZE(mess_vfs_fs_chmod);

typedef struct {
	mode_t mode;

	uint8_t data[52];
} mess_fs_vfs_chmod;
_ASSERT_MSG_SIZE(mess_fs_vfs_chmod);

typedef struct {
	ino_t inode;

	uid_t uid;
	gid_t gid;

	uint8_t data[40];
} mess_vfs_fs_chown;
_ASSERT_MSG_SIZE(mess_vfs_fs_chown);

typedef struct {
	mode_t mode;

	uint8_t data[52];
} mess_fs_vfs_chown;
_ASSERT_MSG_SIZE(mess_fs_vfs_chown);

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
	off_t file_size;
	ino_t inode;

	mode_t mode;
	uid_t uid;
	gid_t gid;

	uint8_t data[28];
} mess_fs_vfs_create;
_ASSERT_MSG_SIZE(mess_fs_vfs_create);

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
	off_t seek_pos;

	size_t nbytes;

	uint8_t data[44];
} mess_fs_vfs_getdents;
_ASSERT_MSG_SIZE(mess_fs_vfs_getdents);

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
	size_t nbytes;

	uint8_t data[52];
} mess_fs_vfs_rdlink;
_ASSERT_MSG_SIZE(mess_fs_vfs_rdlink);

typedef struct {
	dev_t device;

	uint32_t flags;
	size_t path_len;
	cp_grant_id_t grant;

	uint8_t data[36];
} mess_vfs_fs_readsuper;
_ASSERT_MSG_SIZE(mess_vfs_fs_readsuper);

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
	ino_t inode;
	off_t seek_pos;

	cp_grant_id_t grant;
	size_t nbytes;

	uint8_t data[32];
} mess_vfs_fs_readwrite;
_ASSERT_MSG_SIZE(mess_vfs_fs_readwrite);

typedef struct {
	off_t seek_pos;

	size_t nbytes;

	uint8_t data[44];
} mess_fs_vfs_readwrite;
_ASSERT_MSG_SIZE(mess_fs_vfs_readwrite);

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
		mess_5			m_m5;
		mess_7			m_m7;
		mess_8			m_m8;
		mess_9			m_m9;
		mess_10			m_m10;
		mess_11			m_m11;
		mess_mmap		m_mmap;
		mess_notify		m_notify;
		mess_sigcalls		m_sigcalls;

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

		mess_lc_vfs_fsync	m_lc_vfs_fsync;
		mess_lc_vfs_getvfsstat	m_lc_vfs_getvfsstat;
		mess_lc_vfs_ioctl	m_lc_vfs_ioctl;
		mess_lc_vfs_link	m_lc_vfs_link;
		mess_lc_vfs_lseek	m_lc_vfs_lseek;
		mess_lc_vfs_mount	m_lc_vfs_mount;
		mess_lc_vfs_pipe2	m_lc_vfs_pipe2;
		mess_lc_vfs_select	m_lc_vfs_select;
		mess_lc_vfs_statvfs1	m_lc_vfs_statvfs1;
		mess_lc_vfs_truncate	m_lc_vfs_truncate;
		mess_lc_vfs_umask	m_lc_vfs_umask;
		mess_lc_vfs_umount	m_lc_vfs_umount;

		mess_lsys_vfs_checkperms m_lsys_vfs_checkperms;
		mess_lsys_vfs_copyfd	m_lsys_vfs_copyfd;
		mess_lsys_vfs_mapdriver	m_lsys_vfs_mapdriver;

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
		mess_vfs_fs_newnode	m_vfs_fs_newnode;
		mess_vfs_fs_new_driver	m_vfs_fs_new_driver;
		mess_vfs_fs_putnode	m_vfs_fs_putnode;
		mess_vfs_fs_rdlink	m_vfs_fs_rdlink;
		mess_vfs_fs_readsuper	m_vfs_fs_readsuper;
		mess_vfs_fs_rename	m_vfs_fs_rename;
		mess_vfs_fs_readwrite	m_vfs_fs_readwrite;
		mess_vfs_fs_slink	m_vfs_fs_slink;
		mess_vfs_fs_stat	m_vfs_fs_stat;
		mess_vfs_fs_statvfs	m_vfs_fs_statvfs;
		mess_vfs_fs_unlink	m_vfs_fs_unlink;
		mess_vfs_fs_utime	m_vfs_fs_utime;

		mess_vfs_lc_lseek	m_vfs_lc_lseek;

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

#define m5_s1  m_m5.m5s1
#define m5_s2  m_m5.m5s2
#define m5_i1  m_m5.m5i1
#define m5_i2  m_m5.m5i2
#define m5_l1  m_m5.m5l1
#define m5_l2  m_m5.m5l2
#define m5_l3  m_m5.m5l3

#define m7_i1  m_m7.m7i1
#define m7_i2  m_m7.m7i2
#define m7_i3  m_m7.m7i3
#define m7_i4  m_m7.m7i4
#define m7_i5  m_m7.m7i5
#define m7_p1  m_m7.m7p1
#define m7_p2  m_m7.m7p2

#define m8_i1  m_m8.m8i1
#define m8_i2  m_m8.m8i2
#define m8_p1  m_m8.m8p1
#define m8_p2  m_m8.m8p2
#define m8_p3  m_m8.m8p3
#define m8_p4  m_m8.m8p4

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

#define m11_i1 m_m11.m11i1
#define m11_s1 m_m11.m11s1
#define m11_s2 m_m11.m11s2
#define m11_s3 m_m11.m11s3
#define m11_s4 m_m11.m11s4
#define m11_p1 m_m11.m11p1
#define m11_p2 m_m11.m11p2
#define m11_p3 m_m11.m11p3
#define m11_p4 m_m11.m11p4

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

/* kernel-set IPC vectors retrieved by a constructor in libc/sys-minix/init.c */
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
