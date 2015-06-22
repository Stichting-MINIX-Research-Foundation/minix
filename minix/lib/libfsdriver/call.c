
#include "fsdriver.h"
#include <minix/ds.h>
#include <sys/mman.h>

static int fsdriver_vmcache;	/* have we used the VM cache? */

/*
 * Process a READSUPER request from VFS.
 */
int
fsdriver_readsuper(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{
	struct fsdriver_node root_node;
	char label[DS_MAX_KEYLEN];
	cp_grant_id_t label_grant;
	size_t label_len;
	unsigned int flags, res_flags;
	dev_t dev;
	int r;

	dev = m_in->m_vfs_fs_readsuper.device;
	label_grant = m_in->m_vfs_fs_readsuper.grant;
	label_len = m_in->m_vfs_fs_readsuper.path_len;
	flags = m_in->m_vfs_fs_readsuper.flags;

	if (fdp->fdr_mount == NULL)
		return ENOSYS;

	if (fsdriver_mounted) {
		printf("fsdriver: attempt to mount multiple times\n");
		return EBUSY;
	}

	if ((r = fsdriver_getname(m_in->m_source, label_grant, label_len,
	    label, sizeof(label), FALSE /*not_empty*/)) != OK)
		return r;

	if (fdp->fdr_driver != NULL)
		fdp->fdr_driver(dev, label);

	res_flags = RES_NOFLAGS;

	r = fdp->fdr_mount(dev, flags, &root_node, &res_flags);

	if (r == OK) {
		/* This one we can set on the file system's behalf. */
		if ((fdp->fdr_peek != NULL && fdp->fdr_bpeek != NULL) ||
		    major(dev) == NONE_MAJOR)
			res_flags |= RES_HASPEEK;

		m_out->m_fs_vfs_readsuper.inode = root_node.fn_ino_nr;
		m_out->m_fs_vfs_readsuper.mode = root_node.fn_mode;
		m_out->m_fs_vfs_readsuper.file_size = root_node.fn_size;
		m_out->m_fs_vfs_readsuper.uid = root_node.fn_uid;
		m_out->m_fs_vfs_readsuper.gid = root_node.fn_gid;
		m_out->m_fs_vfs_readsuper.flags = res_flags;

		/* Update library-local state. */
		fsdriver_mounted = TRUE;
		fsdriver_device = dev;
		fsdriver_root = root_node.fn_ino_nr;
		fsdriver_vmcache = FALSE;
	}

	return r;
}

/*
 * Process an UNMOUNT request from VFS.
 */
int
fsdriver_unmount(const struct fsdriver * __restrict fdp,
	const message * __restrict __unused m_in,
	message * __restrict __unused m_out)
{

	if (fdp->fdr_unmount != NULL)
		fdp->fdr_unmount();

	/* If we used mmap emulation, clear any cached blocks from VM. */
	if (fsdriver_vmcache)
		vm_clear_cache(fsdriver_device);

	/* Update library-local state. */
	fsdriver_mounted = FALSE;

	return OK;
}

/*
 * Process a PUTNODE request from VFS.
 */
int
fsdriver_putnode(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	ino_t ino_nr;
	unsigned int count;

	ino_nr = m_in->m_vfs_fs_putnode.inode;
	count = m_in->m_vfs_fs_putnode.count;

	if (count == 0 || count > INT_MAX) {
		printf("fsdriver: invalid reference count\n");
		return EINVAL;
	}

	if (fdp->fdr_putnode != NULL)
		return fdp->fdr_putnode(ino_nr, count);
	else
		return OK;
}

/*
 * Process a NEWNODE request from VFS.
 */
int
fsdriver_newnode(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{
	struct fsdriver_node node;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	dev_t dev;
	int r;

	mode = m_in->m_vfs_fs_newnode.mode;
	uid = m_in->m_vfs_fs_newnode.uid;
	gid = m_in->m_vfs_fs_newnode.gid;
	dev = m_in->m_vfs_fs_newnode.device;

	if (fdp->fdr_newnode == NULL)
		return ENOSYS;

	if ((r = fdp->fdr_newnode(mode, uid, gid, dev, &node)) == OK) {
		m_out->m_fs_vfs_newnode.inode = node.fn_ino_nr;
		m_out->m_fs_vfs_newnode.mode = node.fn_mode;
		m_out->m_fs_vfs_newnode.file_size = node.fn_size;
		m_out->m_fs_vfs_newnode.uid = node.fn_uid;
		m_out->m_fs_vfs_newnode.gid = node.fn_gid;
		m_out->m_fs_vfs_newnode.device = node.fn_dev;
	}

	return r;
}

/*
 * Process a read or write request from VFS.
 */
static int
read_write(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out, int call)
{
	struct fsdriver_data data;
	ino_t ino_nr;
	off_t pos;
	size_t nbytes;
	ssize_t r;

	ino_nr = m_in->m_vfs_fs_readwrite.inode;
	pos = m_in->m_vfs_fs_readwrite.seek_pos;
	nbytes = m_in->m_vfs_fs_readwrite.nbytes;

	if (pos < 0 || nbytes > SSIZE_MAX)
		return EINVAL;

	data.endpt = m_in->m_source;
	data.grant = m_in->m_vfs_fs_readwrite.grant;
	data.size = nbytes;

	if (call == FSC_WRITE)
		r = fdp->fdr_write(ino_nr, &data, nbytes, pos, call);
	else
		r = fdp->fdr_read(ino_nr, &data, nbytes, pos, call);

	if (r >= 0) {
		pos += r;

		m_out->m_fs_vfs_readwrite.seek_pos = pos;
		m_out->m_fs_vfs_readwrite.nbytes = r;
		r = OK;
	}

	return r;
}

/*
 * Process a READ request from VFS.
 */
int
fsdriver_read(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{

	if (fdp->fdr_read == NULL)
		return ENOSYS;

	return read_write(fdp, m_in, m_out, FSC_READ);
}

/*
 * Process a WRITE request from VFS.
 */
int
fsdriver_write(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{

	if (fdp->fdr_write == NULL)
		return ENOSYS;

	return read_write(fdp, m_in, m_out, FSC_WRITE);
}

/*
 * A read-based peek implementation.  This allows file systems that do not have
 * a buffer cache and do not implement peek, to support a limited form of mmap.
 * We map in a block, fill it by calling the file system's read function, tell
 * VM about the page, and then unmap the block again.  We tell VM not to cache
 * the block beyond its immediate use for the mmap request, so as to prevent
 * potentially stale data from being cached--at the cost of performance.
 */
static ssize_t
builtin_peek(const struct fsdriver * __restrict fdp, ino_t ino_nr,
	size_t nbytes, off_t pos)
{
	static u32_t flags = 0;	/* storage for the VMMC_ flags of all blocks */
	static off_t dev_off = 0; /* fake device offset, see below */
	struct fsdriver_data data;
	char *buf;
	ssize_t r;

	if ((buf = mmap(NULL, nbytes, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE, -1, 0)) == MAP_FAILED)
		return ENOMEM;

	data.endpt = SELF;
	data.grant = (cp_grant_id_t)buf;
	data.size = nbytes;

	r = fdp->fdr_read(ino_nr, &data, nbytes, pos, FSC_READ);

	if (r >= 0) {
		if ((size_t)r < nbytes)
			memset(&buf[r], 0, nbytes - r);

		/*
		 * VM uses serialized communication to VFS.  Since the page is
		 * to be used only once, VM will use and then discard it before
		 * sending a new peek request.  Thus, it should be safe to
		 * reuse the same device offset all the time.  However, relying
		 * on assumptions in protocols elsewhere a bit dangerous, so we
		 * use an ever-increasing device offset just to be safe.
		 */
		r = vm_set_cacheblock(buf, fsdriver_device, dev_off, ino_nr,
		    pos, &flags, nbytes, VMSF_ONCE);

		if (r == OK) {
			fsdriver_vmcache = TRUE;

			dev_off += nbytes;

			r = nbytes;
		}
	}

	munmap(buf, nbytes);

	return r;
}

/*
 * Process a PEEK request from VFS.
 */
int
fsdriver_peek(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	ino_t ino_nr;
	off_t pos;
	size_t nbytes;
	ssize_t r;

	ino_nr = m_in->m_vfs_fs_readwrite.inode;
	pos = m_in->m_vfs_fs_readwrite.seek_pos;
	nbytes = m_in->m_vfs_fs_readwrite.nbytes;

	if (pos < 0 || nbytes > SSIZE_MAX)
		return EINVAL;

	if (fdp->fdr_peek == NULL) {
		if (major(fsdriver_device) != NONE_MAJOR)
			return ENOSYS;

		/*
		 * For file systems that have no backing device, emulate peek
		 * support by reading into temporary buffers and passing these
		 * to VM.
		 */
		r = builtin_peek(fdp, ino_nr, nbytes, pos);
	} else
		r = fdp->fdr_peek(ino_nr, NULL /*data*/, nbytes, pos,
		    FSC_PEEK);

	/* Do not return a new position. */
	if (r >= 0) {
		m_out->m_fs_vfs_readwrite.nbytes = r;
		r = OK;
	}

	return r;
}

/*
 * Process a GETDENTS request from VFS.
 */
int
fsdriver_getdents(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{
	struct fsdriver_data data;
	ino_t ino_nr;
	off_t pos;
	size_t nbytes;
	ssize_t r;

	ino_nr = m_in->m_vfs_fs_getdents.inode;
	pos = m_in->m_vfs_fs_getdents.seek_pos;
	nbytes = m_in->m_vfs_fs_getdents.mem_size;

	if (fdp->fdr_getdents == NULL)
		return ENOSYS;

	if (pos < 0 || nbytes > SSIZE_MAX)
		return EINVAL;

	data.endpt = m_in->m_source;
	data.grant = m_in->m_vfs_fs_getdents.grant;
	data.size = nbytes;

	r = fdp->fdr_getdents(ino_nr, &data, nbytes, &pos);

	if (r >= 0) {
		m_out->m_fs_vfs_getdents.seek_pos = pos;
		m_out->m_fs_vfs_getdents.nbytes = r;
		r = OK;
	}

	return r;
}

/*
 * Process a FTRUNC request from VFS.
 */
int
fsdriver_trunc(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	ino_t ino_nr;
	off_t start_pos, end_pos;

	ino_nr = m_in->m_vfs_fs_ftrunc.inode;
	start_pos = m_in->m_vfs_fs_ftrunc.trc_start;
	end_pos = m_in->m_vfs_fs_ftrunc.trc_end;

	if (start_pos < 0 || end_pos < 0)
		return EINVAL;

	if (fdp->fdr_trunc == NULL)
		return ENOSYS;

	return fdp->fdr_trunc(ino_nr, start_pos, end_pos);
}

/*
 * Process a INHIBREAD request from VFS.
 */
int
fsdriver_inhibread(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	ino_t ino_nr;

	ino_nr = m_in->m_vfs_fs_inhibread.inode;

	if (fdp->fdr_seek != NULL)
		fdp->fdr_seek(ino_nr);

	return OK;
}

/*
 * Process a CREATE request from VFS.
 */
int
fsdriver_create(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{
	struct fsdriver_node node;
	char name[NAME_MAX+1];
	cp_grant_id_t grant;
	size_t len;
	ino_t dir_nr;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	int r;

	grant = m_in->m_vfs_fs_create.grant;
	len = m_in->m_vfs_fs_create.path_len;
	dir_nr = m_in->m_vfs_fs_create.inode;
	mode = m_in->m_vfs_fs_create.mode;
	uid = m_in->m_vfs_fs_create.uid;
	gid = m_in->m_vfs_fs_create.gid;

	if (fdp->fdr_create == NULL)
		return ENOSYS;

	if ((r = fsdriver_getname(m_in->m_source, grant, len, name,
	    sizeof(name), TRUE /*not_empty*/)) != OK)
		return r;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return EEXIST;

	if ((r = fdp->fdr_create(dir_nr, name, mode, uid, gid, &node)) == OK) {
		m_out->m_fs_vfs_create.inode = node.fn_ino_nr;
		m_out->m_fs_vfs_create.mode = node.fn_mode;
		m_out->m_fs_vfs_create.file_size = node.fn_size;
		m_out->m_fs_vfs_create.uid = node.fn_uid;
		m_out->m_fs_vfs_create.gid = node.fn_gid;
	}

	return r;
}

/*
 * Process a MKDIR request from VFS.
 */
int
fsdriver_mkdir(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	char name[NAME_MAX+1];
	cp_grant_id_t grant;
	size_t path_len;
	ino_t dir_nr;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	int r;

	grant = m_in->m_vfs_fs_mkdir.grant;
	path_len = m_in->m_vfs_fs_mkdir.path_len;
	dir_nr = m_in->m_vfs_fs_mkdir.inode;
	mode = m_in->m_vfs_fs_mkdir.mode;
	uid = m_in->m_vfs_fs_mkdir.uid;
	gid = m_in->m_vfs_fs_mkdir.gid;

	if (fdp->fdr_mkdir == NULL)
		return ENOSYS;

	if ((r = fsdriver_getname(m_in->m_source, grant, path_len, name,
	    sizeof(name), TRUE /*not_empty*/)) != OK)
		return r;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return EEXIST;

	return fdp->fdr_mkdir(dir_nr, name, mode, uid, gid);
}

/*
 * Process a MKNOD request from VFS.
 */
int
fsdriver_mknod(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	char name[NAME_MAX+1];
	cp_grant_id_t grant;
	size_t path_len;
	ino_t dir_nr;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	dev_t dev;
	int r;

	grant = m_in->m_vfs_fs_mknod.grant;
	path_len = m_in->m_vfs_fs_mknod.path_len;
	dir_nr = m_in->m_vfs_fs_mknod.inode;
	mode = m_in->m_vfs_fs_mknod.mode;
	uid = m_in->m_vfs_fs_mknod.uid;
	gid = m_in->m_vfs_fs_mknod.gid;
	dev = m_in->m_vfs_fs_mknod.device;

	if (fdp->fdr_mknod == NULL)
		return ENOSYS;

	if ((r = fsdriver_getname(m_in->m_source, grant, path_len, name,
	    sizeof(name), TRUE /*not_empty*/)) != OK)
		return r;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return EEXIST;

	return fdp->fdr_mknod(dir_nr, name, mode, uid, gid, dev);
}

/*
 * Process a LINK request from VFS.
 */
int
fsdriver_link(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	char name[NAME_MAX+1];
	cp_grant_id_t grant;
	size_t path_len;
	ino_t dir_nr, ino_nr;
	int r;

	grant = m_in->m_vfs_fs_link.grant;
	path_len = m_in->m_vfs_fs_link.path_len;
	dir_nr = m_in->m_vfs_fs_link.dir_ino;
	ino_nr = m_in->m_vfs_fs_link.inode;

	if (fdp->fdr_link == NULL)
		return ENOSYS;

	if ((r = fsdriver_getname(m_in->m_source, grant, path_len, name,
	    sizeof(name), TRUE /*not_empty*/)) != OK)
		return r;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return EEXIST;

	return fdp->fdr_link(dir_nr, name, ino_nr);
}

/*
 * Process an UNLINK request from VFS.
 */
int
fsdriver_unlink(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	char name[NAME_MAX+1];
	cp_grant_id_t grant;
	size_t path_len;
	ino_t dir_nr;
	int r;

	grant = m_in->m_vfs_fs_unlink.grant;
	path_len = m_in->m_vfs_fs_unlink.path_len;
	dir_nr = m_in->m_vfs_fs_unlink.inode;

	if (fdp->fdr_unlink == NULL)
		return ENOSYS;

	if ((r = fsdriver_getname(m_in->m_source, grant, path_len, name,
	    sizeof(name), TRUE /*not_empty*/)) != OK)
		return r;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return EPERM;

	return fdp->fdr_unlink(dir_nr, name, FSC_UNLINK);
}

/*
 * Process a RMDIR request from VFS.
 */
int
fsdriver_rmdir(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	char name[NAME_MAX+1];
	cp_grant_id_t grant;
	size_t path_len;
	ino_t dir_nr;
	int r;

	grant = m_in->m_vfs_fs_unlink.grant;
	path_len = m_in->m_vfs_fs_unlink.path_len;
	dir_nr = m_in->m_vfs_fs_unlink.inode;

	if (fdp->fdr_rmdir == NULL)
		return ENOSYS;

	if ((r = fsdriver_getname(m_in->m_source, grant, path_len, name,
	    sizeof(name), TRUE /*not_empty*/)) != OK)
		return r;

	if (!strcmp(name, "."))
		return EINVAL;

	if (!strcmp(name, ".."))
		return ENOTEMPTY;

	return fdp->fdr_rmdir(dir_nr, name, FSC_RMDIR);
}

/*
 * Process a RENAME request from VFS.
 */
int
fsdriver_rename(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	char old_name[NAME_MAX+1], new_name[NAME_MAX+1];
	cp_grant_id_t old_grant, new_grant;
	size_t old_len, new_len;
	ino_t old_dir_nr, new_dir_nr;
	int r;

	old_grant = m_in->m_vfs_fs_rename.grant_old;
	old_len = m_in->m_vfs_fs_rename.len_old;
	old_dir_nr = m_in->m_vfs_fs_rename.dir_old;
	new_grant = m_in->m_vfs_fs_rename.grant_new;
	new_len = m_in->m_vfs_fs_rename.len_new;
	new_dir_nr = m_in->m_vfs_fs_rename.dir_new;

	if (fdp->fdr_rename == NULL)
		return ENOSYS;

	if ((r = fsdriver_getname(m_in->m_source, old_grant, old_len, old_name,
	    sizeof(old_name), TRUE /*not_empty*/)) != OK)
		return r;

	if (!strcmp(old_name, ".") || !strcmp(old_name, ".."))
		return EINVAL;

	if ((r = fsdriver_getname(m_in->m_source, new_grant, new_len, new_name,
	    sizeof(new_name), TRUE /*not_empty*/)) != OK)
		return r;

	if (!strcmp(new_name, ".") || !strcmp(new_name, ".."))
		return EINVAL;

	return fdp->fdr_rename(old_dir_nr, old_name, new_dir_nr, new_name);
}

/*
 * Process a SLINK request from VFS.
 */
int
fsdriver_slink(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	struct fsdriver_data data;
	char name[NAME_MAX+1];
	cp_grant_id_t grant;
	size_t path_len;
	ino_t dir_nr;
	uid_t uid;
	gid_t gid;
	int r;

	grant = m_in->m_vfs_fs_slink.grant_path;
	path_len = m_in->m_vfs_fs_slink.path_len;
	dir_nr = m_in->m_vfs_fs_slink.inode;
	uid = m_in->m_vfs_fs_slink.uid;
	gid = m_in->m_vfs_fs_slink.gid;

	if (fdp->fdr_slink == NULL)
		return ENOSYS;

	if ((r = fsdriver_getname(m_in->m_source, grant, path_len, name,
	    sizeof(name), TRUE /*not_empty*/)) != OK)
		return r;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return EEXIST;

	data.endpt = m_in->m_source;
	data.grant = m_in->m_vfs_fs_slink.grant_target;
	data.size = m_in->m_vfs_fs_slink.mem_size;

	return fdp->fdr_slink(dir_nr, name, uid, gid, &data, data.size);
}

/*
 * Process a RDLINK request from VFS.
 */
int
fsdriver_rdlink(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{
	struct fsdriver_data data;
	ssize_t r;

	if (fdp->fdr_rdlink == NULL)
		return ENOSYS;

	data.endpt = m_in->m_source;
	data.grant = m_in->m_vfs_fs_rdlink.grant;
	data.size = m_in->m_vfs_fs_rdlink.mem_size;

	r = fdp->fdr_rdlink(m_in->m_vfs_fs_rdlink.inode, &data, data.size);

	if (r >= 0) {
		m_out->m_fs_vfs_rdlink.nbytes = r;
		r = OK;
	}

	return r;
}

/*
 * Process a STAT request from VFS.
 */
int
fsdriver_stat(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	struct stat buf;
	cp_grant_id_t grant;
	ino_t ino_nr;
	int r;

	ino_nr = m_in->m_vfs_fs_stat.inode;
	grant = m_in->m_vfs_fs_stat.grant;

	if (fdp->fdr_stat == NULL)
		return ENOSYS;

	memset(&buf, 0, sizeof(buf));
	buf.st_dev = fsdriver_device;
	buf.st_ino = ino_nr;

	if ((r = fdp->fdr_stat(ino_nr, &buf)) == OK)
		r = sys_safecopyto(m_in->m_source, grant, 0, (vir_bytes)&buf,
		    (phys_bytes)sizeof(buf));

	return r;
}

/*
 * Process a CHOWN request from VFS.
 */
int
fsdriver_chown(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{
	ino_t ino_nr;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	int r;

	ino_nr = m_in->m_vfs_fs_chown.inode;
	uid = m_in->m_vfs_fs_chown.uid;
	gid = m_in->m_vfs_fs_chown.gid;

	if (fdp->fdr_chown == NULL)
		return ENOSYS;

	if ((r = fdp->fdr_chown(ino_nr, uid, gid, &mode)) == OK)
		m_out->m_fs_vfs_chown.mode = mode;

	return r;
}

/*
 * Process a CHMOD request from VFS.
 */
int
fsdriver_chmod(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{
	ino_t ino_nr;
	mode_t mode;
	int r;

	ino_nr = m_in->m_vfs_fs_chmod.inode;
	mode = m_in->m_vfs_fs_chmod.mode;

	if (fdp->fdr_chmod == NULL)
		return ENOSYS;

	if ((r = fdp->fdr_chmod(ino_nr, &mode)) == OK)
		m_out->m_fs_vfs_chmod.mode = mode;

	return r;
}

/*
 * Process a UTIME request from VFS.
 */
int
fsdriver_utime(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	ino_t ino_nr;
	struct timespec atime, mtime;

	ino_nr = m_in->m_vfs_fs_utime.inode;
	atime.tv_sec = m_in->m_vfs_fs_utime.actime;
	atime.tv_nsec = m_in->m_vfs_fs_utime.acnsec;
	mtime.tv_sec = m_in->m_vfs_fs_utime.modtime;
	mtime.tv_nsec = m_in->m_vfs_fs_utime.modnsec;

	if (fdp->fdr_utime == NULL)
		return ENOSYS;

	return fdp->fdr_utime(ino_nr, &atime, &mtime);
}

/*
 * Process a MOUNTPOINT request from VFS.
 */
int
fsdriver_mountpoint(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	ino_t ino_nr;

	ino_nr = m_in->m_vfs_fs_mountpoint.inode;

	if (fdp->fdr_mountpt == NULL)
		return ENOSYS;

	return fdp->fdr_mountpt(ino_nr);
}

/*
 * Process a STATVFS request from VFS.
 */
int
fsdriver_statvfs(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	struct statvfs buf;
	int r;

	if (fdp->fdr_statvfs == NULL)
		return ENOSYS;

	memset(&buf, 0, sizeof(buf));

	if ((r = fdp->fdr_statvfs(&buf)) != OK)
		return r;

	return sys_safecopyto(m_in->m_source, m_in->m_vfs_fs_statvfs.grant, 0,
	    (vir_bytes)&buf, (phys_bytes)sizeof(buf));
}

/*
 * Process a SYNC request from VFS.
 */
int
fsdriver_sync(const struct fsdriver * __restrict fdp,
	const message * __restrict __unused m_in,
	message * __restrict __unused m_out)
{

	if (fdp->fdr_sync != NULL)
		fdp->fdr_sync();

	return OK;
}

/*
 * Process a NEW_DRIVER request from VFS.
 */
int
fsdriver_newdriver(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	char label[DS_MAX_KEYLEN];
	cp_grant_id_t grant;
	size_t path_len;
	dev_t dev;
	int r;

	dev = m_in->m_vfs_fs_new_driver.device;
	grant = m_in->m_vfs_fs_new_driver.grant;
	path_len = m_in->m_vfs_fs_new_driver.path_len;

	if (fdp->fdr_driver == NULL)
		return OK;

	if ((r = fsdriver_getname(m_in->m_source, grant, path_len, label,
	    sizeof(label), FALSE /*not_empty*/)) != OK)
		return r;

	fdp->fdr_driver(dev, label);

	return OK;
}

/*
 * Process a block read or write request from VFS.
 */
static ssize_t
bread_bwrite(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out, int call)
{
	struct fsdriver_data data;
	dev_t dev;
	off_t pos;
	size_t nbytes;
	ssize_t r;

	dev = m_in->m_vfs_fs_breadwrite.device;
	pos = m_in->m_vfs_fs_breadwrite.seek_pos;
	nbytes = m_in->m_vfs_fs_breadwrite.nbytes;

	if (pos < 0 || nbytes > SSIZE_MAX)
		return EINVAL;

	data.endpt = m_in->m_source;
	data.grant = m_in->m_vfs_fs_breadwrite.grant;
	data.size = nbytes;

	if (call == FSC_WRITE)
		r = fdp->fdr_bwrite(dev, &data, nbytes, pos, call);
	else
		r = fdp->fdr_bread(dev, &data, nbytes, pos, call);

	if (r >= 0) {
		pos += r;

		m_out->m_fs_vfs_breadwrite.seek_pos = pos;
		m_out->m_fs_vfs_breadwrite.nbytes = r;
		r = OK;
	}

	return r;
}

/*
 * Process a BREAD request from VFS.
 */
ssize_t
fsdriver_bread(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{

	if (fdp->fdr_bread == NULL)
		return ENOSYS;

	return bread_bwrite(fdp, m_in, m_out, FSC_READ);
}

/*
 * Process a BWRITE request from VFS.
 */
ssize_t
fsdriver_bwrite(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict m_out)
{

	if (fdp->fdr_bwrite == NULL)
		return ENOSYS;

	return bread_bwrite(fdp, m_in, m_out, FSC_WRITE);
}

/*
 * Process a BPEEK request from VFS.
 */
int
fsdriver_bpeek(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	dev_t dev;
	off_t pos;
	size_t nbytes;
	ssize_t r;

	dev = m_in->m_vfs_fs_breadwrite.device;
	pos = m_in->m_vfs_fs_breadwrite.seek_pos;
	nbytes = m_in->m_vfs_fs_breadwrite.nbytes;

	if (fdp->fdr_bpeek == NULL)
		return ENOSYS;

	if (pos < 0 || nbytes > SSIZE_MAX)
		return EINVAL;

	r = fdp->fdr_bpeek(dev, NULL /*data*/, nbytes, pos, FSC_PEEK);

	/* Do not return a new position. */
	if (r >= 0) {
		m_out->m_fs_vfs_breadwrite.nbytes = r;
		r = OK;
	}

	return r;
}

/*
 * Process a FLUSH request from VFS.
 */
int
fsdriver_flush(const struct fsdriver * __restrict fdp,
	const message * __restrict m_in, message * __restrict __unused m_out)
{
	dev_t dev;

	dev = m_in->m_vfs_fs_flush.device;

	if (fdp->fdr_bflush != NULL)
		fdp->fdr_bflush(dev);

	return OK;
}
