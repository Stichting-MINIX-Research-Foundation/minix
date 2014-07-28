/* ProcFS - tree.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

struct proc proc[NR_PROCS + NR_TASKS];
struct mproc mproc[NR_PROCS];
struct fproc fproc[NR_PROCS];

static int nr_pid_entries;

/*===========================================================================*
 *				slot_in_use				     *
 *===========================================================================*/
static int slot_in_use(int slot)
{
	/* Return whether the given slot is in use by a process.
	 */

	/* For kernel tasks, check only the kernel slot. Tasks do not have a
	 * PM/VFS process slot.
	 */
	if (slot < NR_TASKS)
		return (proc[slot].p_rts_flags != RTS_SLOT_FREE);

	/* For regular processes, check only the PM slot. Do not check the
	 * kernel slot, because that would skip zombie processes. The PID check
	 * should be redundant, but if it fails, procfs could crash.
	 */
	return ((mproc[slot - NR_TASKS].mp_flags & IN_USE) &&
		mproc[slot - NR_TASKS].mp_pid != 0);
}

/*===========================================================================*
 *				check_owner				     *
 *===========================================================================*/
static int check_owner(struct inode *node, int slot)
{
	/* Check if the owner user and group ID of the inode are still in sync
	 * the current effective user and group ID of the given process.
	 */
	struct inode_stat stat;

	if (slot < NR_TASKS) return TRUE;

	get_inode_stat(node, &stat);

	return (stat.uid == mproc[slot - NR_TASKS].mp_effuid &&
		stat.gid == mproc[slot - NR_TASKS].mp_effgid);
}

/*===========================================================================*
 *				make_stat				     *
 *===========================================================================*/
static void make_stat(struct inode_stat *stat, int slot, int index)
{
	/* Fill in an inode_stat structure for the given process slot and
	 * per-pid file index (or NO_INDEX for the process subdirectory root).
	 */

	if (index == NO_INDEX)
		stat->mode = DIR_ALL_MODE;
	else
		stat->mode = pid_files[index].mode;

	if (slot < NR_TASKS) {
		stat->uid = SUPER_USER;
		stat->gid = SUPER_USER;
	} else {
		stat->uid = mproc[slot - NR_TASKS].mp_effuid;
		stat->gid = mproc[slot - NR_TASKS].mp_effgid;
	}

	stat->size = 0;
	stat->dev = NO_DEV;
}

/*===========================================================================*
 *				dir_is_pid				     *
 *===========================================================================*/
static int dir_is_pid(struct inode *node)
{
	/* Return whether the given node is a PID directory.
	 */

	return (get_parent_inode(node) == get_root_inode() &&
		get_inode_index(node) != NO_INDEX);
}

/*===========================================================================*
 *				update_proc_table			     *
 *===========================================================================*/
static int update_proc_table(void)
{
	/* Get the process table from the kernel.
	 * Check the magic number in the table entries.
	 */
	int r, slot;

	if ((r = sys_getproctab(proc)) != OK) return r;

	for (slot = 0; slot < NR_PROCS + NR_TASKS; slot++) {
		if (proc[slot].p_magic != PMAGIC) {
			printf("PROCFS: system version mismatch!\n");

			return EINVAL;
		}
	}

	return OK;
}

/*===========================================================================*
 *				update_mproc_table			     *
 *===========================================================================*/
static int update_mproc_table(void)
{
	/* Get the process table from PM.
	 * Check the magic number in the table entries.
	 */
	int r, slot;

	r = getsysinfo(PM_PROC_NR, SI_PROC_TAB, mproc, sizeof(mproc));
	if (r != OK) return r;

	for (slot = 0; slot < NR_PROCS; slot++) {
		if (mproc[slot].mp_magic != MP_MAGIC) {
			printf("PROCFS: PM version mismatch!\n");

			return EINVAL;
		}
	}

	return OK;
}

/*===========================================================================*
 *				update_fproc_table			     *
 *===========================================================================*/
static int update_fproc_table(void)
{
	/* Get the process table from VFS.
	 */

	return getsysinfo(VFS_PROC_NR, SI_PROC_TAB, fproc, sizeof(fproc));
}

/*===========================================================================*
 *				update_tables				     *
 *===========================================================================*/
static int update_tables(void)
{
	/* Get the process tables from the kernel, PM, and VFS.
	 */
	int r;

	if ((r = update_proc_table()) != OK) return r;

	if ((r = update_mproc_table()) != OK) return r;

	if ((r = update_fproc_table()) != OK) return r;

	return OK;
}

/*===========================================================================*
 *				init_tree				     *
 *===========================================================================*/
int init_tree(void)
{
	/* Initialize this module, before VTreeFS is started. As part of the
	 * process, check if we're not compiled against a kernel different from
	 * the one that is running at the moment.
	 */
	int i, r;

	if ((r = update_tables()) != OK)
		return r;

	/* Get the maximum number of entries that we may add to each PID's
	 * directory. We could just default to a large value, but why not get
	 * it right?
	 */
	for (i = 0; pid_files[i].name != NULL; i++);

	nr_pid_entries = i;

	return OK;
}

/*===========================================================================*
 *				out_of_inodes				     *
 *===========================================================================*/
static void out_of_inodes(void)
{
	/* Out of inodes - the NR_INODES value is set too low. We can not do
	 * much, but we might be able to continue with degraded functionality,
	 * so do not panic. If the NR_INODES value is not below the *crucial*
	 * minimum, the symptom of this case will be an incomplete listing of
	 * the main proc directory.
	 */
	static int warned = FALSE;

	if (warned == FALSE) {
		printf("PROCFS: out of inodes!\n");

		warned = TRUE;
	}
}

/*===========================================================================*
 *				construct_pid_dirs			     *
 *===========================================================================*/
static void construct_pid_dirs(void)
{
	/* Regenerate the set of PID directories in the root directory of the
	 * file system. Add new directories and delete old directories as
	 * appropriate; leave unchanged those that should remain the same.
	 *
	 * We have to make two passes. Otherwise, we would trigger a vtreefs
	 * assert when we add an entry for a PID before deleting the previous
	 * entry for that PID. While rare, such rapid PID reuse does occur in
	 * practice.
	 */
	struct inode *root, *node;
	struct inode_stat stat;
	char name[PNAME_MAX+1];
	pid_t pid;
	int i;

	root = get_root_inode();

	/* First pass: delete old entries. */
	for (i = 0; i < NR_PROCS + NR_TASKS; i++) {
		/* Do we already have an inode associated with this slot? */
		node = get_inode_by_index(root, i);
		if (node == NULL)
			continue;

		/* If the process slot is not in use, delete the associated
		 * inode.
		 */
		if (!slot_in_use(i)) {
			delete_inode(node);

			continue;
		}

		/* Otherwise, get the process ID. */
		if (i < NR_TASKS)
			pid = (pid_t) (i - NR_TASKS);
		else
			pid = mproc[i - NR_TASKS].mp_pid;

		/* If there is an old entry, see if the pid matches the current
		 * entry, and the owner is still the same. Otherwise, delete
		 * the old entry first. We reconstruct the entire subtree even
		 * if only the owner changed, for security reasons: if a
		 * process could keep open a file or directory across the owner
		 * change, it might be able to access information it shouldn't.
		 */
		if (pid != (pid_t) get_inode_cbdata(node) ||
				!check_owner(node, i))
			delete_inode(node);
	}

	/* Second pass: add new entries. */
	for (i = 0; i < NR_PROCS + NR_TASKS; i++) {
		/* If the process slot is not in use, skip this slot. */
		if (!slot_in_use(i))
			continue;

		/* If we have an inode associated with this slot, we have
		 * already checked it to be up-to-date above.
		 */
		if (get_inode_by_index(root, i) != NULL)
			continue;

		/* Get the process ID. */
		if (i < NR_TASKS)
			pid = (pid_t) (i - NR_TASKS);
		else
			pid = mproc[i - NR_TASKS].mp_pid;

		/* Add the entry for the process slot. */
		snprintf(name, PNAME_MAX + 1, "%d", pid);

		make_stat(&stat, i, NO_INDEX);

		node = add_inode(root, name, i, &stat, nr_pid_entries,
			(cbdata_t) pid);

		if (node == NULL)
			out_of_inodes();
	}
}

/*===========================================================================*
 *				make_one_pid_entry			     *
 *===========================================================================*/
static void make_one_pid_entry(struct inode *parent, char *name, int slot)
{
	/* Construct one file in a PID directory, if a file with the given name
	 * should exist at all.
	 */
	struct inode *node;
	struct inode_stat stat;
	int i;

	/* Don't readd if it is already there. */
	node = get_inode_by_name(parent, name);
	if (node != NULL)
		return;

	/* Only add the file if it is a known, registered name. */
	for (i = 0; pid_files[i].name != NULL; i++) {
		if (!strcmp(name, pid_files[i].name)) {
			make_stat(&stat, slot, i);

			node = add_inode(parent, name, i, &stat,
				(index_t) 0, (cbdata_t) 0);

			if (node == NULL)
				out_of_inodes();

			break;
		}
	}
}

/*===========================================================================*
 *				make_all_pid_entries			     *
 *===========================================================================*/
static void make_all_pid_entries(struct inode *parent, int slot)
{
	/* Construct all files in a PID directory.
	 */
	struct inode *node;
	struct inode_stat stat;
	int i;

	for (i = 0; pid_files[i].name != NULL; i++) {
		node = get_inode_by_index(parent, i);
		if (node != NULL)
			continue;

		make_stat(&stat, slot, i);

		node = add_inode(parent, pid_files[i].name, i, &stat,
			(index_t) 0, (cbdata_t) 0);

		if (node == NULL)
			out_of_inodes();
	}
}

/*===========================================================================*
 *				construct_pid_entries			     *
 *===========================================================================*/
static void construct_pid_entries(struct inode *parent, char *name)
{
	/* Construct one requested file entry, or all file entries, in a PID
	 * directory.
	 */
	int slot;

	slot = get_inode_index(parent);
	assert(slot >= 0 && slot < NR_TASKS + NR_PROCS);

	/* If this process is already gone, delete the directory now. */
	if (!slot_in_use(slot)) {
		delete_inode(parent);

		return;
	}

	/* If a specific file name is being looked up, see if we have to add
	 * an inode for that file. If the directory contents are being
	 * retrieved, add all files that have not yet been added.
	 */
	if (name != NULL)
		make_one_pid_entry(parent, name, slot);
	else
		make_all_pid_entries(parent, slot);
}

/*===========================================================================*
 *				pid_read				     *
 *===========================================================================*/
static void pid_read(struct inode *node)
{
	/* Data is requested from one of the files in a PID directory. Call the
	 * function that is responsible for generating the data for that file.
	 */
	struct inode *parent;
	int slot, index;

	/* Get the slot number of the process. Note that this currently will
	 * not work for files not in the top-level pid subdirectory.
	 */
	parent = get_parent_inode(node);

	slot = get_inode_index(parent);

	/* Get this file's index number. */
	index = get_inode_index(node);

	/* Call the handler procedure for the file. */
	((void (*) (int)) pid_files[index].data)(slot);
}

/*===========================================================================*
 *				pid_link				     *
 *===========================================================================*/
static int pid_link(struct inode *UNUSED(node), char *ptr, int max)
{
	/* The contents of a symbolic link in a PID directory are requested.
	 * This function is a placeholder for future use.
	 */

	/* Nothing yet. */
	strlcpy(ptr, "", max);

	return OK;
}

/*===========================================================================*
 *				lookup_hook				     *
 *===========================================================================*/
int lookup_hook(struct inode *parent, char *name,
	cbdata_t UNUSED(cbdata))
{
	/* Path name resolution hook, for a specific parent and name pair.
	 * If needed, update our own view of the system first; after that,
	 * determine whether we need to (re)generate certain files.
	 */
	static clock_t last_update = 0;
	clock_t now;
	int r;

	/* Update lazily for lookups, as this gets too expensive otherwise.
	 * Alternative: pull in only PM's table?
	 */
	if ((r = getticks(&now)) != OK)
		panic("unable to get uptime: %d", r);

	if (last_update != now) {
		update_tables();

		last_update = now;
	}

	/* If the parent is the root directory, we must now reconstruct all
	 * entries, because some of them might have been garbage collected.
	 * We must update the entire tree at once; if we update individual
	 * entries, we risk name collisions.
	 */
	if (parent == get_root_inode()) {
		construct_pid_dirs();
	}
	/* If the parent is a process directory, we may need to (re)construct
	 * the entry being looked up.
	 */
	else if (dir_is_pid(parent)) {
		/* We might now have deleted our current containing directory;
		 * construct_pid_entries() will take care of this case.
		 */
		construct_pid_entries(parent, name);
	}

	return OK;
}

/*===========================================================================*
 *				getdents_hook				     *
 *===========================================================================*/
int getdents_hook(struct inode *node, cbdata_t UNUSED(cbdata))
{
	/* Directory entry retrieval hook, for potentially all files in a
	 * directory. Make sure that all files that are supposed to be
	 * returned, are actually part of the virtual tree.
	 */

	if (node == get_root_inode()) {
		update_tables();

		construct_pid_dirs();
	} else if (dir_is_pid(node)) {
		construct_pid_entries(node, NULL /*name*/);
	}

	return OK;
}

/*===========================================================================*
 *				read_hook				     *
 *===========================================================================*/
int read_hook(struct inode *node, off_t off, char **ptr,
	size_t *len, cbdata_t cbdata)
{
	/* Regular file read hook. Call the appropriate callback function to
	 * generate and return the data.
	 */

	buf_init(off, *len);

	/* Populate the buffer with the proper content. */
	if (get_inode_index(node) != NO_INDEX) {
		pid_read(node);
	} else {
		((void (*) (void)) cbdata)();
	}

	*len = buf_get(ptr);

	return OK;
}

/*===========================================================================*
 *				rdlink_hook				     *
 *===========================================================================*/
int rdlink_hook(struct inode *node, char *ptr, size_t max,
	cbdata_t UNUSED(cbdata))
{
	/* Symbolic link resolution hook. Not used yet.
	 */
	struct inode *parent;

	/* Get the parent inode. */
	parent = get_parent_inode(node);

	/* If the parent inode is a pid directory, call the pid handler.
	 */
	if (parent != NULL && dir_is_pid(parent))
		pid_link(node, ptr, max);

	return OK;
}
