/* ProcFS - service.c - the service subdirectory */

#include "inc.h"

#include <minix/rs.h>
#include "rs/const.h"
#include "rs/type.h"

static struct rprocpub rprocpub[NR_SYS_PROCS];
static struct rproc rproc[NR_SYS_PROCS];

static struct inode *service_node;

/*
 * Initialize the service directory.
 */
void
service_init(void)
{
	struct inode *root, *node;
	struct inode_stat stat;

	root = get_root_inode();

	memset(&stat, 0, sizeof(stat));
	stat.mode = DIR_ALL_MODE;
	stat.uid = SUPER_USER;
	stat.gid = SUPER_USER;

	service_node = add_inode(root, "service", NO_INDEX, &stat,
	    NR_SYS_PROCS, NULL);

	if (service_node == NULL)
		panic("unable to create service node");
}

/*
 * Update the contents of the service directory, by first updating the RS
 * tables and then updating the directory contents.
 */
void
service_update(void)
{
	struct inode *node;
	struct inode_stat stat;
	index_t slot;

	/* There is not much we can do if either of these calls fails. */
	(void)getsysinfo(RS_PROC_NR, SI_PROCPUB_TAB, rprocpub,
	    sizeof(rprocpub));
	(void)getsysinfo(RS_PROC_NR, SI_PROC_TAB, rproc, sizeof(rproc));

	/*
	 * As with PIDs, we make two passes.  Delete first, then add.  This
	 * prevents problems in the hypothetical case that between updates, one
	 * slot ends up with the label name of a previous, different slot.
	 */
	for (slot = 0; slot < NR_SYS_PROCS; slot++) {
		if ((node = get_inode_by_index(service_node, slot)) == NULL)
			continue;

		/*
		 * If the slot is no longer in use, or the label name does not
		 * match, the node must be deleted.
		 */
		if (!(rproc[slot].r_flags & RS_IN_USE) ||
		    strcmp(get_inode_name(node), rprocpub[slot].label))
			delete_inode(node);
	}

	memset(&stat, 0, sizeof(stat));
	stat.mode = REG_ALL_MODE;
	stat.uid = SUPER_USER;
	stat.gid = SUPER_USER;

	for (slot = 0; slot < NR_SYS_PROCS; slot++) {
		if (!(rproc[slot].r_flags & RS_IN_USE) ||
		    get_inode_by_index(service_node, slot) != NULL)
			continue;

		node = add_inode(service_node, rprocpub[slot].label, slot,
		    &stat, (index_t)0, (cbdata_t)slot);

		if (node == NULL)
			out_of_inodes();
	}
}

/*
 * A lookup request is being performed.  If it is in the service directory,
 * update the tables.  We do this lazily, to reduce overhead.
 */
void
service_lookup(struct inode * parent, clock_t now)
{
	static clock_t last_update = 0;

	if (parent != service_node)
		return;

	if (last_update != now) {
		service_update();

		last_update = now;
	}
}

/*
 * A getdents request is being performed.  If it is in the service directory,
 * update the tables.
 */
void
service_getdents(struct inode * node)
{

	if (node != service_node)
		return;

	service_update();
}

/*
 * A read request is being performed.  If it is on a file in the service
 * directory, process the read request.  We rely on the fact that any read
 * call will have been preceded by a lookup, so its table entry has been
 * updated very recently.
 */
void
service_read(struct inode * node)
{
	struct inode *parent;
	index_t slot;
	struct rprocpub *rpub;
	struct rproc *rp;

	if (get_parent_inode(node) != service_node)
		return;

	slot = get_inode_index(node);
	rpub = &rprocpub[slot];
	rp = &rproc[slot];

	/* TODO: add a large number of other fields! */
	buf_printf("%d %d\n", rpub->endpoint, rp->r_restarts);
}
