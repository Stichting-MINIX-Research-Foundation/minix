/* ProcFS - tree.c - dynamic PID tree management and hook implementations */

#include "inc.h"

struct minix_proc_list proc_list[NR_PROCS];

static int nr_pid_entries;

/*
 * Return a PID for the given slot, or 0 if the slot is not in use.
 */
pid_t
pid_from_slot(int slot)
{

	/* All kernel tasks are always present.*/
	if (slot < NR_TASKS)
		return (pid_t)(slot - NR_TASKS);

	/* For regular processes, check the process list. */
	if (proc_list[slot - NR_TASKS].mpl_flags & MPLF_IN_USE)
		return proc_list[slot - NR_TASKS].mpl_pid;
	else
		return 0;

}

/*
 * Check if the owner user and group ID of the inode are still in sync with
 * the current effective user and group ID of the given process.
 */
static int
check_owner(struct inode * node, int slot)
{
	struct inode_stat stat;

	if (slot < NR_TASKS) return TRUE;

	get_inode_stat(node, &stat);

	return (stat.uid == proc_list[slot - NR_TASKS].mpl_uid &&
	    stat.gid == proc_list[slot - NR_TASKS].mpl_gid);
}

/*
 * Fill in an inode_stat structure for the given process slot and per-PID file
 * index (or NO_INDEX for the process subdirectory root).
 */
static void
make_stat(struct inode_stat * stat, int slot, int index)
{

	if (index == NO_INDEX)
		stat->mode = DIR_ALL_MODE;
	else
		stat->mode = pid_files[index].mode;

	if (slot < NR_TASKS) {
		stat->uid = SUPER_USER;
		stat->gid = SUPER_USER;
	} else {
		stat->uid = proc_list[slot - NR_TASKS].mpl_uid;
		stat->gid = proc_list[slot - NR_TASKS].mpl_gid;
	}

	stat->size = 0;
	stat->dev = NO_DEV;
}

/*
 * Return whether the given node is a PID directory.
 */
static int
dir_is_pid(struct inode *node)
{

	return (get_parent_inode(node) == get_root_inode() &&
	    get_inode_index(node) != NO_INDEX);
}

/*
 * Get the process listing from the MIB service.
 */
static int
update_list(void)
{
	const int mib[] = { CTL_MINIX, MINIX_PROC, PROC_LIST };
	size_t size;
	int r;

	size = sizeof(proc_list);
	if (__sysctl(mib, __arraycount(mib), proc_list, &size, NULL, 0) != 0)
		printf("ProcFS: unable to obtain process list (%d)\n", -errno);

	return OK;
}

/*
 * Initialize this module, before VTreeFS is started.  As part of the process,
 * check if we're not compiled against a kernel different from the one that is
 * running at the moment.
 */
int
init_tree(void)
{
	int i, r;

	if ((r = update_list()) != OK)
		return r;

	/*
	 * Get the maximum number of entries that we may add to each PID's
	 * directory.  We could just default to a large value, but why not get
	 * it right?
	 */
	for (i = 0; pid_files[i].name != NULL; i++);

	nr_pid_entries = i;

	return OK;
}

/*
 * Out of inodes - the NR_INODES value is set too low.  We can not do much, but
 * we might be able to continue with degraded functionality, so do not panic.
 * If the NR_INODES value is not below the *crucial* minimum, the symptom of
 * this case will be an incomplete listing of the main proc directory.
 */
void
out_of_inodes(void)
{
	static int warned = FALSE;

	if (warned == FALSE) {
		printf("PROCFS: out of inodes!\n");

		warned = TRUE;
	}
}

/*
 * Regenerate the set of PID directories in the root directory of the file
 * system.  Add new directories and delete old directories as appropriate;
 * leave unchanged those that should remain the same.
 */
static void
construct_pid_dirs(void)
{
	/*
	 * We have to make two passes.  Otherwise, we would trigger a vtreefs
	 * assert when we add an entry for a PID before deleting the previous
	 * entry for that PID.  While rare, such rapid PID reuse does occur in
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

		/*
		 * If the process slot is not in use, delete the associated
		 * inode.  Otherwise, get the process ID.
		 */
		if ((pid = pid_from_slot(i)) == 0) {
			delete_inode(node);

			continue;
		}

		/*
		 * If there is an old entry, see if the pid matches the current
		 * entry, and the owner is still the same.  Otherwise, delete
		 * the old entry first.  We reconstruct the entire subtree even
		 * if only the owner changed, for security reasons: if a
		 * process could keep open a file or directory across the owner
		 * change, it might be able to access information it shouldn't.
		 */
		if (pid != (pid_t)get_inode_cbdata(node) ||
		    !check_owner(node, i))
			delete_inode(node);
	}

	/* Second pass: add new entries. */
	for (i = 0; i < NR_PROCS + NR_TASKS; i++) {
		/* If the process slot is not in use, skip this slot. */
		if ((pid = pid_from_slot(i)) == 0)
			continue;

		/*
		 * If we have an inode associated with this slot, we have
		 * already checked it to be up-to-date above.
		 */
		if (get_inode_by_index(root, i) != NULL)
			continue;

		/* Get the process ID. */
		if (i < NR_TASKS)
			pid = (pid_t)(i - NR_TASKS);
		else
			pid = proc_list[i - NR_TASKS].mpl_pid;

		/* Add the entry for the process slot. */
		snprintf(name, PNAME_MAX + 1, "%d", pid);

		make_stat(&stat, i, NO_INDEX);

		node = add_inode(root, name, i, &stat, nr_pid_entries,
		    (cbdata_t)pid);

		if (node == NULL)
			out_of_inodes();
	}
}

/*
 * Construct one file in a PID directory, if a file with the given name should
 * exist at all.
 */
static void
make_one_pid_entry(struct inode * parent, char * name, int slot)
{
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

			node = add_inode(parent, name, i, &stat, (index_t)0,
			    (cbdata_t)0);

			if (node == NULL)
				out_of_inodes();

			break;
		}
	}
}

/*
 * Construct all files in a PID directory.
 */
static void
make_all_pid_entries(struct inode * parent, int slot)
{
	struct inode *node;
	struct inode_stat stat;
	int i;

	for (i = 0; pid_files[i].name != NULL; i++) {
		node = get_inode_by_index(parent, i);
		if (node != NULL)
			continue;

		make_stat(&stat, slot, i);

		node = add_inode(parent, pid_files[i].name, i, &stat,
		    (index_t)0, (cbdata_t)0);

		if (node == NULL)
			out_of_inodes();
	}
}

/*
 * Construct one requested file entry, or all file entries, in a PID directory.
 */
static void
construct_pid_entries(struct inode * parent, char * name)
{
	int slot;

	slot = get_inode_index(parent);
	assert(slot >= 0 && slot < NR_TASKS + NR_PROCS);

	/* If this process is already gone, delete the directory now. */
	if (pid_from_slot(slot) == 0) {
		delete_inode(parent);

		return;
	}

	/*
	 * If a specific file name is being looked up, see if we have to add
	 * an inode for that file.  If the directory contents are being
	 * retrieved, add all files that have not yet been added.
	 */
	if (name != NULL)
		make_one_pid_entry(parent, name, slot);
	else
		make_all_pid_entries(parent, slot);
}

/*
 * Data is requested from one of the files in a PID directory. Call the
 * function that is responsible for generating the data for that file.
 */
static void
pid_read(struct inode * node)
{
	struct inode *parent;
	int slot, index;

	/*
	 * Get the slot number of the process.  Note that this currently will
	 * not work for files not in the top-level pid subdirectory.
	 */
	parent = get_parent_inode(node);

	slot = get_inode_index(parent);

	/* Get this file's index number. */
	index = get_inode_index(node);

	/* Call the handler procedure for the file. */
	((void (*)(int))pid_files[index].data)(slot);
}

/*
 * The contents of a symbolic link in a PID directory are requested.  This
 * function is a placeholder for future use.
 */
static int
pid_link(struct inode * __unused node, char * ptr, int max)
{

	/* Nothing yet. */
	strlcpy(ptr, "", max);

	return OK;
}

/*
 * Path name resolution hook, for a specific parent and name pair.  If needed,
 * update our own view of the system first; after that, determine whether we
 * need to (re)generate certain files.
 */
int
lookup_hook(struct inode * parent, char * name, cbdata_t __unused cbdata)
{
	static clock_t last_update = 0;
	clock_t now;

	/*
	 * Update lazily for lookups, as this gets too expensive otherwise.
	 * Alternative: pull in only PM's table?
	 */
	now = getticks();

	if (last_update != now) {
		update_list();

		last_update = now;
	}

	/*
	 * If the parent is the root directory, we must now reconstruct all
	 * entries, because some of them might have been garbage collected.
	 * We must update the entire tree at once; if we update individual
	 * entries, we risk name collisions.
	 *
	 * If the parent is a process directory, we may need to (re)construct
	 * the entry being looked up.
	 */
	if (parent == get_root_inode())
		construct_pid_dirs();
	else if (dir_is_pid(parent))
		/*
		 * We might now have deleted our current containing directory;
		 * construct_pid_entries() will take care of this case.
		 */
		construct_pid_entries(parent, name);
	else
		/* TODO: skip updating the main tables in this case. */
		service_lookup(parent, now);

	return OK;
}

/*
 * Directory entry retrieval hook, for potentially all files in a directory.
 * Make sure that all files that are supposed to be returned, are actually part
 * of the virtual tree.
 */
int
getdents_hook(struct inode * node, cbdata_t __unused cbdata)
{

	if (node == get_root_inode()) {
		update_list();

		construct_pid_dirs();
	} else if (dir_is_pid(node))
		construct_pid_entries(node, NULL /*name*/);
	else
		service_getdents(node);

	return OK;
}

/*
 * Regular file read hook.  Call the appropriate callback function to generate
 * and return the data.
 */
ssize_t
read_hook(struct inode * node, char * ptr, size_t len, off_t off,
	cbdata_t cbdata)
{
	struct inode *parent;

	buf_init(ptr, len, off);

	/* Populate the buffer with the proper content. */
	if (get_inode_index(node) != NO_INDEX) {
		parent = get_parent_inode(node);

		/* The PID directories are indexed; service/ is not. */
		if (get_inode_index(parent) != NO_INDEX)
			pid_read(node);
		else
			service_read(node);
	} else
		((void (*)(void))cbdata)();

	return buf_result();
}

/*
 * Symbolic link resolution hook.  Not used yet.
 */
int
rdlink_hook(struct inode * node, char * ptr, size_t max,
	cbdata_t __unused cbdata)
{
	struct inode *parent;

	/* Get the parent inode. */
	parent = get_parent_inode(node);

	/* If the parent inode is a pid directory, call the pid handler. */
	if (parent != NULL && dir_is_pid(parent))
		pid_link(node, ptr, max);

	return OK;
}
