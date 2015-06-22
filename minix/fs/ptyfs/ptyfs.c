/* PTYFS - file system for Unix98 pseudoterminal slave nodes (/dev/pts) */

#include <minix/drivers.h>
#include <minix/fsdriver.h>
#include <minix/vfsif.h>
#include <minix/ds.h>
#include <sys/dirent.h>
#include <assert.h>

#include "node.h"

#define ROOT_INO_NR	1	/* inode number of the root directory */
#define BASE_INO_NR	2	/* first inode number for slave nodes */

#define GETDENTS_BUF	1024	/* size of the temporary buffer for getdents */

static struct node_data root_data = {
	.mode	= S_IFDIR | 0755,
	.uid	= 0,
	.gid	= 0,
	.dev	= NO_DEV
};

/*
 * Mount the file system.
 */
static int
ptyfs_mount(dev_t __unused dev, unsigned int flags,
	struct fsdriver_node * root_node, unsigned int * res_flags)
{

	/* This file system can not be used as a root file system. */
	if (flags & REQ_ISROOT)
		return EINVAL;

	/* Return the details of the root node. */
	root_node->fn_ino_nr = ROOT_INO_NR;
	root_node->fn_mode = root_data.mode;
	root_node->fn_uid = root_data.uid;
	root_node->fn_gid = root_data.gid;
	root_node->fn_size = 0;
	root_node->fn_dev = root_data.dev;

	*res_flags = RES_NOFLAGS;

	return OK;
}

/*
 * Generate the name string of a slave node based on its node number.  Return
 * OK on success, with the null-terminated name stored in the buffer 'name'
 * which is 'size' bytes in size.  Return an error code on failure.
 */
static int
make_name(char * name, size_t size, node_t index)
{
	ssize_t r;

	if ((r = snprintf(name, sizeof(name), "%u", index)) < 0)
		return EINVAL;

	if (r >= size)
		return ENAMETOOLONG;

	return OK;
}

/*
 * Parse the name of a slave node as given by a user, and check whether it is a
 * valid slave node number.  A valid slave number is any name that can be
 * produced by make_name().  Return TRUE if the string was successfully parsed
 * as a slave node number (which may or may not actually be allocated), with
 * the number stored in 'indexp'.  Return FALSE if the name is not a number.
 */
static int
parse_name(const char * name, node_t * indexp)
{
	node_t index;
	const char *p;

	index = 0;
	for (p = name; *p; p++) {
		/* Digits only. */
		if (*p < '0' || *p > '9')
			return FALSE;

		/* No leading zeroes. */
		if (p != name && index == 0)
			return FALSE;

		/* No overflow. */
		if (index * 10 < index)
			return FALSE;

		index = index * 10 + *p - '0';
	}

	*indexp = index;
	return TRUE;
}

/*
 * Look up a name in a directory, yielding a node on success.  For a successful
 * lookup, the given name must either be a single dot, which resolves to the
 * file system root directory, or the number of an allocated slave node.
 */
static int
ptyfs_lookup(ino_t dir_nr, char * name, struct fsdriver_node * node,
	int * is_mountpt)
{
	struct node_data *data;
	node_t index;
	ino_t ino_nr;

	assert(name[0] != '\0');

	if (dir_nr != ROOT_INO_NR)
		return ENOENT;

	if (name[0] == '.' && name[1] == '\0') {
		/* The root directory itself is requested. */
		ino_nr = ROOT_INO_NR;

		data = &root_data;
	} else {
		/* Parse the user-provided name, which must be a number. */
		if (!parse_name(name, &index))
			return ENOENT;

		ino_nr = BASE_INO_NR + index;

		/* See if the number is in use, and get its details. */
		if ((data = get_node(index)) == NULL)
			return ENOENT;
	}

	node->fn_ino_nr = ino_nr;
	node->fn_mode = data->mode;
	node->fn_uid = data->uid;
	node->fn_gid = data->gid;
	node->fn_size = 0;
	node->fn_dev = data->dev;

	*is_mountpt = FALSE;

	return OK;
}

/*
 * Enumerate directory contents.
 */
static ssize_t
ptyfs_getdents(ino_t ino_nr, struct fsdriver_data * data,
	size_t bytes, off_t * posp)
{
	struct fsdriver_dentry fsdentry;
	static char buf[GETDENTS_BUF];
	char name[NAME_MAX + 1];
	struct node_data *node_data;
	unsigned int type;
	off_t pos;
	node_t index;
	ssize_t r;

	if (ino_nr != ROOT_INO_NR)
		return EINVAL;

	fsdriver_dentry_init(&fsdentry, data, bytes, buf, sizeof(buf));

	for (;;) {
		pos = (*posp)++;

		if (pos < 2) {
			strlcpy(name, (pos == 0) ? "." : "..", sizeof(name));
			ino_nr = ROOT_INO_NR;
			type = DT_DIR;
		} else {
			if (pos - 2 >= get_max_node())
				break; /* EOF */
			index = (node_t)(pos - 2);

			if ((node_data = get_node(index)) == NULL)
				continue; /* index not in use */

			if (make_name(name, sizeof(name), index) != OK)
				continue; /* could not generate name string */
			ino_nr = BASE_INO_NR + index;
			type = IFTODT(node_data->mode);
		}

		if ((r = fsdriver_dentry_add(&fsdentry, ino_nr, name,
		    strlen(name), type)) < 0)
			return r;
		if (r == 0)
			break; /* result buffer full */
	}

	return fsdriver_dentry_finish(&fsdentry);
}

/*
 * Return a pointer to the node data structure for the given inode number, or
 * NULL if no node exists for the given inode number.
 */
static struct node_data *
get_data(ino_t ino_nr)
{
	node_t index;

	if (ino_nr == ROOT_INO_NR)
		return &root_data;

	if (ino_nr < BASE_INO_NR || ino_nr >= BASE_INO_NR + get_max_node())
		return NULL;

	index = (node_t)(ino_nr - BASE_INO_NR);

	return get_node(index);
}

/*
 * Change file ownership.
 */
static int
ptyfs_chown(ino_t ino_nr, uid_t uid, gid_t gid, mode_t * mode)
{
	struct node_data *data;

	if ((data = get_data(ino_nr)) == NULL)
		return EINVAL;

	data->uid = uid;
	data->gid = gid;
	data->mode &= ~(S_ISUID | S_ISGID);

	*mode = data->mode;

	return OK;
}

/*
 * Change file mode.
 */
static int
ptyfs_chmod(ino_t ino_nr, mode_t * mode)
{
	struct node_data *data;

	if ((data = get_data(ino_nr)) == NULL)
		return EINVAL;

	data->mode = (data->mode & ~ALLPERMS) | (*mode & ALLPERMS);

	*mode = data->mode;

	return OK;
}

/*
 * Return node details.
 */
static int
ptyfs_stat(ino_t ino_nr, struct stat * buf)
{
	struct node_data *data;

	if ((data = get_data(ino_nr)) == NULL)
		return EINVAL;

	buf->st_mode = data->mode;
	buf->st_uid = data->uid;
	buf->st_gid = data->gid;
	buf->st_nlink = S_ISDIR(data->mode) ? 2 : 1;
	buf->st_rdev = data->dev;
	buf->st_atime = data->ctime;
	buf->st_mtime = data->ctime;
	buf->st_ctime = data->ctime;

	return OK;
}

/*
 * Return file system statistics.
 */
static int
ptyfs_statvfs(struct statvfs * buf)
{

	buf->f_flag = ST_NOTRUNC;
	buf->f_namemax = NAME_MAX;

	return OK;
}

/*
 * Process non-filesystem messages, in particular slave node creation and
 * deletion requests from the PTY service.
 */
static void
ptyfs_other(const message * m_ptr, int ipc_status)
{
	char label[DS_MAX_KEYLEN];
	struct node_data data;
	message m_reply;
	int r;

	/*
	 * We only accept requests from the service with the label "pty".
	 * More sophisticated access checks are part of future work.
	 */
	if ((r = ds_retrieve_label_name(label, m_ptr->m_source)) != OK) {
		printf("PTYFS: unable to obtain label for %u (%d)\n",
		    m_ptr->m_source, r);
		return;
	}

	if (strcmp(label, "pty")) {
		printf("PTYFS: unexpected request %x from %s/%u\n",
		    m_ptr->m_type, label, m_ptr->m_source);
		return;
	}

	/* Process the request from PTY. */
	memset(&m_reply, 0, sizeof(m_reply));

	switch (m_ptr->m_type) {
	case PTYFS_SET:
		memset(&data, 0, sizeof(data));
		data.dev = m_ptr->m_pty_ptyfs_req.dev;
		data.mode = m_ptr->m_pty_ptyfs_req.mode;
		data.uid = m_ptr->m_pty_ptyfs_req.uid;
		data.gid = m_ptr->m_pty_ptyfs_req.gid;
		data.ctime = clock_time(NULL);

		r = set_node(m_ptr->m_pty_ptyfs_req.index, &data);

		break;

	case PTYFS_CLEAR:
		clear_node(m_ptr->m_pty_ptyfs_req.index);
		r = OK;

		break;

	case PTYFS_NAME:
		r = make_name(m_reply.m_ptyfs_pty_name.name,
		    sizeof(m_reply.m_ptyfs_pty_name.name),
		    m_ptr->m_pty_ptyfs_req.index);

		break;

	default:
		printf("PTYFS: invalid request %x from PTY\n", m_ptr->m_type);
		r = ENOSYS;
	}

	/*
	 * Send a reply to the request.  In particular slave node addition
	 * requests must be blocking for the PTY service, so as to avoid race
	 * conditions between PTYFS creating the slave node and userland trying
	 * to open it.
	 */
	m_reply.m_type = r;

	if (IPC_STATUS_CALL(ipc_status) == SENDREC)
		r = ipc_sendnb(m_ptr->m_source, &m_reply);
	else
		r = asynsend3(m_ptr->m_source, &m_reply, AMF_NOREPLY);

	if (r != OK)
		printf("PTYFS: unable to reply to PTY (%d)\n", r);
}

/*
 * Initialize the service.
 */
static int
ptyfs_init(int __unused type, sef_init_info_t * __unused info)
{

	init_nodes();

	root_data.ctime = clock_time(NULL);

	return OK;
}

/*
 * Process an incoming signal.
 */
static void
ptyfs_signal(int sig)
{

	if (sig == SIGTERM)
		fsdriver_terminate();
}

/*
 * Perform SEF initialization.
 */
static void
ptyfs_startup(void)
{

	sef_setcb_init_fresh(ptyfs_init);
	sef_setcb_signal_handler(ptyfs_signal);
	sef_startup();
}

static struct fsdriver ptyfs_table = {
	.fdr_mount	= ptyfs_mount,
	.fdr_lookup	= ptyfs_lookup,
	.fdr_getdents	= ptyfs_getdents,
	.fdr_stat	= ptyfs_stat,
	.fdr_chown	= ptyfs_chown,
	.fdr_chmod	= ptyfs_chmod,
	.fdr_statvfs	= ptyfs_statvfs,
	.fdr_other	= ptyfs_other
};

/*
 * The PTYFS service.
 */
int
main(void)
{

	ptyfs_startup();

	fsdriver_task(&ptyfs_table);

	return 0;
}
