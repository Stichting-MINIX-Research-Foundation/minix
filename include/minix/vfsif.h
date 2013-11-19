#ifndef __MINIX_VFSIF_H
#define __MINIX_VFSIF_H

#include <sys/types.h>
#include <limits.h>

/* VFS/FS request fields */
#define REQ_ACTIME		m9_l2
#define REQ_ACNSEC		m9_l4
#define REQ_COUNT		m9_l2
#define REQ_DEV			m9_l5
#define REQ_DEV2		m9_l1
#define REQ_DIR_INO   		m9_l3
#define REQ_FLAGS		m9_s3
#define REQ_GID			m9_s1
#define REQ_GRANT		m9_l2
#define REQ_GRANT2		m9_l1 
#define REQ_GRANT3		m9_l3
#define REQ_INODE_NR		m9_l1
#define REQ_MEM_SIZE		m9_l5
#define REQ_MODE		m9_s3
#define REQ_MODTIME		m9_l3
#define REQ_MODNSEC		m9_l5
#define REQ_NBYTES		m9_l5
#define REQ_PATH_LEN		m9_s2
#define REQ_PATH_SIZE		m9_l5
#define REQ_REN_GRANT_NEW	m9_l1
#define REQ_REN_GRANT_OLD	m9_l2
#define REQ_REN_LEN_NEW		m9_s2
#define REQ_REN_LEN_OLD		m9_s1
#define REQ_REN_NEW_DIR		m9_l4
#define REQ_REN_OLD_DIR		m9_l3
#define REQ_ROOT_INO		m9_l4
#define REQ_SEEK_POS_HI		m9_l3
#define REQ_SEEK_POS_LO		m9_l4
#define REQ_TRC_END_HI		m9_l4
#define REQ_TRC_END_LO		m9_l5
#define REQ_TRC_START_HI	m9_l2
#define REQ_TRC_START_LO	m9_l3
#define REQ_UCRED_SIZE		m9_s4 
#define REQ_UID			m9_s4

/* VFS/FS reply fields */
#define RES_DEV			m9_l4
#define RES_GID			m9_s1
#define RES_INODE_NR		m9_l1
#define RES_FILE_SIZE_HI	m9_l2
#define RES_FILE_SIZE_LO	m9_l3
#define RES_MODE		m9_s2
#define RES_NBYTES		m9_l5
#define RES_OFFSET		m9_s2
#define RES_SEEK_POS_HI		m9_l3
#define RES_SEEK_POS_LO		m9_l4
#define RES_SYMLOOP		m9_s3
#define RES_UID			m9_s4
#define	RES_FLAGS		m9_s3

/* VFS/FS flags */
#define REQ_RDONLY		001	/* FS is mounted read-only */
#define REQ_ISROOT		002	/* FS is root file system */

#define PATH_NOFLAGS		000
#define PATH_RET_SYMLINK	010	/* Return a symlink object (i.e.
					 * do not continue with the contents
					 * of the symlink if it is the last
					 * component in a path). */
#define PATH_GET_UCRED		020	/* Request provides a grant ID in m9_l1
					 * and struct ucred size in m9_s4 (as
					 * opposed to a REQ_UID). */

#define RES_NOFLAGS		000
#define RES_THREADED		001	/* FS supports multithreading */
#define RES_HASPEEK		002	/* FS implements REQ_PEEK/REQ_BPEEK */
#define RES_64BIT		004	/* FS can handle 64-bit file sizes */

/* VFS/FS error messages */
#define EENTERMOUNT              (-301)
#define ELEAVEMOUNT              (-302)
#define ESYMLINK                 (-303)

/* VFS/FS types */

/* User credential structure */
typedef struct {
	uid_t vu_uid;
	gid_t vu_gid;
	int vu_ngroups;
	gid_t vu_sgroups[NGROUPS_MAX];
} vfs_ucred_t;

/* Some system types are larger than what the protocol and FSes use */
typedef u16_t	puid_t;		/* Protocol version of uid_t */
typedef u16_t	pgid_t;		/* Protocol version of gid_t */
typedef u16_t	pmode_t;	/* Protocol version of mode_t */
typedef u32_t	pino_t;		/* Protocol version of ino_t */

/* Request numbers */
#define REQ_GETNODE	(FS_BASE + 1)	/* Should be removed */
#define REQ_PUTNODE	(FS_BASE + 2)
#define REQ_SLINK	(FS_BASE + 3)
#define REQ_FTRUNC	(FS_BASE + 4)
#define REQ_CHOWN	(FS_BASE + 5)
#define REQ_CHMOD	(FS_BASE + 6)
#define REQ_INHIBREAD	(FS_BASE + 7)
#define REQ_STAT	(FS_BASE + 8)
#define REQ_UTIME	(FS_BASE + 9)
#define REQ_STATVFS	(FS_BASE + 10)
#define REQ_BREAD	(FS_BASE + 11)
#define REQ_BWRITE	(FS_BASE + 12)
#define REQ_UNLINK	(FS_BASE + 13)
#define REQ_RMDIR	(FS_BASE + 14)
#define REQ_UNMOUNT	(FS_BASE + 15)
#define REQ_SYNC	(FS_BASE + 16)
#define REQ_NEW_DRIVER	(FS_BASE + 17)
#define REQ_FLUSH	(FS_BASE + 18)
#define REQ_READ	(FS_BASE + 19)
#define REQ_WRITE	(FS_BASE + 20)
#define REQ_MKNOD	(FS_BASE + 21)
#define REQ_MKDIR	(FS_BASE + 22)
#define REQ_CREATE	(FS_BASE + 23)
#define REQ_LINK	(FS_BASE + 24)
#define REQ_RENAME	(FS_BASE + 25)
#define REQ_LOOKUP	(FS_BASE + 26)
#define REQ_MOUNTPOINT	(FS_BASE + 27)
#define REQ_READSUPER	(FS_BASE + 28)
#define REQ_NEWNODE	(FS_BASE + 29)
#define REQ_RDLINK	(FS_BASE + 30)
#define REQ_GETDENTS	(FS_BASE + 31)
#define REQ_PEEK	(FS_BASE + 32)
#define REQ_BPEEK	(FS_BASE + 33)

#define NREQS			    34

#define IS_FS_RQ(type) (((type) & ~0xff) == FS_BASE)

#define TRNS_GET_ID(t)		((t) & 0xFFFF)
#define TRNS_ADD_ID(t,id)	(((t) << 16) | ((id) & 0xFFFF))
#define TRNS_DEL_ID(t)		((short)((t) >> 16))

#endif

