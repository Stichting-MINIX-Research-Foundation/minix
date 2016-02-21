
#include "inc.h"

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#if 0 /* not yet, header is missing */
#include <netbt/bluetooth.h>
#endif
#include <arpa/inet.h>

/*
 * This function should always be used when printing a file descriptor.  It
 * currently offers no benefit, but will in the future allow for features such
 * as color highlighting and tracking of specific open files (TODO).
 */
void
put_fd(struct trace_proc * proc, const char * name, int fd)
{

	put_value(proc, name, "%d", fd);
}

static int
vfs_read_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_readwrite.fd);

	return CT_NOTDONE;
}

static void
vfs_read_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	put_buf(proc, "buf", failed, m_out->m_lc_vfs_readwrite.buf,
	    m_in->m_type);
	put_value(proc, "len", "%zu", m_out->m_lc_vfs_readwrite.len);
	put_equals(proc);
	put_result(proc);
}

static int
vfs_write_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_readwrite.fd);
	put_buf(proc, "buf", 0, m_out->m_lc_vfs_readwrite.buf,
	    m_out->m_lc_vfs_readwrite.len);
	put_value(proc, "len", "%zu", m_out->m_lc_vfs_readwrite.len);

	return CT_DONE;
}

static void
put_lseek_whence(struct trace_proc * proc, const char * name, int whence)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (whence) {
		TEXT(SEEK_SET);
		TEXT(SEEK_CUR);
		TEXT(SEEK_END);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", whence);
}

static int
vfs_lseek_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_lseek.fd);
	put_value(proc, "offset", "%"PRId64, m_out->m_lc_vfs_lseek.offset);
	put_lseek_whence(proc, "whence", m_out->m_lc_vfs_lseek.whence);

	return CT_DONE;
}

static void
vfs_lseek_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_value(proc, NULL, "%"PRId64, m_in->m_vfs_lc_lseek.offset);
	else
		put_result(proc);
}

static const struct flags open_flags[] = {
	FLAG_MASK(O_ACCMODE, O_RDONLY),
	FLAG_MASK(O_ACCMODE, O_WRONLY),
	FLAG_MASK(O_ACCMODE, O_RDWR),
#define ACCMODE_ENTRIES 3	/* the first N entries are for O_ACCMODE */
	FLAG(O_NONBLOCK),
	FLAG(O_APPEND),
	FLAG(O_SHLOCK),
	FLAG(O_EXLOCK),
	FLAG(O_ASYNC),
	FLAG(O_SYNC),
	FLAG(O_NOFOLLOW),
	FLAG(O_CREAT),
	FLAG(O_TRUNC),
	FLAG(O_EXCL),
	FLAG(O_NOCTTY),
	FLAG(O_DSYNC),
	FLAG(O_RSYNC),
	FLAG(O_ALT_IO),
	FLAG(O_DIRECT),
	FLAG(O_DIRECTORY),
	FLAG(O_CLOEXEC),
	FLAG(O_SEARCH),
	FLAG(O_NOSIGPIPE),
};

static void
put_open_flags(struct trace_proc * proc, const char * name, int value,
	int full)
{
	const struct flags *fp;
	unsigned int num;

	fp = open_flags;
	num = COUNT(open_flags);

	/*
	 * If we're not printing a full open()-style set of flags, but instead
	 * just a loose set of flags, then skip the access mode altogether,
	 * otherwise we'd be printing O_RDONLY when no access mode is given.
	 */
	if (!full) {
		fp += ACCMODE_ENTRIES;
		num -= ACCMODE_ENTRIES;
	}

	put_flags(proc, name, fp, num, "0x%x", value);
}

static const struct flags mode_flags[] = {
	FLAG_MASK(S_IFMT, S_IFIFO),
	FLAG_MASK(S_IFMT, S_IFCHR),
	FLAG_MASK(S_IFMT, S_IFDIR),
	FLAG_MASK(S_IFMT, S_IFBLK),
	FLAG_MASK(S_IFMT, S_IFREG),
	FLAG_MASK(S_IFMT, S_IFLNK),
	FLAG_MASK(S_IFMT, S_IFSOCK),
	FLAG_MASK(S_IFMT, S_IFWHT),
	FLAG(S_ARCH1),
	FLAG(S_ARCH2),
	FLAG(S_ISUID),
	FLAG(S_ISGID),
	FLAG(S_ISTXT),
};

/* Do not use %04o instead of 0%03o; it is octal even if greater than 0777. */
#define put_mode(p, n, v) \
	put_flags(p, n, mode_flags, COUNT(mode_flags), "0%03o", v)

static void
put_path(struct trace_proc * proc, const message * m_out)
{
	size_t len;

	if ((len = m_out->m_lc_vfs_path.len) <= M_PATH_STRING_MAX)
		put_buf(proc, "path", PF_LOCADDR | PF_PATH,
		    (vir_bytes)m_out->m_lc_vfs_path.buf, len);
	else
		put_buf(proc, "path", PF_PATH, m_out->m_lc_vfs_path.name, len);
}

static int
vfs_open_out(struct trace_proc * proc, const message * m_out)
{

	put_path(proc, m_out);
	put_open_flags(proc, "flags", m_out->m_lc_vfs_path.flags,
	    TRUE /*full*/);

	return CT_DONE;
}

/* This function is shared between creat and open. */
static void
vfs_open_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_fd(proc, NULL, m_in->m_type);
	else
		put_result(proc);
}

static int
vfs_creat_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "path", PF_PATH, m_out->m_lc_vfs_creat.name,
	    m_out->m_lc_vfs_creat.len);
	put_open_flags(proc, "flags", m_out->m_lc_vfs_creat.flags,
	    TRUE /*full*/);
	put_mode(proc, "mode", m_out->m_lc_vfs_creat.mode);

	return CT_DONE;
}

static int
vfs_close_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_close.fd);

	return CT_DONE;
}

/* This function is used for link, rename, and symlink. */
static int
vfs_link_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "path1", PF_PATH, m_out->m_lc_vfs_link.name1,
	    m_out->m_lc_vfs_link.len1);
	put_buf(proc, "path2", PF_PATH, m_out->m_lc_vfs_link.name2,
	    m_out->m_lc_vfs_link.len2);

	return CT_DONE;
}

static int
vfs_path_out(struct trace_proc * proc, const message * m_out)
{

	put_path(proc, m_out);

	return CT_DONE;
}

static int
vfs_path_mode_out(struct trace_proc * proc, const message * m_out)
{

	put_path(proc, m_out);
	put_mode(proc, "mode", m_out->m_lc_vfs_path.mode);

	return CT_DONE;
}

void
put_dev(struct trace_proc * proc, const char * name, dev_t dev)
{
	devmajor_t major;
	devminor_t minor;

	major = major(dev);
	minor = minor(dev);

	/* The value 0 ("no device") should print as "0". */
	if (dev != 0 && makedev(major, minor) == dev && !valuesonly)
		put_value(proc, name, "<%d,%d>", major, minor);
	else
		put_value(proc, name, "%"PRIu64, dev);
}

static int
vfs_mknod_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "path", PF_PATH, m_out->m_lc_vfs_mknod.name,
	    m_out->m_lc_vfs_mknod.len);
	put_mode(proc, "mode", m_out->m_lc_vfs_mknod.mode);
	put_dev(proc, "dev", m_out->m_lc_vfs_mknod.device);

	return CT_DONE;
}

static int
vfs_chown_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "path", PF_PATH, m_out->m_lc_vfs_chown.name,
	    m_out->m_lc_vfs_chown.len);
	/* -1 means "keep the current value" so print as signed */
	put_value(proc, "owner", "%d", m_out->m_lc_vfs_chown.owner);
	put_value(proc, "group", "%d", m_out->m_lc_vfs_chown.group);

	return CT_DONE;
}

/* TODO: expand this to the full ST_ set. */
static const struct flags mount_flags[] = {
	FLAG(MNT_RDONLY),
};

static int
vfs_mount_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "special", PF_PATH, m_out->m_lc_vfs_mount.dev,
	    m_out->m_lc_vfs_mount.devlen);
	put_buf(proc, "path", PF_PATH, m_out->m_lc_vfs_mount.path,
	    m_out->m_lc_vfs_mount.pathlen);
	put_flags(proc, "flags", mount_flags, COUNT(mount_flags), "0x%x",
	    m_out->m_lc_vfs_mount.flags);
	put_buf(proc, "type", PF_STRING, m_out->m_lc_vfs_mount.type,
	    m_out->m_lc_vfs_mount.typelen);
	put_buf(proc, "label", PF_STRING, m_out->m_lc_vfs_mount.label,
	    m_out->m_lc_vfs_mount.labellen);

	return CT_DONE;
}

static int
vfs_umount_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "path", PF_PATH, m_out->m_lc_vfs_umount.name,
	    m_out->m_lc_vfs_umount.namelen);

	return CT_DONE;
}

static void
vfs_umount_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{

	put_result(proc);

	if (!failed) {
		put_open(proc, NULL, 0, "(", ", ");
		put_buf(proc, "label", PF_STRING, m_out->m_lc_vfs_umount.label,
		    m_out->m_lc_vfs_umount.labellen);

		put_close(proc, ")");
	}
}


static const struct flags access_flags[] = {
	FLAG_ZERO(F_OK),
	FLAG(R_OK),
	FLAG(W_OK),
	FLAG(X_OK),
};

static int
vfs_access_out(struct trace_proc * proc, const message * m_out)
{

	put_path(proc, m_out);
	put_flags(proc, "mode", access_flags, COUNT(access_flags), "0x%x",
	    m_out->m_lc_vfs_path.mode);

	return CT_DONE;
}

static int
vfs_readlink_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "path", PF_PATH, m_out->m_lc_vfs_readlink.name,
	    m_out->m_lc_vfs_readlink.namelen);

	return CT_NOTDONE;
}

static void
vfs_readlink_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	/* The call does not return a string, so do not use PF_STRING here. */
	put_buf(proc, "buf", failed, m_out->m_lc_vfs_readlink.buf,
	    m_in->m_type);
	put_value(proc, "bufsize", "%zd", m_out->m_lc_vfs_readlink.bufsize);
	put_equals(proc);
	put_result(proc);
}

static void
put_struct_stat(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct stat buf;
	int is_special;

	if (!put_open_struct(proc, name, flags, addr, &buf, sizeof(buf)))
		return;

	/*
	 * The combination of struct stat's frequent usage and large number of
	 * fields makes this structure a pain to print.  For now, the idea is
	 * that for verbosity level 0, we print the mode, and the target device
	 * for block/char special files or the file size for all other files.
	 * For higher verbosity levels, largely maintain the structure's own
	 * order of fields.  Violate this general structure printing rule for
	 * some fields though, because the actual field order in struct stat is
	 * downright ridiculous.  Like elsewhere, for verbosity level 1 print
	 * all fields with meaningful values, and for verbosity level 2 just
	 * print everything, including fields that are known to be not yet
	 * supported and fields that contain known values.
	 */
	is_special = (S_ISBLK(buf.st_mode) || S_ISCHR(buf.st_mode));

	if (verbose > 0) {
		put_dev(proc, "st_dev", buf.st_dev);
		put_value(proc, "st_ino", "%"PRId64, buf.st_ino);
	}
	put_mode(proc, "st_mode", buf.st_mode);
	if (verbose > 0) {
		put_value(proc, "st_nlink", "%u", buf.st_nlink);
		put_value(proc, "st_uid", "%u", buf.st_uid);
		put_value(proc, "st_gid", "%u", buf.st_gid);
	}
	if (is_special || verbose > 1)
		put_dev(proc, "st_rdev", buf.st_rdev);
	if (verbose > 0) {
		/*
		 * TODO: print the nanosecond part, but possibly only if we are
		 * not actually interpreting the time as a date (another TODO),
		 * and/or possibly only with verbose > 1 (largely unsupported).
		 */
		put_time(proc, "st_atime", buf.st_atime);
		put_time(proc, "st_mtime", buf.st_mtime);
		put_time(proc, "st_ctime", buf.st_ctime);
	}
	if (verbose > 1) /* not yet supported on MINIX3 */
		put_time(proc, "st_birthtime", buf.st_birthtime);
	if (!is_special || verbose > 1)
		put_value(proc, "st_size", "%"PRId64, buf.st_size);
	if (verbose > 0) {
		put_value(proc, "st_blocks", "%"PRId64, buf.st_blocks);
		put_value(proc, "st_blksize", "%"PRId32, buf.st_blksize);
	}
	if (verbose > 1) {
		put_value(proc, "st_flags", "%"PRIu32, buf.st_flags);
		put_value(proc, "st_gen", "%"PRIu32, buf.st_gen);
	}

	put_close_struct(proc, verbose > 1);
}

static int
vfs_stat_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "path", PF_PATH, m_out->m_lc_vfs_stat.name,
	    m_out->m_lc_vfs_stat.len);

	return CT_NOTDONE;
}

static void
vfs_stat_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{

	put_struct_stat(proc, "buf", failed, m_out->m_lc_vfs_stat.buf);
	put_equals(proc);
	put_result(proc);
}

static int
vfs_fstat_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_fstat.fd);

	return CT_NOTDONE;
}

static void
vfs_fstat_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{

	put_struct_stat(proc, "buf", failed, m_out->m_lc_vfs_fstat.buf);
	put_equals(proc);
	put_result(proc);
}

static int
vfs_ioctl_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_ioctl.fd);
	put_ioctl_req(proc, "req", m_out->m_lc_vfs_ioctl.req,
	    FALSE /*is_svrctl*/);
	return put_ioctl_arg_out(proc, "arg", m_out->m_lc_vfs_ioctl.req,
	    (vir_bytes)m_out->m_lc_vfs_ioctl.arg, FALSE /*is_svrctl*/);
}

static void
vfs_ioctl_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{

	put_ioctl_arg_in(proc, "arg", failed, m_out->m_lc_vfs_ioctl.req,
	    (vir_bytes)m_out->m_lc_vfs_ioctl.arg, FALSE /*is_svrctl*/);
}

static void
put_fcntl_cmd(struct trace_proc * proc, const char * name, int cmd)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (cmd) {
		TEXT(F_DUPFD);
		TEXT(F_GETFD);
		TEXT(F_SETFD);
		TEXT(F_GETFL);
		TEXT(F_SETFL);
		TEXT(F_GETOWN);
		TEXT(F_SETOWN);
		TEXT(F_GETLK);
		TEXT(F_SETLK);
		TEXT(F_SETLKW);
		TEXT(F_CLOSEM);
		TEXT(F_MAXFD);
		TEXT(F_DUPFD_CLOEXEC);
		TEXT(F_GETNOSIGPIPE);
		TEXT(F_SETNOSIGPIPE);
		TEXT(F_FREESP);
		TEXT(F_FLUSH_FS_CACHE);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", cmd);
}

static const struct flags fd_flags[] = {
	FLAG(FD_CLOEXEC),
};

#define put_fd_flags(p, n, v) \
	put_flags(p, n, fd_flags, COUNT(fd_flags), "0x%x", v)

static void
put_flock_type(struct trace_proc * proc, const char * name, int type)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (type) {
		TEXT(F_RDLCK);
		TEXT(F_UNLCK);
		TEXT(F_WRLCK);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", type);
}

/*
 * With PF_FULL, also print l_pid, unless l_type is F_UNLCK in which case
 * only that type is printed.   With PF_ALT, print only l_whence/l_start/l_len.
 */
static void
put_struct_flock(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct flock flock;
	int limited;

	if (!put_open_struct(proc, name, flags, addr, &flock, sizeof(flock)))
		return;

	limited = ((flags & PF_FULL) && flock.l_type == F_UNLCK);

	if (!(flags & PF_ALT))
		put_flock_type(proc, "l_type", flock.l_type);
	if (!limited) {
		put_lseek_whence(proc, "l_whence", flock.l_whence);
		put_value(proc, "l_start", "%"PRId64, flock.l_start);
		put_value(proc, "l_len", "%"PRId64, flock.l_len);
		if (flags & PF_FULL)
			put_value(proc, "l_pid", "%d", flock.l_pid);
	}

	put_close_struct(proc, TRUE /*all*/);
}

static int
vfs_fcntl_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_fcntl.fd);
	put_fcntl_cmd(proc, "cmd", m_out->m_lc_vfs_fcntl.cmd);

	switch (m_out->m_lc_vfs_fcntl.cmd) {
	case F_DUPFD:
	case F_DUPFD_CLOEXEC:
		put_fd(proc, "fd2", m_out->m_lc_vfs_fcntl.arg_int);
		break;
	case F_SETFD:
		put_fd_flags(proc, "flags", m_out->m_lc_vfs_fcntl.arg_int);
		break;
	case F_SETFL:
		/*
		 * One of those difficult cases: the access mode is ignored, so
		 * we don't want to print O_RDONLY if it is not given.  On the
		 * other hand, fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_..) is
		 * a fairly common construction, in which case we don't want to
		 * print eg O_..|0x2 if the access mode is O_RDWR.  Thus, we
		 * compromise: show the access mode if any of its bits are set.
		 */
		put_open_flags(proc, "flags", m_out->m_lc_vfs_fcntl.arg_int,
		    m_out->m_lc_vfs_fcntl.arg_int & O_ACCMODE /*full*/);
		break;
	case F_SETLK:
	case F_SETLKW:
		put_struct_flock(proc, "lkp", 0,
		    m_out->m_lc_vfs_fcntl.arg_ptr);
		break;
	case F_FREESP:
		put_struct_flock(proc, "lkp", PF_ALT,
		    m_out->m_lc_vfs_fcntl.arg_ptr);
		break;
	case F_SETNOSIGPIPE:
		put_value(proc, "arg", "%d", m_out->m_lc_vfs_fcntl.arg_int);
		break;
	}

	return (m_out->m_lc_vfs_fcntl.cmd != F_GETLK) ? CT_DONE : CT_NOTDONE;
}

static void
vfs_fcntl_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	switch (m_out->m_lc_vfs_fcntl.cmd) {
	case F_GETFD:
		if (failed)
			break;
		put_fd_flags(proc, NULL, m_in->m_type);
		return;
	case F_GETFL:
		if (failed)
			break;
		put_open_flags(proc, NULL, m_in->m_type, TRUE /*full*/);
		return;
	case F_GETLK:
		put_struct_flock(proc, "lkp", failed | PF_FULL,
		    m_out->m_lc_vfs_fcntl.arg_ptr);
		put_equals(proc);
		break;
	}

	put_result(proc);
}

static int
vfs_pipe2_out(struct trace_proc * __unused proc,
	const message * __unused m_out)
{

	return CT_NOTDONE;
}

static void
vfs_pipe2_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	if (!failed) {
		put_open(proc, "fd", PF_NONAME, "[", ", ");
		put_fd(proc, "rfd", m_in->m_vfs_lc_fdpair.fd0);
		put_fd(proc, "wfd", m_in->m_vfs_lc_fdpair.fd1);
		put_close(proc, "]");
	} else
		put_field(proc, "fd", "&..");
	put_open_flags(proc, "flags", m_out->m_lc_vfs_pipe2.flags,
	    FALSE /*full*/);
	put_equals(proc);
	put_result(proc);
}

static int
vfs_umask_out(struct trace_proc * proc, const message * m_out)
{

	put_mode(proc, NULL, m_out->m_lc_vfs_umask.mask);

	return CT_DONE;
}

static void
vfs_umask_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_mode(proc, NULL, m_in->m_type);
	else
		put_result(proc);

}

static void
put_dirent_type(struct trace_proc * proc, const char * name, unsigned int type)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (type) {
		TEXT(DT_UNKNOWN);
		TEXT(DT_FIFO);
		TEXT(DT_CHR);
		TEXT(DT_DIR);
		TEXT(DT_BLK);
		TEXT(DT_REG);
		TEXT(DT_LNK);
		TEXT(DT_SOCK);
		TEXT(DT_WHT);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%u", type);
}

static void
put_struct_dirent(struct trace_proc * proc, const char *name, int flags,
	vir_bytes addr)
{
	struct dirent dirent;

	if (!put_open_struct(proc, name, flags, addr, &dirent, sizeof(dirent)))
		return;

	if (verbose > 0)
		put_value(proc, "d_fileno", "%"PRIu64, dirent.d_fileno);
	if (verbose > 1) {
		put_value(proc, "d_reclen", "%u", dirent.d_reclen);
		put_value(proc, "d_namlen", "%u", dirent.d_namlen);
	}
	if (verbose >= 1 + (dirent.d_type == DT_UNKNOWN))
		put_dirent_type(proc, "d_type", dirent.d_type);
	put_buf(proc, "d_name", PF_LOCADDR, (vir_bytes)dirent.d_name,
	    MIN(dirent.d_namlen, sizeof(dirent.d_name)));

	put_close_struct(proc, verbose > 1);
}

static void
put_dirent_array(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, ssize_t size)
{
	struct dirent dirent;
	unsigned count, max;
	ssize_t off, chunk;

	if ((flags & PF_FAILED) || valuesonly > 1 || size < 0) {
		put_ptr(proc, name, addr);

		return;
	}

	if (size == 0) {
		put_field(proc, name, "[]");

		return;
	}

	if (verbose == 0)
		max = 0; /* TODO: should we set this to 1 instead? */
	else if (verbose == 1)
		max = 3; /* low; just to give an indication where we are */
	else
		max = INT_MAX;

	/*
	 * TODO: as is, this is highly inefficient, as we are typically copying
	 * in the same pieces of memory in repeatedly..
	 */
	count = 0;
	for (off = 0; off < size; off += chunk) {
		chunk = size - off;
		if ((size_t)chunk > sizeof(dirent))
			chunk = (ssize_t)sizeof(dirent);
		if ((size_t)chunk < _DIRENT_MINSIZE(&dirent))
			break;

		if (mem_get_data(proc->pid, addr + off, &dirent, chunk) < 0) {
			if (off == 0) {
				put_ptr(proc, name, addr);

				return;
			}

			break;
		}

		if (off == 0)
			put_open(proc, name, PF_NONAME, "[", ", ");

		if (count < max)
			put_struct_dirent(proc, NULL, PF_LOCADDR,
			    (vir_bytes)&dirent);

		if (chunk > dirent.d_reclen)
			chunk = dirent.d_reclen;
		count++;
	}

	if (off < size)
		put_tail(proc, 0, 0);
	else if (count > max)
		put_tail(proc, count, max);
	put_close(proc, "]");
}

static int
vfs_getdents_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_readwrite.fd);

	return CT_NOTDONE;
}

static void
vfs_getdents_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	put_dirent_array(proc, "buf", failed, m_out->m_lc_vfs_readwrite.buf,
	    m_in->m_type);
	put_value(proc, "len", "%zu", m_out->m_lc_vfs_readwrite.len);
	put_equals(proc);
	put_result(proc);
}

static void
put_fd_set(struct trace_proc * proc, const char * name, vir_bytes addr,
	int nfds)
{
	fd_set set;
	size_t off;
	unsigned int i, j, words, count, max;

	if (addr == 0 || nfds < 0) {
		put_ptr(proc, name, addr);

		return;
	}

	/*
	 * Each process may define its own FD_SETSIZE, so our fd_set may be of
	 * a different size than theirs.  Thus, we copy at a granularity known
	 * to be valid in any case: a single word of bits.  We make the
	 * assumption that fd_set consists purely of bits, so that we can use
	 * the second (and so on) bit word as an fd_set by itself.
	 */
	words = (nfds + NFDBITS - 1) / NFDBITS;

	count = 0;

	if (verbose == 0)
		max = 16;
	else if (verbose == 1)
		max = FD_SETSIZE;
	else
		max = INT_MAX;

	/* TODO: copy in more at once, but stick to fd_mask boundaries. */
	for (off = 0, i = 0; i < words; i++, off += sizeof(fd_mask)) {
		if (mem_get_data(proc->pid, addr + off, &set,
		    sizeof(fd_mask)) != 0) {
			if (count == 0) {
				put_ptr(proc, name, addr);

				return;
			}

			break;
		}

		for (j = 0; j < NFDBITS; j++) {
			if (FD_ISSET(j, &set)) {
				if (count == 0)
					put_open(proc, name, PF_NONAME, "[",
					    " ");

				if (count < max)
					put_fd(proc, NULL, i * NFDBITS + j);

				count++;
			}
		}
	}

	/*
	 * The empty set should print as "[]".  If copying any part failed, it
	 * should print as "[x, ..(?)]" where x is the set printed so far, if
	 * any.  If copying never failed, and we did not print all fds in the
	 * set, print the remaining count n as "[x, ..(+n)]" at the end.
	 */
	if (count == 0)
		put_open(proc, name, PF_NONAME, "[", " ");

	if (i < words)
		put_tail(proc, 0, 0);
	else if (count > max)
		put_tail(proc, count, max);

	put_close(proc, "]");
}

static int
vfs_select_out(struct trace_proc * proc, const message * m_out)
{
	int nfds;

	nfds = m_out->m_lc_vfs_select.nfds;

	put_fd(proc, "nfds", nfds); /* not really a file descriptor.. */
	put_fd_set(proc, "readfds",
	    (vir_bytes)m_out->m_lc_vfs_select.readfds, nfds);
	put_fd_set(proc, "writefds",
	    (vir_bytes)m_out->m_lc_vfs_select.writefds, nfds);
	put_fd_set(proc, "errorfds",
	    (vir_bytes)m_out->m_lc_vfs_select.errorfds, nfds);
	put_struct_timeval(proc, "timeout", 0, m_out->m_lc_vfs_select.timeout);

	return CT_DONE;
}

static void
vfs_select_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{
	vir_bytes readfds, writefds, errorfds;
	int nfds;

	put_result(proc);
	if (failed)
		return;

	nfds = m_out->m_lc_vfs_select.nfds;

	readfds = (vir_bytes)m_out->m_lc_vfs_select.readfds;
	writefds = (vir_bytes)m_out->m_lc_vfs_select.writefds;
	errorfds = (vir_bytes)m_out->m_lc_vfs_select.errorfds;

	if (readfds == 0 && writefds == 0 && errorfds == 0)
		return;

	/* Omit names, because it looks weird. */
	put_open(proc, NULL, PF_NONAME, "(", ", ");
	if (readfds != 0)
		put_fd_set(proc, "readfds", readfds, nfds);
	if (writefds != 0)
		put_fd_set(proc, "writefds", writefds, nfds);
	if (errorfds != 0)
		put_fd_set(proc, "errorfds", errorfds, nfds);
	put_close(proc, ")");
}

static int
vfs_fchdir_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_fchdir.fd);

	return CT_DONE;
}

static int
vfs_fsync_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_fsync.fd);

	return CT_DONE;
}

static int
vfs_truncate_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "path", PF_PATH, m_out->m_lc_vfs_truncate.name,
	    m_out->m_lc_vfs_truncate.len);
	put_value(proc, "length", "%"PRId64, m_out->m_lc_vfs_truncate.offset);

	return CT_DONE;
}

static int
vfs_ftruncate_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_truncate.fd);
	put_value(proc, "length", "%"PRId64, m_out->m_lc_vfs_truncate.offset);

	return CT_DONE;
}

static int
vfs_fchmod_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_fchmod.fd);
	put_mode(proc, "mode", m_out->m_lc_vfs_fchmod.mode);

	return CT_DONE;
}

static int
vfs_fchown_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_chown.fd);
	/* -1 means "keep the current value" so print as signed */
	put_value(proc, "owner", "%d", m_out->m_lc_vfs_chown.owner);
	put_value(proc, "group", "%d", m_out->m_lc_vfs_chown.group);

	return CT_DONE;
}

static const char *
vfs_utimens_name(const message * m_out)
{
	int has_path, has_flags;

	has_path = (m_out->m_vfs_utimens.name != NULL);
	has_flags = (m_out->m_vfs_utimens.flags != 0);

	if (has_path && m_out->m_vfs_utimens.flags == AT_SYMLINK_NOFOLLOW)
		return "lutimens";
	if (has_path && !has_flags)
		return "utimens";
	else if (!has_path && !has_flags)
		return "futimens";
	else
		return "utimensat";
}

static const struct flags at_flags[] = {
	FLAG(AT_EACCESS),
	FLAG(AT_SYMLINK_NOFOLLOW),
	FLAG(AT_SYMLINK_FOLLOW),
	FLAG(AT_REMOVEDIR),
};

static void
put_utimens_timespec(struct trace_proc * proc, const char * name,
	time_t sec, long nsec)
{

	/* No field names. */
	put_open(proc, name, PF_NONAME, "{", ", ");

	put_time(proc, "tv_sec", sec);

	if (!valuesonly && nsec == UTIME_NOW)
		put_field(proc, "tv_nsec", "UTIME_NOW");
	else if (!valuesonly && nsec == UTIME_OMIT)
		put_field(proc, "tv_nsec", "UTIME_OMIT");
	else
		put_value(proc, "tv_nsec", "%ld", nsec);

	put_close(proc, "}");
}

static int
vfs_utimens_out(struct trace_proc * proc, const message * m_out)
{
	int has_path, has_flags;

	/* Here we do not care about the utimens/lutimens distinction. */
	has_path = (m_out->m_vfs_utimens.name != NULL);
	has_flags = !!(m_out->m_vfs_utimens.flags & ~AT_SYMLINK_NOFOLLOW);

	if (has_path && has_flags)
		put_field(proc, "fd", "AT_CWD"); /* utimensat */
	else if (!has_path)
		put_fd(proc, "fd", m_out->m_vfs_utimens.fd); /* futimes */
	if (has_path || has_flags) /* lutimes, utimes, utimensat */
		put_buf(proc, "path", PF_PATH,
		    (vir_bytes)m_out->m_vfs_utimens.name,
		    m_out->m_vfs_utimens.len);

	put_open(proc, "times", 0, "[", ", ");
	put_utimens_timespec(proc, "atime", m_out->m_vfs_utimens.atime,
	    m_out->m_vfs_utimens.ansec);
	put_utimens_timespec(proc, "mtime", m_out->m_vfs_utimens.mtime,
	    m_out->m_vfs_utimens.mnsec);
	put_close(proc, "]");

	if (has_flags)
		put_flags(proc, "flag", at_flags, COUNT(at_flags), "0x%x",
		    m_out->m_vfs_utimens.flags);

	return CT_DONE;
}

static const struct flags statvfs_flags[] = {
	FLAG(ST_WAIT),
	FLAG(ST_NOWAIT),
};

static const struct flags st_flags[] = {
	FLAG(ST_RDONLY),
	FLAG(ST_SYNCHRONOUS),
	FLAG(ST_NOEXEC),
	FLAG(ST_NOSUID),
	FLAG(ST_NODEV),
	FLAG(ST_UNION),
	FLAG(ST_ASYNC),
	FLAG(ST_NOCOREDUMP),
	FLAG(ST_RELATIME),
	FLAG(ST_IGNORE),
	FLAG(ST_NOATIME),
	FLAG(ST_SYMPERM),
	FLAG(ST_NODEVMTIME),
	FLAG(ST_SOFTDEP),
	FLAG(ST_LOG),
	FLAG(ST_EXTATTR),
	FLAG(ST_EXRDONLY),
	FLAG(ST_EXPORTED),
	FLAG(ST_DEFEXPORTED),
	FLAG(ST_EXPORTANON),
	FLAG(ST_EXKERB),
	FLAG(ST_EXNORESPORT),
	FLAG(ST_EXPUBLIC),
	FLAG(ST_LOCAL),
	FLAG(ST_QUOTA),
	FLAG(ST_ROOTFS),
	FLAG(ST_NOTRUNC),
};

static void
put_struct_statvfs(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr)
{
	struct statvfs buf;

	if (!put_open_struct(proc, name, flags, addr, &buf, sizeof(buf)))
		return;

	put_flags(proc, "f_flag", st_flags, COUNT(st_flags), "0x%x",
	    buf.f_flag);
	put_value(proc, "f_bsize", "%lu", buf.f_bsize);
	if (verbose > 0 || buf.f_bsize != buf.f_frsize)
		put_value(proc, "f_frsize", "%lu", buf.f_frsize);
	if (verbose > 1)
		put_value(proc, "f_iosize", "%lu", buf.f_iosize);

	put_value(proc, "f_blocks", "%"PRIu64, buf.f_blocks);
	put_value(proc, "f_bfree", "%"PRIu64, buf.f_bfree);
	if (verbose > 1) {
		put_value(proc, "f_bavail", "%"PRIu64, buf.f_bavail);
		put_value(proc, "f_bresvd", "%"PRIu64, buf.f_bresvd);
	}

	if (verbose > 0) {
		put_value(proc, "f_files", "%"PRIu64, buf.f_files);
		put_value(proc, "f_ffree", "%"PRIu64, buf.f_ffree);
	}
	if (verbose > 1) {
		put_value(proc, "f_favail", "%"PRIu64, buf.f_favail);
		put_value(proc, "f_fresvd", "%"PRIu64, buf.f_fresvd);
	}

	if (verbose > 1) {
		put_value(proc, "f_syncreads", "%"PRIu64, buf.f_syncreads);
		put_value(proc, "f_syncwrites", "%"PRIu64, buf.f_syncwrites);
		put_value(proc, "f_asyncreads", "%"PRIu64, buf.f_asyncreads);
		put_value(proc, "f_asyncwrites", "%"PRIu64, buf.f_asyncwrites);

		put_value(proc, "f_fsidx", "<%"PRId32",%"PRId32">",
		    buf.f_fsidx.__fsid_val[0], buf.f_fsidx.__fsid_val[1]);
	}
	put_dev(proc, "f_fsid", buf.f_fsid); /* MINIX3 interpretation! */

	if (verbose > 0)
		put_value(proc, "f_namemax", "%lu", buf.f_namemax);
	if (verbose > 1)
		put_value(proc, "f_owner", "%u", buf.f_owner);

	put_buf(proc, "f_fstypename", PF_STRING | PF_LOCADDR,
	    (vir_bytes)&buf.f_fstypename, sizeof(buf.f_fstypename));
	if (verbose > 0)
		put_buf(proc, "f_mntfromname", PF_STRING | PF_LOCADDR,
		    (vir_bytes)&buf.f_mntfromname, sizeof(buf.f_mntfromname));
	put_buf(proc, "f_mntonname", PF_STRING | PF_LOCADDR,
	    (vir_bytes)&buf.f_mntonname, sizeof(buf.f_mntonname));

	put_close_struct(proc, verbose > 1);
}

static void
put_statvfs_array(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, int count)
{
	struct statvfs buf;
	int i, max;

	if ((flags & PF_FAILED) || valuesonly > 1 || count < 0) {
		put_ptr(proc, name, addr);

		return;
	}

	if (count == 0) {
		put_field(proc, name, "[]");

		return;
	}

	if (verbose == 0)
		max = 0;
	else if (verbose == 1)
		max = 1; /* TODO: is this reasonable? */
	else
		max = INT_MAX;

	if (max > count)
		max = count;

	for (i = 0; i < max; i++) {
		if (mem_get_data(proc->pid, addr + i * sizeof(buf), &buf,
		    sizeof(buf)) < 0) {
			if (i == 0) {
				put_ptr(proc, name, addr);

				return;
			}

			break;
		}

		if (i == 0)
			put_open(proc, name, PF_NONAME, "[", ", ");

		put_struct_statvfs(proc, NULL, PF_LOCADDR, (vir_bytes)&buf);
	}

	if (i == 0)
		put_open(proc, name, PF_NONAME, "[", ", ");
	if (i < max)
		put_tail(proc, 0, 0);
	else if (count > i)
		put_tail(proc, count, i);
	put_close(proc, "]");
}

static int
vfs_getvfsstat_out(struct trace_proc * proc, const message * m_out)
{

	if (m_out->m_lc_vfs_getvfsstat.buf == 0) {
		put_ptr(proc, "buf", m_out->m_lc_vfs_getvfsstat.buf);
		put_value(proc, "bufsize", "%zu",
		    m_out->m_lc_vfs_getvfsstat.len);
		put_flags(proc, "flags", statvfs_flags, COUNT(statvfs_flags),
		    "%d", m_out->m_lc_vfs_getvfsstat.flags);
		return CT_DONE;
	} else
		return CT_NOTDONE;
}

static void
vfs_getvfsstat_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	if (m_out->m_lc_vfs_getvfsstat.buf != 0) {
		put_statvfs_array(proc, "buf", failed,
		    m_out->m_lc_vfs_getvfsstat.buf, m_in->m_type);
		put_value(proc, "bufsize", "%zu",
		    m_out->m_lc_vfs_getvfsstat.len);
		put_flags(proc, "flags", statvfs_flags, COUNT(statvfs_flags),
		    "%d", m_out->m_lc_vfs_getvfsstat.flags);
		put_equals(proc);
	}
	put_result(proc);
}

static int
vfs_statvfs1_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "path", PF_PATH, m_out->m_lc_vfs_statvfs1.name,
	    m_out->m_lc_vfs_statvfs1.len);

	return CT_NOTDONE;
}

static void
vfs_statvfs1_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{

	put_struct_statvfs(proc, "buf", failed, m_out->m_lc_vfs_statvfs1.buf);
	put_flags(proc, "flags", statvfs_flags, COUNT(statvfs_flags), "%d",
	    m_out->m_lc_vfs_statvfs1.flags);
	put_equals(proc);
	put_result(proc);
}

/* This function is shared between statvfs1 and fstatvfs1. */
static int
vfs_fstatvfs1_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_statvfs1.fd);

	return CT_NOTDONE;
}

static int
vfs_svrctl_out(struct trace_proc * proc, const message * m_out)
{

	put_ioctl_req(proc, "request", m_out->m_lc_svrctl.request,
	    TRUE /*is_svrctl*/);
	return put_ioctl_arg_out(proc, "arg", m_out->m_lc_svrctl.request,
	    m_out->m_lc_svrctl.arg, TRUE /*is_svrctl*/);
}

static void
vfs_svrctl_in(struct trace_proc * proc, const message * m_out,
	const message * __unused m_in, int failed)
{

	put_ioctl_arg_in(proc, "arg", failed, m_out->m_lc_svrctl.request,
	    m_out->m_lc_svrctl.arg, TRUE /*is_svrctl*/);
}

static int
vfs_gcov_flush_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "label", PF_STRING, m_out->m_lc_vfs_gcov.label,
	    m_out->m_lc_vfs_gcov.labellen);
	put_ptr(proc, "buff", m_out->m_lc_vfs_gcov.buf);
	put_value(proc, "buff_sz", "%zu", m_out->m_lc_vfs_gcov.buflen);

	return CT_DONE;
}

void
put_socket_family(struct trace_proc * proc, const char * name, int family)
{
	const char *text = NULL;

	if (!valuesonly) {
		/*
		 * For socket(2) and socketpair(2) this should really be using
		 * the prefix "PF_" since those functions take a protocol
		 * family rather than an address family.  This rule is applied
		 * fairly consistently within the system.  Here I caved because
		 * I don't want to duplicate this entire function just for the
		 * one letter.  There are exceptions however; some names only
		 * exist as "PF_".
		 */
		switch (family) {
		TEXT(AF_UNSPEC);
		TEXT(AF_LOCAL);
		TEXT(AF_INET);
		TEXT(AF_IMPLINK);
		TEXT(AF_PUP);
		TEXT(AF_CHAOS);
		TEXT(AF_NS);
		TEXT(AF_ISO);
		TEXT(AF_ECMA);
		TEXT(AF_DATAKIT);
		TEXT(AF_CCITT);
		TEXT(AF_SNA);
		TEXT(AF_DECnet);
		TEXT(AF_DLI);
		TEXT(AF_LAT);
		TEXT(AF_HYLINK);
		TEXT(AF_APPLETALK);
		TEXT(AF_OROUTE);
		TEXT(AF_LINK);
		TEXT(PF_XTP);
		TEXT(AF_COIP);
		TEXT(AF_CNT);
		TEXT(PF_RTIP);
		TEXT(AF_IPX);
		TEXT(AF_INET6);
		TEXT(PF_PIP);
		TEXT(AF_ISDN);
		TEXT(AF_NATM);
		TEXT(AF_ARP);
		TEXT(PF_KEY);
		TEXT(AF_BLUETOOTH);
		TEXT(AF_IEEE80211);
		TEXT(AF_MPLS);
		TEXT(AF_ROUTE);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", family);
}

static const struct flags socket_types[] = {
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_STREAM),
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_DGRAM),
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_RAW),
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_RDM),
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_SEQPACKET),
	FLAG_MASK(~SOCK_FLAGS_MASK, SOCK_CONN_DGRAM),
	FLAG(SOCK_CLOEXEC),
	FLAG(SOCK_NONBLOCK),
	FLAG(SOCK_NOSIGPIPE),
};

void
put_socket_type(struct trace_proc * proc, const char * name, int type)
{

	put_flags(proc, name, socket_types, COUNT(socket_types), "%d", type);
}

static void
put_socket_protocol(struct trace_proc * proc, const char * name, int family,
	int type, int protocol)
{
	const char *text = NULL;

	if (!valuesonly && (type == SOCK_RAW || protocol != 0)) {
		switch (family) {
		case PF_INET:
		case PF_INET6:
			/* TODO: is this all that is used in socket(2)? */
			switch (protocol) {
			TEXT(IPPROTO_IP);
			TEXT(IPPROTO_ICMP);
			TEXT(IPPROTO_IGMP);
			TEXT(IPPROTO_TCP);
			TEXT(IPPROTO_UDP);
			TEXT(IPPROTO_ICMPV6);
			TEXT(IPPROTO_RAW);
			}
			break;
#if 0 /* not yet */
		case PF_BLUETOOTH:
			switch (protocol) {
			TEXT(BTPROTO_HCI);
			TEXT(BTPROTO_L2CAP);
			TEXT(BTPROTO_RFCOMM);
			TEXT(BTPROTO_SCO);
			}
			break;
#endif
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", protocol);
}

static int
vfs_socket_out(struct trace_proc * proc, const message * m_out)
{

	put_socket_family(proc, "domain", m_out->m_lc_vfs_socket.domain);
	put_socket_type(proc, "type", m_out->m_lc_vfs_socket.type);
	put_socket_protocol(proc, "protocol", m_out->m_lc_vfs_socket.domain,
	    m_out->m_lc_vfs_socket.type & ~SOCK_FLAGS_MASK,
	    m_out->m_lc_vfs_socket.protocol);

	return CT_DONE;
}

static int
vfs_socketpair_out(struct trace_proc * proc, const message * m_out)
{

	put_socket_family(proc, "domain", m_out->m_lc_vfs_socket.domain);
	put_socket_type(proc, "type", m_out->m_lc_vfs_socket.type);
	put_socket_protocol(proc, "protocol", m_out->m_lc_vfs_socket.domain,
	    m_out->m_lc_vfs_socket.type & ~SOCK_FLAGS_MASK,
	    m_out->m_lc_vfs_socket.protocol);

	return CT_NOTDONE;
}

static void
vfs_socketpair_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	if (!failed) {
		put_open(proc, "fd", PF_NONAME, "[", ", ");
		put_fd(proc, "fd0", m_in->m_vfs_lc_fdpair.fd0);
		put_fd(proc, "fd1", m_in->m_vfs_lc_fdpair.fd1);
		put_close(proc, "]");
	} else
		put_field(proc, "fd", "&..");
	put_equals(proc);
	put_result(proc);
}

void
put_in_addr(struct trace_proc * proc, const char * name, struct in_addr in)
{

	if (!valuesonly) {
		/* Is this an acceptable encapsulation? */
		put_value(proc, name, "[%s]", inet_ntoa(in));
	} else
		put_value(proc, name, "0x%08x", ntohl(in.s_addr));
}

static void
put_in6_addr(struct trace_proc * proc, const char * name, struct in6_addr * in)
{
	char buf[INET6_ADDRSTRLEN];
	const char *ptr;
	unsigned int i, n;

	if (!valuesonly &&
	    (ptr = inet_ntop(AF_INET6, in, buf, sizeof(buf))) != NULL) {
		put_value(proc, name, "[%s]", ptr);
	} else {
		for (i = n = 0; i < 16; i++)
			n += snprintf(buf + n, sizeof(buf) - n, "%02x",
			    ((unsigned char *)in)[i]);
		put_value(proc, name, "0x%s", buf);
	}
}

static void
put_struct_sockaddr(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, socklen_t addr_len)
{
	char buf[UCHAR_MAX + 1];
	uint8_t len;
	sa_family_t family;
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	int all, off, left;

	/*
	 * For UNIX domain sockets, make sure there's always room to add a
	 * trailing NULL byte, because UDS paths are not necessarily null
	 * terminated.
	 */
	if (addr_len < offsetof(struct sockaddr, sa_data) ||
	    addr_len >= sizeof(buf)) {
		put_ptr(proc, name, addr);

		return;
	}

	if (!put_open_struct(proc, name, flags, addr, buf, addr_len))
		return;

	memcpy(&sa, buf, sizeof(sa));
	len = sa.sa_len;
	family = sa.sa_family;
	all = (verbose > 1);

	switch (family) {
	case AF_LOCAL:
		if (verbose > 1)
			put_value(proc, "sun_len", "%u", len);
		if (verbose > 0)
			put_socket_family(proc, "sun_family", family);
		off = (int)offsetof(struct sockaddr_un, sun_path);
		left = addr_len - off;
		if (left > 0) {
			buf[addr_len] = 0; /* force null termination */
			put_buf(proc, "sun_path", PF_LOCADDR | PF_PATH,
			    (vir_bytes)&buf[off],
			    left + 1 /* include null byte */);
		}
		break;
	case AF_INET:
		if (verbose > 1)
			put_value(proc, "sin_len", "%u", len);
		if (verbose > 0)
			put_socket_family(proc, "sin_family", family);
		if (addr_len == sizeof(sin)) {
			memcpy(&sin, buf, sizeof(sin));
			put_value(proc, "sin_port", "%u", ntohs(sin.sin_port));
			put_in_addr(proc, "sin_addr", sin.sin_addr);
		} else
			all = FALSE;
		break;
	case AF_INET6:
		if (verbose > 1)
			put_value(proc, "sin6_len", "%u", len);
		if (verbose > 0)
			put_socket_family(proc, "sin6_family", family);
		if (addr_len == sizeof(sin6)) {
			memcpy(&sin6, buf, sizeof(sin6));
			put_value(proc, "sin6_port", "%u",
			    ntohs(sin6.sin6_port));
			if (verbose > 1)
				put_value(proc, "sin6_flowinfo", "%"PRIu32,
				    sin6.sin6_flowinfo);
			put_in6_addr(proc, "sin6_addr", &sin6.sin6_addr);
			if (IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr) ||
			    IN6_IS_ADDR_MC_NODELOCAL(&sin6.sin6_addr) ||
			    IN6_IS_ADDR_MC_LINKLOCAL(&sin6.sin6_addr) ||
			    verbose > 0)
				put_value(proc, "sin6_scope_id", "%"PRIu32,
				    sin6.sin6_scope_id);
		} else
			all = FALSE;
		break;
	/* TODO: support for other address families */
	default:
		if (verbose > 1)
			put_value(proc, "sa_len", "%u", len);
		put_socket_family(proc, "sa_family", family);
		all = (verbose > 1 && family == AF_UNSPEC);
	}

	put_close_struct(proc, all);
}

/* This function is shared between bind and connect. */
static int
vfs_bind_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_sockaddr.fd);
	put_struct_sockaddr(proc, "addr", 0, m_out->m_lc_vfs_sockaddr.addr,
	    m_out->m_lc_vfs_sockaddr.addr_len);
	put_value(proc, "addr_len", "%u", m_out->m_lc_vfs_sockaddr.addr_len);

	return CT_DONE;
}

static int
vfs_listen_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_listen.fd);
	put_value(proc, "backlog", "%d", m_out->m_lc_vfs_listen.backlog);

	return CT_DONE;
}

static int
vfs_accept_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_sockaddr.fd);

	return CT_NOTDONE;
}

static void
vfs_accept_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	put_struct_sockaddr(proc, "addr", failed,
	    m_out->m_lc_vfs_sockaddr.addr, m_in->m_vfs_lc_socklen.len);
	/*
	 * We print the resulting address length rather than the given buffer
	 * size here, as we do in recvfrom, getsockname, getpeername, and (less
	 * explicitly) recvmsg.  We could also print both, by adding the
	 * resulting length after the call result.
	 */
	if (m_out->m_lc_vfs_sockaddr.addr == 0)
		put_field(proc, "addr_len", "NULL");
	else if (!failed)
		put_value(proc, "addr_len", "{%u}",
		    m_in->m_vfs_lc_socklen.len);
	else
		put_field(proc, "addr_len", "&..");

	put_equals(proc);
	put_result(proc);
}

static const struct flags msg_flags[] = {
	FLAG(MSG_OOB),
	FLAG(MSG_PEEK),
	FLAG(MSG_DONTROUTE),
	FLAG(MSG_EOR),
	FLAG(MSG_TRUNC),
	FLAG(MSG_CTRUNC),
	FLAG(MSG_WAITALL),
	FLAG(MSG_DONTWAIT),
	FLAG(MSG_BCAST),
	FLAG(MSG_MCAST),
#ifdef MSG_NOSIGNAL
	FLAG(MSG_NOSIGNAL),
#endif
	FLAG(MSG_CMSG_CLOEXEC),
	FLAG(MSG_NBIO),
	FLAG(MSG_WAITFORONE),
};

static int
vfs_sendto_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_sendrecv.fd);
	put_buf(proc, "buf", 0, m_out->m_lc_vfs_sendrecv.buf,
	    m_out->m_lc_vfs_readwrite.len);
	put_value(proc, "len", "%zu", m_out->m_lc_vfs_sendrecv.len);
	put_flags(proc, "flags", msg_flags, COUNT(msg_flags), "0x%x",
	    m_out->m_lc_vfs_sendrecv.flags);
	put_struct_sockaddr(proc, "addr", 0, m_out->m_lc_vfs_sendrecv.addr,
	    m_out->m_lc_vfs_sendrecv.addr_len);
	put_value(proc, "addr_len", "%u", m_out->m_lc_vfs_sendrecv.addr_len);

	return CT_DONE;
}

static void
put_struct_iovec(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, int len, ssize_t bmax)
{
	struct iovec iov;
	size_t bytes;
	int i, imax;

	/*
	 * For simplicity and clarity reasons, we currently print the I/O
	 * vector as an array of data elements rather than an array of
	 * structures.  We also copy in each element separately, because as of
	 * writing there is no system support for more than one element anyway.
	 * All of this may be changed later.
	 */
	if ((flags & PF_FAILED) || valuesonly > 1 || addr == 0 || len < 0) {
		put_ptr(proc, name, addr);

		return;
	}

	if (len == 0 || bmax == 0) {
		put_field(proc, name, "[]");

		return;
	}

	/* As per logic below, 'imax' must be set to a nonzero value here. */
	if (verbose == 0)
		imax = 4;
	else if (verbose == 1)
		imax = 16;
	else
		imax = INT_MAX;

	for (i = 0; i < len && bmax > 0; i++) {
		if (mem_get_data(proc->pid, addr, &iov, sizeof(iov)) < 0) {
			if (i == 0) {
				put_ptr(proc, name, addr);

				return;
			}

			len = imax = 0; /* make put_tail() print an error */
			break;
		}

		if (i == 0)
			put_open(proc, name, 0, "[", ", ");

		bytes = MIN(iov.iov_len, (size_t)bmax);

		if (len < imax)
			put_buf(proc, NULL, 0, (vir_bytes)iov.iov_base, bytes);

		addr += sizeof(struct iovec);
		bmax -= bytes;
	}

	if (imax == 0 || imax < len)
		put_tail(proc, len, imax);
	put_close(proc, "]");
}

static void
put_struct_sockcred(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, size_t left)
{
	struct sockcred sc;

	if (!put_open_struct(proc, name, flags, addr, &sc, sizeof(sc)))
		return;

	put_value(proc, "sc_uid", "%u", sc.sc_uid);
	if (verbose > 0)
		put_value(proc, "sc_euid", "%u", sc.sc_euid);
	put_value(proc, "sc_gid", "%u", sc.sc_gid);
	if (verbose > 0) {
		put_value(proc, "sc_egid", "%u", sc.sc_egid);
		if (verbose > 1)
			put_value(proc, "sc_ngroups", "%d", sc.sc_ngroups);
		if (left >= sizeof(sc.sc_groups[0]) * (sc.sc_ngroups - 1)) {
			put_groups(proc, "sc_groups", flags,
			    addr + offsetof(struct sockcred, sc_groups),
			    sc.sc_ngroups);
		} else
			put_field(proc, "sc_groups", "..");
	}

	put_close_struct(proc, verbose > 1);
}

static void
put_socket_level(struct trace_proc * proc, const char * name, int level)
{

	/*
	 * Unfortunately, the level is a domain-specific protocol number.  That
	 * means that without knowing how the socket was created, we cannot
	 * tell what it means.  The only thing we can print is SOL_SOCKET,
	 * which is the same across all domains.
	 */
	if (!valuesonly && level == SOL_SOCKET)
		put_field(proc, name, "SOL_SOCKET");
	else
		put_value(proc, name, "%d", level);
}

void
put_cmsg_type(struct trace_proc * proc, const char * name, int type)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (type) {
		TEXT(SCM_RIGHTS);
		TEXT(SCM_CREDS);
		TEXT(SCM_TIMESTAMP);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", type);
}

static void
put_cmsg_rights(struct trace_proc * proc, const char * name, char * buf,
	size_t size, char * cptr, size_t chunk, vir_bytes addr, size_t len)
{
	unsigned int i, nfds;
	int *ptr;

	put_open(proc, name, PF_NONAME, "[", ", ");

	/*
	 * Since file descriptors are important, we print them all, regardless
	 * of the current verbosity level.  Start with the file descriptors
	 * that are already copied into the local buffer.
	 */
	ptr = (int *)cptr;
	chunk = MIN(chunk, len);

	nfds = chunk / sizeof(int);
	for (i = 0; i < nfds; i++)
		put_fd(proc, NULL, ptr[i]);

	/* Then do the remaining file descriptors, in chunks. */
	size -= size % sizeof(int);

	for (len -= chunk; len >= sizeof(int); len -= chunk) {
		chunk = MIN(len, size);

		if (mem_get_data(proc->pid, addr, buf, chunk) < 0) {
			put_field(proc, NULL, "..");

			break;
		}

		ptr = (int *)buf;
		nfds = chunk / sizeof(int);
		for (i = 0; i < nfds; i++)
			put_fd(proc, NULL, ptr[i]);

		addr += chunk;
	}

	put_close(proc, "]");
}

static void
put_cmsg(struct trace_proc * proc, const char * name, vir_bytes addr,
	size_t len)
{
	struct cmsghdr cmsg;
	char buf[CMSG_SPACE(sizeof(struct sockcred))];
	size_t off, chunk, datalen;

	if (valuesonly > 1 || addr == 0 || len < CMSG_LEN(0)) {
		put_ptr(proc, name, addr);

		return;
	}

	for (off = 0; off < len; off += CMSG_SPACE(datalen)) {
		chunk = MIN(len - off, sizeof(buf));

		if (chunk < CMSG_LEN(0))
			break;

		if (mem_get_data(proc->pid, addr + off, buf, chunk) < 0) {
			if (off == 0) {
				put_ptr(proc, name, addr);

				return;
			}
			break;
		}

		if (off == 0)
			put_open(proc, name, 0, "[", ", ");

		memcpy(&cmsg, buf, sizeof(cmsg));

		put_open(proc, NULL, 0, "{", ", ");
		if (verbose > 0)
			put_value(proc, "cmsg_len", "%u", cmsg.cmsg_len);
		put_socket_level(proc, "cmsg_level", cmsg.cmsg_level);
		if (cmsg.cmsg_level == SOL_SOCKET)
			put_cmsg_type(proc, "cmsg_type", cmsg.cmsg_type);
		else
			put_value(proc, "cmsg_type", "%d", cmsg.cmsg_type);

		if (cmsg.cmsg_len < CMSG_LEN(0) || off + cmsg.cmsg_len > len) {
			put_tail(proc, 0, 0);
			put_close(proc, "}");
			break;
		}

		datalen = cmsg.cmsg_len - CMSG_LEN(0);

		if (cmsg.cmsg_level == SOL_SOCKET &&
		    cmsg.cmsg_type == SCM_RIGHTS) {
			put_cmsg_rights(proc, "cmsg_data", buf, sizeof(buf),
			    &buf[CMSG_LEN(0)], chunk - CMSG_LEN(0),
			    addr + off + chunk, datalen);
		} else if (cmsg.cmsg_level == SOL_SOCKET &&
		    cmsg.cmsg_type == SCM_CREDS &&
		    datalen >= sizeof(struct sockcred) &&
		    chunk >= CMSG_LEN(datalen)) {
			put_struct_sockcred(proc, "cmsg_data", PF_LOCADDR,
			    (vir_bytes)&buf[CMSG_LEN(0)],
			    datalen - sizeof(struct sockcred));
		} else if (datalen > 0)
			put_field(proc, "cmsg_data", "..");

		if (verbose == 0)
			put_field(proc, NULL, "..");
		put_close(proc, "}");
	}

	if (off < len)
		put_field(proc, NULL, "..");
	put_close(proc, "]");
}

static void
put_struct_msghdr(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, ssize_t max)
{
	struct msghdr msg;
	int all;

	if (!put_open_struct(proc, name, flags, addr, &msg, sizeof(msg)))
		return;

	all = TRUE;

	if (msg.msg_name != NULL || verbose > 1) {
		put_struct_sockaddr(proc, "msg_name", 0,
		    (vir_bytes)msg.msg_name, msg.msg_namelen);
		if (verbose > 0)
			put_value(proc, "msg_namelen", "%u", msg.msg_namelen);
		else
			all = FALSE;
	} else
		all = FALSE;

	put_struct_iovec(proc, "msg_iov", 0, (vir_bytes)msg.msg_iov,
	    msg.msg_iovlen, max);
	if (verbose > 0)
		put_value(proc, "msg_iovlen", "%d", msg.msg_iovlen);
	else
		all = FALSE;

	if (msg.msg_control != NULL || verbose > 1) {
		put_cmsg(proc, "msg_control", (vir_bytes)msg.msg_control,
		    msg.msg_controllen);

		if (verbose > 0)
			put_value(proc, "msg_controllen", "%u",
			    msg.msg_controllen);
		else
			all = FALSE;
	} else
		all = FALSE;

	/* When receiving, print the flags field as well. */
	if (flags & PF_ALT)
		put_flags(proc, "msg_flags", msg_flags, COUNT(msg_flags),
		    "0x%x", msg.msg_flags);

	put_close_struct(proc, all);
}

static int
vfs_sendmsg_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_sockmsg.fd);
	put_struct_msghdr(proc, "msg", 0, m_out->m_lc_vfs_sockmsg.msgbuf,
	    SSIZE_MAX);
	put_flags(proc, "flags", msg_flags, COUNT(msg_flags), "0x%x",
	    m_out->m_lc_vfs_sockmsg.flags);

	return CT_DONE;
}

static int
vfs_recvfrom_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_sendrecv.fd);

	return CT_NOTDONE;
}

static void
vfs_recvfrom_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	put_buf(proc, "buf", failed, m_out->m_lc_vfs_sendrecv.buf,
	    m_in->m_type);
	put_value(proc, "len", "%zu", m_out->m_lc_vfs_sendrecv.len);
	put_flags(proc, "flags", msg_flags, COUNT(msg_flags), "0x%x",
	    m_out->m_lc_vfs_sendrecv.flags);
	put_struct_sockaddr(proc, "addr", failed,
	    m_out->m_lc_vfs_sendrecv.addr, m_in->m_vfs_lc_socklen.len);
	if (m_out->m_lc_vfs_sendrecv.addr == 0)
		put_field(proc, "addr_len", "NULL");
	else if (!failed)
		put_value(proc, "addr_len", "{%u}",
		    m_in->m_vfs_lc_socklen.len);
	else
		put_field(proc, "addr_len", "&..");

	put_equals(proc);
	put_result(proc);
}

static int
vfs_recvmsg_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_sockmsg.fd);

	return CT_NOTDONE;
}

static void
vfs_recvmsg_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	/*
	 * We choose to print only the resulting structure in this case.  Doing
	 * so is easier and less messy than printing both the original and the
	 * result for the fields that are updated by the system (msg_namelen
	 * and msg_controllen); also, this approach is stateless.  Admittedly
	 * it is not entirely consistent with many other parts of the trace
	 * output, though.
	 */
	put_struct_msghdr(proc, "msg", PF_ALT | failed,
	    m_out->m_lc_vfs_sockmsg.msgbuf, m_in->m_type);
	put_flags(proc, "flags", msg_flags, COUNT(msg_flags), "0x%x",
	    m_out->m_lc_vfs_sockmsg.flags);

	put_equals(proc);
	put_result(proc);
}

static void
put_sockopt_name(struct trace_proc * proc, const char * name, int level,
	int optname)
{
	const char *text = NULL;

	/*
	 * The only level for which we can know names is SOL_SOCKET.  See also
	 * put_socket_level().  Of course we could guess, but then we need a
	 * proper guessing system, which should probably also take into account
	 * the [gs]etsockopt option length.  TODO.
	 */
	if (!valuesonly && level == SOL_SOCKET) {
		switch (optname) {
		TEXT(SO_DEBUG);
		TEXT(SO_ACCEPTCONN);
		TEXT(SO_REUSEADDR);
		TEXT(SO_KEEPALIVE);
		TEXT(SO_DONTROUTE);
		TEXT(SO_BROADCAST);
		TEXT(SO_USELOOPBACK);
		TEXT(SO_LINGER);
		TEXT(SO_OOBINLINE);
		TEXT(SO_REUSEPORT);
		TEXT(SO_NOSIGPIPE);
		TEXT(SO_TIMESTAMP);
		TEXT(SO_SNDBUF);
		TEXT(SO_RCVBUF);
		TEXT(SO_SNDLOWAT);
		TEXT(SO_RCVLOWAT);
		TEXT(SO_ERROR);
		TEXT(SO_TYPE);
		TEXT(SO_OVERFLOWED);
		TEXT(SO_NOHEADER);
		TEXT(SO_SNDTIMEO);
		TEXT(SO_RCVTIMEO);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "0x%x", optname);
}

static void
put_sockopt_data(struct trace_proc * proc, const char * name, int flags,
	int level, int optname, vir_bytes addr, socklen_t len)
{
	const char *text;
	int i;
	struct linger l;
	struct timeval tv;
	void *ptr;
	size_t size;

	/* See above regarding ambiguity for levels other than SOL_SOCKET. */
	if ((flags & PF_FAILED) || valuesonly > 1 || len == 0 ||
	    level != SOL_SOCKET) {
		put_ptr(proc, name, addr);

		return;
	}

	/* Determine how much data to get, and where to put it. */
	switch (optname) {
	case SO_DEBUG:
	case SO_ACCEPTCONN:
	case SO_REUSEADDR:
	case SO_KEEPALIVE:
	case SO_DONTROUTE:
	case SO_BROADCAST:
	case SO_USELOOPBACK:
	case SO_OOBINLINE:
	case SO_REUSEPORT:
	case SO_NOSIGPIPE:
	case SO_TIMESTAMP:
	case SO_SNDBUF:
	case SO_RCVBUF:
	case SO_SNDLOWAT:
	case SO_RCVLOWAT:
	case SO_ERROR:
	case SO_TYPE:
	case SO_OVERFLOWED:
	case SO_NOHEADER:
		ptr = &i;
		size = sizeof(i);
		break;
	case SO_LINGER:
		ptr = &l;
		size = sizeof(l);
		break;
	case SO_SNDTIMEO:
	case SO_RCVTIMEO:
		ptr = &tv;
		size = sizeof(tv);
		break;
	default:
		put_ptr(proc, name, addr);
		return;
	}

	/* Get the data.  Do not bother with truncated values. */
	if (len < size || mem_get_data(proc->pid, addr, ptr, size) < 0) {
		put_ptr(proc, name, addr);

		return;
	}

	/* Print the data according to the option name. */
	switch (optname) {
	case SO_LINGER:
		/* This isn't going to appear anywhere else; do it inline. */
		put_open(proc, name, 0, "{", ", ");
		put_value(proc, "l_onoff", "%d", l.l_onoff);
		put_value(proc, "l_linger", "%d", l.l_linger);
		put_close(proc, "}");
		break;
	case SO_ERROR:
		put_open(proc, name, 0, "{", ", ");
		if (!valuesonly && (text = get_error_name(i)) != NULL)
			put_field(proc, NULL, text);
		else
			put_value(proc, NULL, "%d", i);
		put_close(proc, "}");
		break;
	case SO_TYPE:
		put_open(proc, name, 0, "{", ", ");
		put_socket_type(proc, NULL, i);
		put_close(proc, "}");
		break;
	case SO_SNDTIMEO:
	case SO_RCVTIMEO:
		put_struct_timeval(proc, name, PF_LOCADDR, (vir_bytes)&tv);
		break;
	default:
		/* All other options are integer values. */
		put_value(proc, name, "{%d}", i);
	}
}

static int
vfs_setsockopt_out(struct trace_proc * proc, const message * m_out)
{
	int level, name;

	level = m_out->m_lc_vfs_sockopt.level;
	name = m_out->m_lc_vfs_sockopt.name;

	put_fd(proc, "fd", m_out->m_lc_vfs_sockopt.fd);
	put_socket_level(proc, "level", level);
	put_sockopt_name(proc, "name", level, name);
	put_sockopt_data(proc, "buf", 0, level, name,
	    m_out->m_lc_vfs_sockopt.buf, m_out->m_lc_vfs_sockopt.len);
	put_value(proc, "len", "%u", m_out->m_lc_vfs_sockopt.len);

	return CT_DONE;
}

static int
vfs_getsockopt_out(struct trace_proc * proc, const message * m_out)
{
	int level;

	level = m_out->m_lc_vfs_sockopt.level;

	put_fd(proc, "fd", m_out->m_lc_vfs_sockopt.fd);
	put_socket_level(proc, "level", level);
	put_sockopt_name(proc, "name", level, m_out->m_lc_vfs_sockopt.name);

	return CT_NOTDONE;
}

static void
vfs_getsockopt_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	put_sockopt_data(proc, "buf", failed, m_out->m_lc_vfs_sockopt.level,
	    m_out->m_lc_vfs_sockopt.name, m_out->m_lc_vfs_sockopt.buf,
	    m_in->m_vfs_lc_socklen.len);
	/*
	 * For the length, we follow the same scheme as for addr_len pointers
	 * in accept() et al., in that we print the result only.  We need not
	 * take into account that the given buffer is NULL as it must not be.
	 */
	if (!failed)
		put_value(proc, "len", "%u", m_out->m_lc_vfs_sockopt.len);
	else
		put_field(proc, "len", "&..");

	put_equals(proc);
	put_result(proc);
}

/* This function is shared between getsockname and getpeername. */
static int
vfs_getsockname_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_sockaddr.fd);

	return CT_NOTDONE;
}

static void
vfs_getsockname_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{

	put_struct_sockaddr(proc, "addr", failed,
	    m_out->m_lc_vfs_sockaddr.addr, m_in->m_vfs_lc_socklen.len);
	if (m_out->m_lc_vfs_sockaddr.addr == 0)
		put_field(proc, "addr_len", "NULL");
	else if (!failed)
		put_value(proc, "addr_len", "{%u}",
		    m_in->m_vfs_lc_socklen.len);
	else
		put_field(proc, "addr_len", "&..");

	put_equals(proc);
	put_result(proc);
}

void
put_shutdown_how(struct trace_proc * proc, const char * name, int how)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (how) {
		TEXT(SHUT_RD);
		TEXT(SHUT_WR);
		TEXT(SHUT_RDWR);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", how);
}

static int
vfs_shutdown_out(struct trace_proc * proc, const message * m_out)
{

	put_fd(proc, "fd", m_out->m_lc_vfs_shutdown.fd);
	put_shutdown_how(proc, "how", m_out->m_lc_vfs_shutdown.how);

	return CT_DONE;
}

#define VFS_CALL(c) [((VFS_ ## c) - VFS_BASE)]

static const struct call_handler vfs_map[] = {
	VFS_CALL(READ) = HANDLER("read", vfs_read_out, vfs_read_in),
	VFS_CALL(WRITE) = HANDLER("write", vfs_write_out, default_in),
	VFS_CALL(LSEEK) = HANDLER("lseek", vfs_lseek_out, vfs_lseek_in),
	VFS_CALL(OPEN) = HANDLER("open", vfs_open_out, vfs_open_in),
	VFS_CALL(CREAT) = HANDLER("open", vfs_creat_out, vfs_open_in),
	VFS_CALL(CLOSE) = HANDLER("close", vfs_close_out, default_in),
	VFS_CALL(LINK) = HANDLER("link", vfs_link_out, default_in),
	VFS_CALL(UNLINK) = HANDLER("unlink", vfs_path_out, default_in),
	VFS_CALL(CHDIR) = HANDLER("chdir", vfs_path_out, default_in),
	VFS_CALL(MKDIR) = HANDLER("mkdir", vfs_path_mode_out, default_in),
	VFS_CALL(MKNOD) = HANDLER("mknod", vfs_mknod_out, default_in),
	VFS_CALL(CHMOD) = HANDLER("chmod", vfs_path_mode_out, default_in),
	VFS_CALL(CHOWN) = HANDLER("chown", vfs_chown_out, default_in),
	VFS_CALL(MOUNT) = HANDLER("mount", vfs_mount_out, default_in),
	VFS_CALL(UMOUNT) = HANDLER("umount", vfs_umount_out, vfs_umount_in),
	VFS_CALL(ACCESS) = HANDLER("access", vfs_access_out, default_in),
	VFS_CALL(SYNC) = HANDLER("sync", default_out, default_in),
	VFS_CALL(RENAME) = HANDLER("rename", vfs_link_out, default_in),
	VFS_CALL(RMDIR) = HANDLER("rmdir", vfs_path_out, default_in),
	VFS_CALL(SYMLINK) = HANDLER("symlink", vfs_link_out, default_in),
	VFS_CALL(READLINK) = HANDLER("readlink", vfs_readlink_out,
	    vfs_readlink_in),
	VFS_CALL(STAT) = HANDLER("stat", vfs_stat_out, vfs_stat_in),
	VFS_CALL(FSTAT) = HANDLER("fstat", vfs_fstat_out, vfs_fstat_in),
	VFS_CALL(LSTAT) = HANDLER("lstat", vfs_stat_out, vfs_stat_in),
	VFS_CALL(IOCTL) = HANDLER("ioctl", vfs_ioctl_out, vfs_ioctl_in),
	VFS_CALL(FCNTL) = HANDLER("fcntl", vfs_fcntl_out, vfs_fcntl_in),
	VFS_CALL(PIPE2) = HANDLER("pipe2", vfs_pipe2_out, vfs_pipe2_in),
	VFS_CALL(UMASK) = HANDLER("umask", vfs_umask_out, vfs_umask_in),
	VFS_CALL(CHROOT) = HANDLER("chroot", vfs_path_out, default_in),
	VFS_CALL(GETDENTS) = HANDLER("getdents", vfs_getdents_out,
	    vfs_getdents_in),
	VFS_CALL(SELECT) = HANDLER("select", vfs_select_out, vfs_select_in),
	VFS_CALL(FCHDIR) = HANDLER("fchdir", vfs_fchdir_out, default_in),
	VFS_CALL(FSYNC) = HANDLER("fsync", vfs_fsync_out, default_in),
	VFS_CALL(TRUNCATE) = HANDLER("truncate", vfs_truncate_out, default_in),
	VFS_CALL(FTRUNCATE) = HANDLER("ftruncate", vfs_ftruncate_out,
	    default_in),
	VFS_CALL(FCHMOD) = HANDLER("fchmod", vfs_fchmod_out, default_in),
	VFS_CALL(FCHOWN) = HANDLER("fchown", vfs_fchown_out, default_in),
	VFS_CALL(UTIMENS) = HANDLER_NAME(vfs_utimens_name, vfs_utimens_out,
	    default_in),
	VFS_CALL(GETVFSSTAT) = HANDLER("getvfsstat", vfs_getvfsstat_out,
	    vfs_getvfsstat_in),
	VFS_CALL(STATVFS1) = HANDLER("statvfs1", vfs_statvfs1_out,
	    vfs_statvfs1_in),
	VFS_CALL(FSTATVFS1) = HANDLER("fstatvfs1", vfs_fstatvfs1_out,
	    vfs_statvfs1_in),
	VFS_CALL(SVRCTL) = HANDLER("vfs_svrctl", vfs_svrctl_out,
	    vfs_svrctl_in),
	VFS_CALL(GCOV_FLUSH) = HANDLER("gcov_flush", vfs_gcov_flush_out,
	    default_in),
	VFS_CALL(SOCKET) = HANDLER("socket", vfs_socket_out, default_in),
	VFS_CALL(SOCKETPAIR) = HANDLER("socketpair", vfs_socketpair_out,
	    vfs_socketpair_in),
	VFS_CALL(BIND) = HANDLER("bind", vfs_bind_out, default_in),
	VFS_CALL(CONNECT) = HANDLER("connect", vfs_bind_out, default_in),
	VFS_CALL(LISTEN) = HANDLER("listen", vfs_listen_out, default_in),
	VFS_CALL(ACCEPT) = HANDLER("accept", vfs_accept_out, vfs_accept_in),
	VFS_CALL(SENDTO) = HANDLER("sendto", vfs_sendto_out, default_in),
	VFS_CALL(SENDMSG) = HANDLER("sendmsg", vfs_sendmsg_out, default_in),
	VFS_CALL(RECVFROM) = HANDLER("recvfrom", vfs_recvfrom_out,
	    vfs_recvfrom_in),
	VFS_CALL(RECVMSG) = HANDLER("recvmsg", vfs_recvmsg_out,
	    vfs_recvmsg_in),
	VFS_CALL(SETSOCKOPT) = HANDLER("setsockopt", vfs_setsockopt_out,
	    default_in),
	VFS_CALL(GETSOCKOPT) = HANDLER("getsockopt", vfs_getsockopt_out,
	    vfs_getsockopt_in),
	VFS_CALL(GETSOCKNAME) = HANDLER("getsockname", vfs_getsockname_out,
	    vfs_getsockname_in),
	VFS_CALL(GETPEERNAME) = HANDLER("getpeername", vfs_getsockname_out,
	    vfs_getsockname_in),
	VFS_CALL(SHUTDOWN) = HANDLER("shutdown", vfs_shutdown_out, default_in),
};

const struct calls vfs_calls = {
	.endpt = VFS_PROC_NR,
	.base = VFS_BASE,
	.map = vfs_map,
	.count = COUNT(vfs_map)
};
