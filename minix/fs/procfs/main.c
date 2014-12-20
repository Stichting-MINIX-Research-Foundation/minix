/* ProcFS - main.c - main functions of the process file system */

#include "inc.h"

static void init_hook(void);

/* The hook functions that will be called by VTreeFS. */
static struct fs_hooks hooks = {
	.init_hook	= init_hook,
	.lookup_hook	= lookup_hook,
	.getdents_hook	= getdents_hook,
	.read_hook	= read_hook,
	.rdlink_hook	= rdlink_hook,
};

/*
 * Construct a tree of static files from a null-terminated array of file
 * structures, recursively creating directories which have their associated
 * data point to child file structures.
 */
static void
construct_tree(struct inode * dir, struct file * files)
{
	struct file *file;
	struct inode *node;
	struct inode_stat stat;

	stat.uid = SUPER_USER;
	stat.gid = SUPER_USER;
	stat.size = 0;
	stat.dev = NO_DEV;

	for (file = files; file->name != NULL; file++) {
		stat.mode = file->mode;

		node = add_inode(dir, file->name, NO_INDEX, &stat, (index_t)0,
		    (cbdata_t)file->data);

		assert(node != NULL);

		if (S_ISDIR(file->mode))
			construct_tree(node, (struct file *)file->data);
	}
}

/*
 * Initialization hook.  Generate the static part of the tree.
 */
static void
init_hook(void)
{
	static int first_time = TRUE;
	struct inode *root;
	int r;

	if (first_time) {
		/*
		 * Initialize some state.  If we are incompatible with the kernel,
		 * exit immediately.
		 */
		if ((r = init_tree()) != OK)
			panic("init_tree failed!");

		root = get_root_inode();

		construct_tree(root, root_files);

		service_init();

		first_time = FALSE;
	}
}

/*
 * ProcFS entry point.
 */
int main(void)
{
	static struct inode_stat stat;

	/* Properties of the root directory. */
	stat.mode 	= DIR_ALL_MODE;
	stat.uid 	= SUPER_USER;
	stat.gid 	= SUPER_USER;
	stat.size 	= 0;
	stat.dev 	= NO_DEV;

	/* Run VTreeFS. */
	run_vtreefs(&hooks, NR_INODES, 0, &stat, NR_PROCS + NR_TASKS,
	    BUF_SIZE);

	return 0;
}
