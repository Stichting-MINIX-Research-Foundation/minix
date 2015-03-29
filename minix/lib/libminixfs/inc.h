#ifndef _LIBMINIXFS_INC_H
#define _LIBMINIXFS_INC_H

int lmfs_get_partial_block(struct buf **bpp, dev_t dev, block64_t block,
	int how, size_t block_size);

#endif /* !_LIBMINIXFS_INC_H */
