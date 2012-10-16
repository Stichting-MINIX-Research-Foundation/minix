
#ifndef _MFS_CLEAN_H
#define _MFS_CLEAN_H 1

#define MARKDIRTY(b) do { if(superblock.s_dev == lmfs_dev(b) && superblock.s_rd_only) { printf("%s:%d: dirty block on rofs! ", __FILE__, __LINE__); util_stacktrace(); } else { lmfs_markdirty(b); } } while(0)
#define MARKCLEAN(b) lmfs_markclean(b)

#define ISDIRTY(b)	(!lmfs_isclean(b))
#define ISCLEAN(b)	(lmfs_isclean(b))

#endif
