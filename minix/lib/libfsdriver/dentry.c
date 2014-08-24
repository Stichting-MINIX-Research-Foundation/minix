
#include "fsdriver.h"
#include <sys/dirent.h>

/*
 * Initialize a directory entry listing.
 */
void
fsdriver_dentry_init(struct fsdriver_dentry * __restrict dentry,
	const struct fsdriver_data * __restrict data, size_t bytes,
	char * __restrict buf, size_t bufsize)
{

	dentry->data = data;
	dentry->data_size = bytes;
	dentry->data_off = 0;
	dentry->buf = buf;
	dentry->buf_size = bufsize;
	dentry->buf_off = 0;
}

/*
 * Add an entry to a directory entry listing.  Return the entry size if it was
 * added, zero if no more entries could be added and the listing should stop,
 * or an error code in case of an error.
 */
ssize_t
fsdriver_dentry_add(struct fsdriver_dentry * __restrict dentry, ino_t ino_nr,
	const char * __restrict name, size_t namelen, unsigned int type)
{
	struct dirent *dirent;
	size_t len, used;
	int r;

	/* We could do several things here, but it should never happen.. */
	if (namelen > MAXNAMLEN)
		panic("fsdriver: directory entry name excessively long");

	len = _DIRENT_RECLEN(dirent, namelen);

	if (dentry->data_off + dentry->buf_off + len > dentry->data_size) {
		if (dentry->data_off == 0 && dentry->buf_off == 0)
			return EINVAL;

		return 0;
	}

	if (dentry->buf_off + len > dentry->buf_size) {
		if (dentry->buf_off == 0)
			panic("fsdriver: getdents buffer too small");

		if ((r = fsdriver_copyout(dentry->data, dentry->data_off,
		    dentry->buf, dentry->buf_off)) != OK)
			return r;

		dentry->data_off += dentry->buf_off;
		dentry->buf_off = 0;
	}

	dirent = (struct dirent *)&dentry->buf[dentry->buf_off];
	dirent->d_fileno = ino_nr;
	dirent->d_reclen = len;
	dirent->d_namlen = namelen;
	dirent->d_type = type;
	memcpy(dirent->d_name, name, namelen);

	/*
	 * Null-terminate the name, and zero out any alignment bytes after it,
	 * so as not to leak any data.
	 */
	used = _DIRENT_NAMEOFF(dirent) + namelen;
	if (used >= len)
		panic("fsdriver: inconsistency in dirent record");
	memset(&dirent->d_name[namelen], 0, len - used);

	dentry->buf_off += len;

	return len;
}

/*
 * Finish a directory entry listing operation.  Return the total number of
 * bytes copied to the caller, or an error code in case of an error.
 */
ssize_t
fsdriver_dentry_finish(struct fsdriver_dentry *dentry)
{
	int r;

	if (dentry->buf_off > 0) {
		if ((r = fsdriver_copyout(dentry->data, dentry->data_off,
		    dentry->buf, dentry->buf_off)) != OK)
			return r;

		dentry->data_off += dentry->buf_off;
	}

	return dentry->data_off;
}
