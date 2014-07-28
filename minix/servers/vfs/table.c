/* This file contains the table used to map system call numbers onto the
 * routines that perform them.
 */

#define _TABLE

#include "fs.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include "file.h"
#include "lock.h"
#include "scratchpad.h"
#include "vnode.h"
#include "vmnt.h"

#define CALL(n) [((n) - VFS_BASE)]

int (* const call_vec[NR_VFS_CALLS])(void) = {
	CALL(VFS_READ)		= do_read,		/* read(2) */
	CALL(VFS_WRITE)		= do_write,		/* write(2) */
	CALL(VFS_LSEEK)		= do_lseek,		/* lseek(2) */
	CALL(VFS_OPEN)		= do_open,		/* open(2) */
	CALL(VFS_CREAT)		= do_creat,		/* creat(2) */
	CALL(VFS_CLOSE)		= do_close,		/* close(2) */
	CALL(VFS_LINK)		= do_link,		/* link(2) */
	CALL(VFS_UNLINK)	= do_unlink,		/* unlink(2) */
	CALL(VFS_CHDIR)		= do_chdir,		/* chdir(2) */
	CALL(VFS_MKDIR)		= do_mkdir,		/* mkdir(2) */
	CALL(VFS_MKNOD)		= do_mknod,		/* mknod(2) */
	CALL(VFS_CHMOD)		= do_chmod,		/* chmod(2) */
	CALL(VFS_CHOWN)		= do_chown,		/* chown(2) */
	CALL(VFS_MOUNT)		= do_mount,		/* mount(2) */
	CALL(VFS_UMOUNT)	= do_umount,		/* umount(2) */
	CALL(VFS_ACCESS)	= do_access,		/* access(2) */
	CALL(VFS_SYNC)		= do_sync,		/* sync(2) */
	CALL(VFS_RENAME)	= do_rename,		/* rename(2) */
	CALL(VFS_RMDIR)		= do_unlink,		/* rmdir(2) */
	CALL(VFS_SYMLINK)	= do_slink,		/* symlink(2) */
	CALL(VFS_READLINK)	= do_rdlink,		/* readlink(2) */
	CALL(VFS_STAT)		= do_stat,		/* stat(2) */
	CALL(VFS_FSTAT)		= do_fstat,		/* fstat(2) */
	CALL(VFS_LSTAT)		= do_lstat,		/* lstat(2) */
	CALL(VFS_IOCTL)		= do_ioctl,		/* ioctl(2) */
	CALL(VFS_FCNTL)		= do_fcntl,		/* fcntl(2) */
	CALL(VFS_PIPE2)		= do_pipe2,		/* pipe2(2) */
	CALL(VFS_UMASK)		= do_umask,		/* umask(2) */
	CALL(VFS_CHROOT)	= do_chroot,		/* chroot(2) */
	CALL(VFS_GETDENTS)	= do_getdents,		/* getdents(2) */
	CALL(VFS_SELECT)	= do_select,		/* select(2) */
	CALL(VFS_FCHDIR)	= do_fchdir,		/* fchdir(2) */
	CALL(VFS_FSYNC)		= do_fsync,		/* fsync(2) */
	CALL(VFS_TRUNCATE)	= do_truncate,		/* truncate(2) */
	CALL(VFS_FTRUNCATE)	= do_ftruncate,		/* ftruncate(2) */
	CALL(VFS_FCHMOD)	= do_chmod,		/* fchmod(2) */
	CALL(VFS_FCHOWN)	= do_chown,		/* fchown(2) */
	CALL(VFS_UTIMENS)	= do_utimens,		/* [fl]utime[n]s(2) */
	CALL(VFS_VMCALL)	= do_vm_call,
	CALL(VFS_GETVFSSTAT)	= do_getvfsstat,	/* getvfsstat(2) */
	CALL(VFS_STATVFS1)	= do_statvfs,		/* statvfs(2) */
	CALL(VFS_FSTATVFS1)	= do_fstatvfs,		/* fstatvfs(2) */
	CALL(VFS_GETRUSAGE)	= do_getrusage,		/* getrusage(2) */
	CALL(VFS_SVRCTL)	= do_svrctl,		/* svrctl(2) */
	CALL(VFS_GCOV_FLUSH)	= do_gcov_flush,	/* gcov_flush(2) */
	CALL(VFS_MAPDRIVER)	= do_mapdriver,		/* mapdriver(2) */
	CALL(VFS_COPYFD)	= do_copyfd,		/* copyfd(2) */
	CALL(VFS_CHECKPERMS)	= do_checkperms,	/* checkperms(2) */
	CALL(VFS_GETSYSINFO)	= do_getsysinfo,	/* getsysinfo(2) */
};
