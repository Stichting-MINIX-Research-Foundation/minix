
struct cached_page {
	/*  - The (dev, dev_offset) pair are unique;
	 *    the (ino, ino_offset) pair is information and
	 *    might be missing. duplicate do not make sense
	 *    although it won't bother VM much.
	 *  - dev must always be valid, i.e. not NO_DEV
	 *  - ino may be unknown, i.e. VMC_NO_INODE
	 */
	dev_t dev;			/* which dev is it on */
	u64_t dev_offset;		/* offset within dev */

	ino_t ino;			/* which ino is it about */
	u64_t ino_offset;		/* offset within ino */
	int flags;			/* currently only VMSF_ONCE or 0 */
	struct phys_block *page;	/* page ptr */
	struct cached_page *older;	/* older in lru chain */
	struct cached_page *newer;	/* newer in lru chain */
	struct cached_page *hash_next_dev; /* next in hash chain (bydev) */
	struct cached_page *hash_next_ino; /* next in hash chain (byino) */
};

