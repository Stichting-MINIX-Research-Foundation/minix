
#ifndef _MFS_CLEAN_H
#define _MFS_CLEAN_H 1

#define MARKDIRTY(b) ((b)->b_dirt = BP_DIRTY)
#define MARKCLEAN(b) ((b)->b_dirt = BP_CLEAN)

#define ISDIRTY(b)	((b)->b_dirt == BP_DIRTY)
#define ISCLEAN(b)	((b)->b_dirt == BP_CLEAN)

#endif
