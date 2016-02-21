/* PFS - Pipe File Server */

#include <minix/drivers.h>
#include <minix/fsdriver.h>
#include <minix/vfsif.h>
#include <minix/rs.h>
#include <assert.h>

/*
 * The following constant defines the number of inodes in PFS, which is
 * therefore the maximum number of open pipes and cloned devices that can be
 * used in the entire system.  If anything, it should be kept somewhat in sync
 * with VFS's maximum number of inodes.  In the future, inodes could be
 * allocated dynamically, but this will require extra infrastructure.
 */
#define PFS_NR_INODES	512		/* maximum number of inodes in PFS */

/* The following bits can be combined in the inode's i_update field. */
#define ATIME		0x1		/* update access time later */
#define MTIME		0x2		/* update modification time later */
#define CTIME		0x4		/* update change time later */

static struct inode {
	ino_t i_num;			/* inode number */

	mode_t i_mode;			/* file mode and permissions */
	uid_t i_uid;			/* user ID of the file's owner */
	gid_t i_gid;			/* group ID of the file's owner */
	size_t i_size;			/* current file size in bytes */
	dev_t i_rdev;			/* device number for device nodes */
	time_t i_atime;			/* file access time */
	time_t i_mtime;			/* file modification time */
	time_t i_ctime;			/* file change time */

	char *i_data;			/* data buffer, for pipes only */
	size_t i_start;			/* start of data into data buffer */

	unsigned char i_update;		/* which file times to update? */
	unsigned char i_free;		/* sanity check: is the inode free? */

	LIST_ENTRY(inode) i_next;	/* next element in free list */
} inode[PFS_NR_INODES];

static LIST_HEAD(, inode) free_inodes;	/* list of free inodes */

/*
 * Mount the pipe file server.
 */
static int
pfs_mount(dev_t __unused dev, unsigned int __unused flags,
	struct fsdriver_node * node, unsigned int * res_flags)
{
	struct inode *rip;
	unsigned int i;

	LIST_INIT(&free_inodes);	/* initialize the free list */

	/*
	 * Initialize the inode table.  We walk backwards so that the lowest
	 * inode numbers end up being used first.  Silly?  Sure, but aesthetics
	 * are worth something, too..
	 */
	for (i = PFS_NR_INODES; i > 0; i--) {
		rip = &inode[i - 1];

		/* Inode number 0 is reserved.  See also pfs_findnode. */
		rip->i_num = i;
		rip->i_free = TRUE;

		LIST_INSERT_HEAD(&free_inodes, rip, i_next);
	}

	/*
	 * PFS has no root node, and VFS will ignore the returned node details
	 * anyway.  The whole idea is to provide symmetry with other file
	 * systems, thus keeping libfsdriver simple and free of special cases.
	 */
	memset(node, 0, sizeof(*node));
	*res_flags = RES_64BIT;

	return OK;
}

/*
 * Unmount the pipe file server.
 */
static void
pfs_unmount(void)
{
	unsigned int i;

	/* Warn about in-use inodes.  There's nothing else we can do. */
	for (i = 0; i < PFS_NR_INODES; i++)
		if (inode[i].i_free == FALSE)
			break;

	if (i < PFS_NR_INODES)
		printf("PFS: unmounting while busy!\n");
}

/*
 * Find the node with the corresponding inode number.  It must be in use.
 */
static struct inode *
pfs_findnode(ino_t ino_nr)
{
	struct inode *rip;

	/* Inode numbers are 1-based, because inode number 0 is reserved. */
	if (ino_nr < 1 || ino_nr > PFS_NR_INODES)
		return NULL;

	rip = &inode[ino_nr - 1];
	assert(rip->i_num == ino_nr);

	if (rip->i_free == TRUE)
		return NULL;

	return rip;
}

/*
 * Create a new, unlinked node.  It must be either a pipe or a device file.
 */
static int
pfs_newnode(mode_t mode, uid_t uid, gid_t gid, dev_t dev,
	struct fsdriver_node * node)
{
	struct inode *rip;
	char *data;
	int isfifo, isdev;

	/* Check the file type.  Do we support it at all? */
	isfifo = S_ISFIFO(mode);
	isdev = S_ISBLK(mode) || S_ISCHR(mode) || S_ISSOCK(mode);

	if (!isfifo && !isdev)
		return EINVAL;	/* this means VFS is misbehaving.. */

	/* Is there a free inode? */
	if (LIST_EMPTY(&free_inodes))
		return ENFILE;

	/* For pipes, we need a buffer.  Try to allocate one. */
	data = NULL;
	if (isfifo && (data = malloc(PIPE_BUF)) == NULL)
		return ENOSPC;

	/* Nothing can go wrong now.  Take an inode off the free list. */
	rip = LIST_FIRST(&free_inodes);
	LIST_REMOVE(rip, i_next);

	assert(rip->i_free == TRUE);
	rip->i_free = FALSE;	/* this is for sanity checks only */

	/* Initialize the inode's fields. */
	rip->i_mode = mode;
	rip->i_uid = uid;
	rip->i_gid = gid;
	rip->i_size = 0;
	rip->i_update = ATIME | MTIME | CTIME;
	if (isdev)
		rip->i_rdev = dev;
	else
		rip->i_rdev = NO_DEV;
	rip->i_data = data;
	rip->i_start = 0;

	/* Fill in the fields of the response message. */
	node->fn_ino_nr = rip->i_num;
	node->fn_mode = rip->i_mode;
	node->fn_size = rip->i_size;
	node->fn_uid = rip->i_uid;
	node->fn_gid = rip->i_gid;
	node->fn_dev = rip->i_rdev;

	return OK;
}

/*
 * Close a node.
 */
static int
pfs_putnode(ino_t ino_nr, unsigned int count)
{
	struct inode *rip;

	if ((rip = pfs_findnode(ino_nr)) == NULL)
		return EINVAL;

	/*
	 * Since the new-node call is the only way to open an inode, and there
	 * is no way to increase the use count of an already-opened inode, we
	 * can safely assume that the reference count will only ever be one.
	 * That also means we are always freeing up the target inode here.
	 */
	if (count != 1)
		return EINVAL;

	/* For pipes, free the inode data buffer. */
	if (rip->i_data != NULL) {
		free(rip->i_data);
		rip->i_data = NULL;
	}

	/* Return the inode to the free list. */
	rip->i_free = TRUE;

	LIST_INSERT_HEAD(&free_inodes, rip, i_next);

	return OK;
}

/*
 * Read from a pipe.
 */
static ssize_t
pfs_read(ino_t ino_nr, struct fsdriver_data * data, size_t bytes,
	off_t __unused pos, int __unused call)
{
	struct inode *rip;
	int r;

	/* The target node must be a pipe. */
	if ((rip = pfs_findnode(ino_nr)) == NULL || !S_ISFIFO(rip->i_mode))
		return EINVAL;

	/* We can't read beyond the maximum file position. */
	if (bytes > PIPE_BUF)
		return EFBIG;

	/* Limit the request to how much is in the pipe. */
	if (bytes > rip->i_size)
		bytes = rip->i_size;

	/* Copy the data to user space. */
	if ((r = fsdriver_copyout(data, 0, rip->i_data + rip->i_start,
	    bytes)) != OK)
		return r;

	/* Update file size and access time. */
	rip->i_size -= bytes;
	rip->i_start += bytes;
	rip->i_update |= ATIME;

	/* Return the number of bytes transferred. */
	return bytes;
}

/*
 * Write to a pipe.
 */
static ssize_t
pfs_write(ino_t ino_nr, struct fsdriver_data * data, size_t bytes,
	off_t __unused pos, int __unused call)
{
	struct inode *rip;
	int r;

	/* The target node must be a pipe. */
	if ((rip = pfs_findnode(ino_nr)) == NULL || !S_ISFIFO(rip->i_mode))
		return EINVAL;

	/* Check in advance to see if file will grow too big. */
	if (rip->i_size + bytes > PIPE_BUF)
		return EFBIG;

	/*
	 * Move any previously remaining data to the front of the buffer.
	 * Doing so upon writes rather than reads saves on memory moves when
	 * there are many small reads.  Not using the buffer circularly saves
	 * on kernel calls.
	 */
	if (rip->i_start > 0) {
		if (rip->i_size > 0)
			memmove(rip->i_data, rip->i_data + rip->i_start,
			    rip->i_size);

		rip->i_start = 0;
	}

	/* Copy the data from user space. */
	r = fsdriver_copyin(data, 0, rip->i_data + rip->i_size, bytes);
	if (r != OK)
		return r;

	/* Update file size and times. */
	rip->i_size += bytes;
	rip->i_update |= CTIME | MTIME;

	/* Return the number of bytes transferred. */
	return bytes;
}

/*
 * Truncate a pipe.
 */
static int
pfs_trunc(ino_t ino_nr, off_t start_pos, off_t end_pos)
{
	struct inode *rip;

	/* The target node must be a pipe. */
	if ((rip = pfs_findnode(ino_nr)) == NULL || !S_ISFIFO(rip->i_mode))
		return EINVAL;

	/* We only support full truncation of pipes. */
	if (start_pos != 0 || end_pos != 0)
		return EINVAL;

	/* Update file size and times. */
	rip->i_size = 0;
	rip->i_update |= CTIME | MTIME;

	return OK;
}

/*
 * Return node status.
 */
static int
pfs_stat(ino_t ino_nr, struct stat * statbuf)
{
	struct inode *rip;
	time_t now;

	if ((rip = pfs_findnode(ino_nr)) == NULL)
		return EINVAL;

	/* Update the time fields in the inode, if need be. */
	if (rip->i_update != 0) {
		now = clock_time(NULL);

		if (rip->i_update & ATIME) rip->i_atime = now;
		if (rip->i_update & MTIME) rip->i_mtime = now;
		if (rip->i_update & CTIME) rip->i_ctime = now;

		rip->i_update = 0;
	}

	/* Fill the stat buffer. */
	statbuf->st_dev = rip->i_rdev;	/* workaround for old socketpair bug */
	statbuf->st_mode = rip->i_mode;
	statbuf->st_nlink = 0;
	statbuf->st_uid = rip->i_uid;
	statbuf->st_gid = rip->i_gid;
	statbuf->st_rdev = rip->i_rdev;
	statbuf->st_size = rip->i_size;
	statbuf->st_atime = rip->i_atime;
	statbuf->st_mtime = rip->i_mtime;
	statbuf->st_ctime = rip->i_ctime;
	statbuf->st_blksize = PIPE_BUF;
	statbuf->st_blocks = howmany(rip->i_size, S_BLKSIZE);

	return OK;
}

/*
 * Change node permissions.
 */
static int
pfs_chmod(ino_t ino_nr, mode_t * mode)
{
	struct inode *rip;

	if ((rip = pfs_findnode(ino_nr)) == NULL)
		return EINVAL;

	/* Update file mode and times. */
	rip->i_mode = (rip->i_mode & ~ALLPERMS) | (*mode & ALLPERMS);
	rip->i_update |= MTIME | CTIME;

	*mode = rip->i_mode;
	return OK;
}

/*
 * Process a signal.
 */
static void
pfs_signal(int signo)
{

	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM) return;

	fsdriver_terminate();
}

/*
 * Initialize PFS.
 */
static int
pfs_init(int __unused type, sef_init_info_t * __unused info)
{

	/* Drop privileges. */
	if (setuid(SERVICE_UID) != 0)
		printf("PFS: warning, unable to drop privileges\n");

	return OK;
}

/*
 * Perform SEF initialization.
 */
static void
pfs_startup(void)
{

	/* Register initialization callbacks. */
	sef_setcb_init_fresh(pfs_init);
	sef_setcb_init_restart(SEF_CB_INIT_RESTART_STATEFUL);

	/* Register signal callbacks. */
	sef_setcb_signal_handler(pfs_signal);

	/* Let SEF perform startup. */
	sef_startup();
}

/*
 * Function call table for the fsdriver library.
 */
static struct fsdriver pfs_table = {
	.fdr_mount	= pfs_mount,
	.fdr_unmount	= pfs_unmount,
	.fdr_newnode	= pfs_newnode,
	.fdr_putnode	= pfs_putnode,
	.fdr_read	= pfs_read,
	.fdr_write	= pfs_write,
	.fdr_trunc	= pfs_trunc,
	.fdr_stat	= pfs_stat,
	.fdr_chmod	= pfs_chmod
};

/*
 * The main routine of this service.
 */
int
main(void)
{

	/* Local startup. */
	pfs_startup();

	/* The fsdriver library does the actual work here. */
	fsdriver_task(&pfs_table);

	return EXIT_SUCCESS;
}
