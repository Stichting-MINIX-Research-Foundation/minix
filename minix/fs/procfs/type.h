#ifndef _PROCFS_TYPE_H
#define _PROCFS_TYPE_H

typedef void *data_t;		/* abstract data type; can hold pointer */

struct load {
	clock_t ticks;		/* in this umber of ticks: */
	long proc_load;		/* .. the CPU had this load */
};

/*
 * ProcFS supports two groups of files: dynamic files, which are created within
 * process-specific (PID) directories and the service directory, and static
 * files, which are global.  For both, the following structure is used to
 * construct the files.
 *
 * For dynamic service files, no indirection infrastructure is present.  Each
 * service gets one flat file, named after its label, and generating the
 * contents of this flat file is all handled within the service module.  They
 * are not relevant to the rest of this comment.
 *
 * For dynamic PID files, the rules are simple: only regular files are
 * supported (although partial support for symbolic links is already present),
 * and the 'data' field must be filled with a pointer to a function of type:
 *
 *   void (*)(int slot)
 *
 * The function will be called whenever a read request for the file is made;
 * 'slot' contains the kernel slot number of the process being queried (so for
 * the PM and VFS process tables, NR_TASKS has to be subtracted from the slot
 * number to find the right slot).  The function is expected to produce
 * appropriate output using the buf_printf() function.
 *
 * For static files, regular files and directories are supported.  For
 * directories, the 'data' field must be a pointer to another 'struct file'
 * array that specifies the contents of the directory - this directory will
 * the be created recursively.  For regular files, the 'data' field must point
 * to a function of the type:
 *
 *   void (*)(void)
 *
 * Here too, the function will be called upon a read request, and it is
 * supposed to "fill" the file using buf_printf().  Obviously, for static
 * files, there is no slot number.
 *
 * For both static and dynamic files, 'mode' must specify the file type as well
 * as the access mode, and in both cases, each array is terminated with an
 * entry that has its name set to NULL.
 */
/*
 * The internal link between static/dynamic files/directories and VTreeFS'
 * indexes and cbdata values is as follows:
 * - Dynamic directories are always PID directories in the root directory.
 *   They are generated automatically, and are not specified using a "struct
 *   file" structure.  Their index is their slot number, so that getdents()
 *   calls always return any PID at most once.  Their cbdata value is the PID
 *   of the process associated with that dynamic directory, for the purpose of
 *   comparing old and new PIDs after updating process tables (without having
 *   to atoi() the directory's name).
 * - Dynamic files in a dynamic directory are PID files.  Their index is the
 *   array index into the "struct file" array of pid files (pid_files[]).  They
 *   are indexed at all because they may be deleted at any time due to inode
 *   shortages, independently of other dynamic files in the same directory.
 *   Recreating them without index would again risk possibly inconsistent
 *   getdents() results, where for example the same file shows up twice.
 *   VTreeFS currently does not distinguish between indexed and deletable files
 *   and hence, all dynamic files must be indexed so as to be deletable anyway.
 * - Dynamic files in a static directory are currently always service files.
 *   Their index is the slot number in process tables, for the same reasons as
 *   above.  They have no meaningful cbdata value.
 * - Static directories have no index (they are not and must not be deletable),
 *   and although their cbdata is their associated 'data' field from their
 *   "struct file" entries, their cbdata value is currently not relied on
 *   anywhere.  Then again, as of writing, there are no static directories at
 *   all, except the service directory, which is an exception case.
 * - Static files have no index either (for the same reason).  Their cbdata is
 *   also their 'data' field from the "struct file" entry creating the file,
 *   and this is used to actually call the callback function directly.
 */
struct file {
	char *name;		/* file name, maximum length PNAME_MAX */
	mode_t mode;		/* file mode, including file type */
	data_t data;		/* custom data associated with this file */
};

#endif /* _PROCFS_TYPE_H */
