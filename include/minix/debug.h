#ifndef _MINIX_DEBUG_H
#define _MINIX_DEBUG_H 1

/* For reminders of things to be fixed. */
#define FIXME(str) { static int fixme_warned = 0; \
	if(!fixme_warned) { \
		printf("FIXME: %s:%d: %s\n", __FILE__, __LINE__, str);\
		fixme_warned = 1; \
	} \
}

#endif /* _MINIX_DEBUG_H */

