#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#ifdef __weak_alias
__weak_alias(stat, _stat)
__weak_alias(fstat, _fstat)
__weak_alias(lstat, _lstat)
#ifdef __MINIX_EMULATE_NETBSD_STAT
__weak_alias(_stat, __emu_netbsd_stat)
__weak_alias(_fstat, __emu_netbsd_fstat)
__weak_alias(_lstat, __emu_netbsd_lstat)
#else
__weak_alias(_stat, __orig_minix_stat)
__weak_alias(_fstat, __orig_minix_fstat)
__weak_alias(_lstat, __orig_minix_lstat)
#endif
#else /* !__weak_alias */
#ifdef __MINIX_EMULATE_NETBSD_STAT
#define __emu_netbsd_stat stat
#define __emu_netbsd_fstat fstat
#define __emu_netbsd_lstat lstat
#else
#define __orig_minix_stat stat
#define __orig_minix_fstat fstat
#define __orig_minix_lstat lstat
#endif
#endif /* !__weak_alias */

#include <sys/stat.h>
#include <string.h>

int __orig_minix_stat(name, buffer)
const char *name;
struct __minix_stat *buffer;
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) name;
  m.m1_p2 = (char *) buffer;
  return(_syscall(VFS_PROC_NR, STAT, &m));
}

int __orig_minix_fstat(fd, buffer)
int fd;
struct __minix_stat *buffer;
{
	message m;

	m.m1_i1 = fd;
	m.m1_p1 = (char *) buffer;
	return(_syscall(VFS_PROC_NR, FSTAT, &m));
}

int __orig_minix_lstat(name, buffer)
const char *name;
struct __minix_stat *buffer;
{
  message m;
  int r;

  m.m1_i1 = strlen(name) + 1;
  m.m1_p1 = (char *) name;
  m.m1_p2 = (char *) buffer;
  if((r = _syscall(VFS_PROC_NR, LSTAT, &m)) >= 0 || errno != ENOSYS)
     return r;
  return __orig_minix_stat(name, buffer);
}

/*
 * NetBSD Fields Emulation.
 */

static void __emulate_netbsd_fields(struct __netbsd_stat *buffer)
{
	/* Emulated NetBSD fields. */
	buffer->st_atimespec.tv_sec = buffer->st_atime;
	buffer->st_atimespec.tv_nsec = 0;
	buffer->st_mtimespec.tv_sec = buffer->st_mtime;
	buffer->st_mtimespec.tv_nsec = 0;
	buffer->st_ctimespec.tv_sec = buffer->st_ctime;
	buffer->st_ctimespec.tv_nsec = 0;
	buffer->st_birthtimespec.tv_sec = 0;
	buffer->st_birthtimespec.tv_nsec = 0;
	buffer->st_blocks = (buffer->st_size / S_BLKSIZE) + 1;
	buffer->st_blksize = MINIX_ST_BLKSIZE;
	buffer->st_flags = 0;
	buffer->st_gen = 0;
}

const int __emu_netbsd_stat(name, buffer)
const char *name;
struct __netbsd_stat *buffer;
{
	int r;

	r = __orig_minix_stat(name, (struct __minix_stat *)buffer);
	if (r < 0)
		return r;
	__emulate_netbsd_fields(buffer);
	return r;
}

int __emu_netbsd_fstat(fd, buffer)
int fd;
struct __netbsd_stat *buffer;
{
	int r;
	r = __orig_minix_fstat(fd, (struct __minix_stat *)buffer);
	if ( r < 0 )
		return r;
	__emulate_netbsd_fields(buffer);
	return r;
}

int __emu_netbsd_lstat(name, buffer)
const char *name;
struct __netbsd_stat *buffer;
{
	int r;

	r = __orig_minix_lstat(name, (struct __minix_stat *)buffer);
	if ( r < 0 )
		return r;
	__emulate_netbsd_fields(buffer);
	return r;
}
