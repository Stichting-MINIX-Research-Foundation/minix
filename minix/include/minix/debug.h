#ifndef _MINIX_DEBUG_H
#define _MINIX_DEBUG_H 1

/* For reminders of things to be fixed. */
#define FIXME(str) { static int fixme_warned = 0; \
	if(!fixme_warned) { \
		printf("FIXME: %s:%d: %s\n", __FILE__, __LINE__, str);\
		fixme_warned = 1; \
	} \
}

#define NOT_REACHABLE	do {						\
	panic("NOT_REACHABLE at %s:%d", __FILE__, __LINE__);	\
	for(;;);							\
} while(0)

#define NOT_IMPLEMENTED do {	\
		panic("NOT_IMPLEMENTED at %s:%d", __FILE__, __LINE__); \
} while(0)

#endif /* _MINIX_DEBUG_H */

