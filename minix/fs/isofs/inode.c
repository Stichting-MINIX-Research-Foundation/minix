
/*
 * This file contains all the function that handle the dir records
 * (inodes) for the ISO9660 filesystem.
 */

#include "inc.h"

#include "uthash.h"

struct inode_cache {
	ino_t key;
	struct inode *value;
	UT_hash_handle hh;
} ;

struct inode_cache *icache = NULL;

void read_inode_iso9660(struct inode_dir_entry *i,
	const struct iso9660_dir_record *dir_rec, struct dir_extent *extent,
	size_t offset, int name_only);

#ifdef ISO9660_OPTION_MODE3
static void read_inode_extents(struct inode_dir_entry *i,
	const struct iso9660_dir_record *dir_rec, struct dir_extent *extent,
	size_t *offset);
#endif

#ifdef ISO9660_OPTION_ROCKRIDGE
void read_inode_susp(struct inode_dir_entry *i,
	const struct iso9660_dir_record *dir_rec, struct buf *bp, size_t offset,
	int name_only);
#endif

static int check_dir_record(const struct iso9660_dir_record *d, size_t offset);

int fs_putnode(ino_t ino_nr, unsigned int count)
{
       /*
        * Find the inode specified by the request message and decrease its
        * counter.
        */
	struct inode *i_node;

	if ((i_node = get_inode(ino_nr)) == NULL) {
		puts("ISOFS: trying to free unused inode");
		return EINVAL;
	}
	if (count > i_node->i_count) {
		puts("ISOFS: put_node count too high");
		return EINVAL;
	}

	i_node->i_count -= count - 1;
	put_inode(i_node);
	return OK;
}


struct inode* get_inode(ino_t ino_nr) {
	/* Return an already opened inode from cache. */
	struct inode *i_node = inode_cache_get(ino_nr);

	if (i_node == NULL)
		return NULL;

	if (i_node->i_count == 0)
		return NULL;

	return i_node;
}

struct inode* open_inode(ino_t ino_nr) {
	/* Return an inode from cache. */
	struct inode *i_node = inode_cache_get(ino_nr);
	if (i_node == NULL)
		return NULL;

	i_node->i_count++;

	return i_node;
}

void put_inode(struct inode *i_node) {
	if (i_node == NULL)
		return;

	assert(i_node->i_count > 0);
	i_node->i_count--;

	if(i_node->i_count == 0)
		i_node->i_mountpoint = FALSE;
}

void dup_inode(struct inode *i_node) {
	assert(i_node != NULL);
	assert(i_node->i_count > 0);

	i_node->i_count++;
}

int read_directory(struct inode *dir) {
#define MAX_ENTRIES 256		/* avoid using lots of stack.. */
	/* Read all entries in a directory. */
	size_t pos = 0, saved_pos, cur_entry, num_entries, cpt;
	struct inode_dir_entry entries[MAX_ENTRIES + 1];
	int status;

	if (dir->dir_contents)
		return OK;

	if (!S_ISDIR(dir->i_stat.st_mode))
		return ENOTDIR;

	/*
	 * We do not know how many inode entries we will find, but we want to
	 * allocate an array of the right size for dir->dir_contents.  First
	 * find out how many entries there are, and store up to MAX_ENTRIES of
	 * them into a temporary array on the stack.  If there are more than
	 * MAX_ENTRIES entries, we have to do a second pass on the part of the
	 * directory that we did not manage to fit in the temporary array.
	 *
	 * The entire service needs massive structural improvement (and in
	 * particular, no dynamic memory allocation like this), but for now
	 * this is the simplest way to be fast for small directories while at
	 * the same time supporting seriously large directories.
	 */
	cur_entry = 0;
	num_entries = 0;

	while ((status = read_inode(&entries[cur_entry], &dir->extent,
	    &pos)) == OK) {
		/* Dump the entry if it's not to be exported to userland. */
		if (entries[cur_entry].i_node->skip) {
			free_inode_dir_entry(&entries[cur_entry]);
			continue;
		}

		if (cur_entry < MAX_ENTRIES) {
			cur_entry++;

			/*
			 * As long as more entries fit in the temporary array,
			 * update the saved position of the next entry.  Once
			 * we hit the first entry that does not fit (if any),
			 * the updating stops and we will have the correct
			 * saved position.
			 */
			saved_pos = pos;
		} else {
			/*
			 * No room in the temporary array.  Free the entry
			 * again.  This is costly but only for those rare
			 * directories that have more than MAX_ENTRIES entries.
			 */
			free_inode_dir_entry(&entries[cur_entry]);
		}

		num_entries++;
	}

	/*
	 * Allocate a dynamic array of the correct size, and populate it with
	 * all the entries in the temporary array.  For large directories, the
	 * temporary array will have partial results, in which case we have to
	 * do a second pass on the rest below.
	 */
	dir->dir_contents =
	    alloc_mem(sizeof(struct inode_dir_entry) * num_entries);

	memcpy(dir->dir_contents, entries,
	    sizeof(struct inode_dir_entry) * cur_entry);

	/*
	 * The second pass.  This pass starts from the saved position and reads
	 * only the entries that did not fit in the temporary array.  This time
	 * we can read straight into the actual destination array.  We expect
	 * to find the same entries as during the first pass.
	 */
	while (cur_entry < num_entries) {
		if (read_inode(&dir->dir_contents[cur_entry], &dir->extent,
		    &saved_pos) != OK)
			panic("unexpected EOF or error rereading directory");

		if (dir->dir_contents[cur_entry].i_node->skip) {
			free_inode_dir_entry(&entries[cur_entry]);
			continue;
		}

		cur_entry++;
	}

	dir->dir_size = num_entries;

	/* The name pointer has to point to the new memory location. */
	for (cpt = 0; cpt < num_entries; cpt++) {
		if (dir->dir_contents[cpt].r_name == NULL)
			dir->dir_contents[cpt].name =
			    dir->dir_contents[cpt].i_name;
		else
			dir->dir_contents[cpt].name =
			    dir->dir_contents[cpt].r_name;
	}

	return (status == EOF) ? OK : status;
}

int check_inodes(void) {
	/* Check whether there are no more inodes in use. Called on unmount. */
	int i;

	/* XXX: actually check for inodes in use. */
	return TRUE;
}

int read_inode(struct inode_dir_entry *dir_entry, struct dir_extent *extent,
	size_t *offset)
{
	struct iso9660_dir_record *dir_rec;
	struct buf *bp;
	struct inode *i_node;
	ino_t ino_nr;
	int name_only = FALSE;

	/* Find inode. */
	bp = read_extent_block(extent, *offset);
	if (bp == NULL) {
		return EOF;
	}

	/* Check if we are crossing a sector boundary. */
	dir_rec = (struct iso9660_dir_record*)(b_data(bp) + *offset %
	          v_pri.logical_block_size_l);

	if (dir_rec->length == 0) {
		*offset = ((*offset / v_pri.logical_block_size_l) + 1) *
		    v_pri.logical_block_size_l;

		lmfs_put_block(bp);
		bp = read_extent_block(extent, *offset);
		if (bp == NULL) {
			return EOF;
		}

		dir_rec = (struct iso9660_dir_record*)(b_data(bp) + *offset %
	          v_pri.logical_block_size_l);
	}

	/* Parse basic ISO 9660 specs. */
	if (check_dir_record(dir_rec, *offset % v_pri.logical_block_size_l)
	    != OK) {
		lmfs_put_block(bp);
		return EINVAL;
	}

	/* Get inode */
	if ((dir_rec->file_flags & D_TYPE) == D_DIRECTORY) {
		ino_nr = dir_rec->loc_extent_l;
	}
	else {
		ino_nr = get_extent_absolute_block_id(extent, *offset)
		    * v_pri.logical_block_size_l +
		    *offset % v_pri.logical_block_size_l;
	}

	memset(dir_entry, 0, sizeof(*dir_entry));

	i_node = inode_cache_get(ino_nr);
	if (i_node) {
		/* Inode was already loaded, parse file names only. */
		dir_entry->i_node = i_node;
		i_node->i_refcount++;

		memset(&dir_entry->i_name[0], 0, sizeof(dir_entry->i_name));

		name_only = TRUE;
	}
	else {
		/* Inode wasn't in memory, parse it. */
		i_node = alloc_mem(sizeof(struct inode));
		dir_entry->i_node = i_node;
		i_node->i_refcount = 1;
		i_node->i_stat.st_ino = ino_nr;
		inode_cache_add(ino_nr, i_node);
	}

	dir_entry->i_node = i_node;
	read_inode_iso9660(dir_entry, dir_rec, extent, *offset, name_only);

	/* Parse extensions. */
#ifdef ISO9660_OPTION_ROCKRIDGE
	read_inode_susp(dir_entry, dir_rec, bp,
	    *offset % v_pri.logical_block_size_l, name_only);
#endif

	*offset += dir_rec->length;
	if (dir_rec->length % 2)
		(*offset)++;

#ifdef ISO9660_OPTION_MODE3
	read_inode_extents(dir_entry, dir_rec, extent, offset);
#endif

	lmfs_put_block(bp);

	return OK;
}

struct inode* inode_cache_get(ino_t ino_nr) {
	struct inode_cache *i_node;
	HASH_FIND(hh, icache, &ino_nr, sizeof(ino_t), i_node);

	if (i_node)
		return i_node->value;
	else
		return NULL;
}

void inode_cache_add(ino_t ino_nr, struct inode *i_node) {
	struct inode_cache *c_check;
	struct inode_cache *c_entry;

	HASH_FIND(hh, icache, &ino_nr, sizeof(ino_t), c_check);

	if (c_check == NULL) {
		c_entry = alloc_mem(sizeof(struct inode_cache));
		c_entry->key = ino_nr;
		c_entry->value = i_node;

		HASH_ADD(hh, icache, key, sizeof(ino_t), c_entry);
	}
	else
		panic("Trying to insert inode into cache twice");
}

void read_inode_iso9660(struct inode_dir_entry *i,
	const struct iso9660_dir_record *dir_rec, struct dir_extent *extent,
	size_t offset, int name_only)
{
	char *cp;

	/* Parse file name. */
	if (dir_rec->file_id[0] == 0)
		strcpy(i->i_name, ".");
	else if (dir_rec->file_id[0] == 1)
		strcpy(i->i_name, "..");
	else {
		memcpy(i->i_name, dir_rec->file_id, dir_rec->length_file_id);

		/* Truncate/ignore file version suffix. */
		cp = strchr(i->i_name, ';');
		if (cp != NULL) {
			*cp = '\0';
			/* Truncate dot if file has no extension. */
			if (strchr(i->i_name, '.') + 1 == cp)
				*(cp-1) = '\0';
		}
	}

	if (name_only == TRUE)
		return;

	/* Parse first extent. */
	if (dir_rec->data_length_l > 0) {
		i->i_node->extent.location = dir_rec->loc_extent_l +
		    dir_rec->ext_attr_rec_length;
		i->i_node->extent.length = dir_rec->data_length_l /
		    v_pri.logical_block_size_l;

		if (dir_rec->data_length_l % v_pri.logical_block_size_l)
			i->i_node->extent.length++;

		i->i_node->i_stat.st_size = dir_rec->data_length_l;
	}

	/* Parse timestamps (record date). */
	i->i_node->i_stat.st_atime = i->i_node->i_stat.st_mtime =
	    i->i_node->i_stat.st_ctime = i->i_node->i_stat.st_birthtime =
	    date7_to_time_t(dir_rec->rec_date);

	if ((dir_rec->file_flags & D_TYPE) == D_DIRECTORY)
		i->i_node->i_stat.st_mode = S_IFDIR;
	else
		i->i_node->i_stat.st_mode = S_IFREG;

	i->i_node->i_stat.st_mode |= 0555;

	/* Initialize stat. */
	i->i_node->i_stat.st_dev = fs_dev;
	i->i_node->i_stat.st_blksize = v_pri.logical_block_size_l;
	i->i_node->i_stat.st_blocks = dir_rec->data_length_l / 512;
	i->i_node->i_stat.st_nlink = 1;
}

#ifdef ISO9660_OPTION_ROCKRIDGE

void read_inode_susp(struct inode_dir_entry *i,
	const struct iso9660_dir_record *dir_rec, struct buf *bp, size_t offset,
	int name_only)
{
	int susp_offset, susp_size, name_length;
	struct rrii_dir_record rrii_data;

	susp_offset = 33 + dir_rec->length_file_id;
	/* Get rid of padding byte. */
	if(dir_rec->length_file_id % 2 == 0) {
		susp_offset++;
	}

	if(dir_rec->length - susp_offset < 4)
		return;

	susp_size = dir_rec->length - susp_offset;

	/* Initialize record with known, sane data. */
	memcpy(rrii_data.mtime, dir_rec->rec_date, ISO9660_SIZE_DATE7);
	memcpy(rrii_data.atime, dir_rec->rec_date, ISO9660_SIZE_DATE7);
	memcpy(rrii_data.ctime, dir_rec->rec_date, ISO9660_SIZE_DATE7);
	memcpy(rrii_data.birthtime, dir_rec->rec_date, ISO9660_SIZE_DATE7);

	rrii_data.d_mode = i->i_node->i_stat.st_mode;
	rrii_data.uid    = SYS_UID;
	rrii_data.gid    = SYS_GID;
	rrii_data.rdev   = NO_DEV;
	rrii_data.file_id_rrip[0] = '\0';
	rrii_data.slink_rrip[0]   = '\0';
	rrii_data.reparented_inode = NULL;

	parse_susp_buffer(&rrii_data, b_data(bp)+offset+susp_offset, susp_size);

	/* Copy back data from rrii_dir_record structure. */
	if (rrii_data.file_id_rrip[0] != '\0') {
		name_length = strlen(rrii_data.file_id_rrip);
		i->r_name = alloc_mem(name_length + 1);
		memcpy(i->r_name, rrii_data.file_id_rrip, name_length);
	}

	if (rrii_data.slink_rrip[0] != '\0') {
		name_length = strlen(rrii_data.slink_rrip);
		i->i_node->s_name = alloc_mem(name_length + 1);
		memcpy(i->i_node->s_name, rrii_data.slink_rrip, name_length);
	}

	if (rrii_data.reparented_inode) {
		/* Recycle the inode already parsed. */
		i->i_node = rrii_data.reparented_inode;
		return;
	}

	/* XXX: not the correct way to ignore reparented directory holder... */
	if (strcmp(rrii_data.file_id_rrip, ".rr_moved") == 0)
		i->i_node->skip = 1;

	if (name_only == TRUE)
		return;

	/* Write back all Rock Ridge properties. */
	i->i_node->i_stat.st_atime = date7_to_time_t(rrii_data.atime);
	i->i_node->i_stat.st_ctime = date7_to_time_t(rrii_data.ctime);
	i->i_node->i_stat.st_mtime = date7_to_time_t(rrii_data.mtime);
	i->i_node->i_stat.st_birthtime = date7_to_time_t(rrii_data.birthtime);

	i->i_node->i_stat.st_mode = rrii_data.d_mode;
	i->i_node->i_stat.st_uid  = rrii_data.uid;
	i->i_node->i_stat.st_gid  = rrii_data.gid;
	i->i_node->i_stat.st_rdev = rrii_data.rdev;
}

#endif

#ifdef ISO9660_OPTION_MODE3

void read_inode_extents(struct inode *i,
	const struct iso9660_dir_record *dir_rec,
	struct dir_extent *extent, size_t *offset)
{
	panic("read_inode_extents() isn't implemented yet!");
}

#endif

int check_dir_record(const struct iso9660_dir_record *d, size_t offset) {
	/* Run some consistency check on a directory entry. */
	if ((d->length < 33) || (d->length_file_id < 1))
		return EINVAL;
	if (d->length_file_id + 32 > d->length)
		return EINVAL;
	if (offset + d->length > v_pri.logical_block_size_l)
		return EINVAL;

	return OK;
}
