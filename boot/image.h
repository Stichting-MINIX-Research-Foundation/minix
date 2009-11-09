/*	image.h - Info between installboot and boot.	Author: Kees J. Bot
 */

#define IM_NAME_MAX	63

struct image_header {
	char		name[IM_NAME_MAX + 1];	/* Null terminated. */
	struct exec	process;
};

/*
 * $PchId: image.h,v 1.4 1995/11/27 22:23:12 philip Exp $
 */
