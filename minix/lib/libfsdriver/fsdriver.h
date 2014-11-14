#ifndef _LIBFSDRIVER_FSDRIVER_H
#define _LIBFSDRIVER_FSDRIVER_H

#include <minix/drivers.h>
#include <minix/fsdriver.h>
#include <minix/vfsif.h>

#define ROOT_UID	0	/* user ID of superuser */

extern int fsdriver_putnode(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_slink(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_trunc(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_chown(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_chmod(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_inhibread(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_stat(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_utime(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_statvfs(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_bread(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_bwrite(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_unlink(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_rmdir(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_unmount(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_sync(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_newdriver(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_flush(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_read(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_write(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_mknod(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_mkdir(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_create(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_link(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_rename(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_lookup(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_mountpoint(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_readsuper(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_newnode(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_rdlink(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_getdents(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_peek(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);
extern int fsdriver_bpeek(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);

extern int fsdriver_getname(endpoint_t endpt, cp_grant_id_t grant, size_t len,
	char *name, size_t size, int not_empty);

extern dev_t fsdriver_device;
extern ino_t fsdriver_root;
extern int fsdriver_mounted;
extern int (*fsdriver_callvec[])(const struct fsdriver * __restrict,
	const message * __restrict, message * __restrict);

#endif /* !_LIBFSDRIVER_FSDRIVER_H */
