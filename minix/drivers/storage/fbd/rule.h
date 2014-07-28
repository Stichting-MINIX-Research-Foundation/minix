#ifndef _FBD_RULE_H
#define _FBD_RULE_H

#define MAX_RULES	16

extern int rule_ctl(int request, endpoint_t endpt, cp_grant_id_t grant);

extern int rule_find(u64_t pos, size_t size, int flag);

extern void rule_pre_hook(iovec_t *iov, unsigned *count, size_t *size,
	u64_t *pos);
extern void rule_io_hook(char *buf, size_t size, u64_t pos, int flag);
extern void rule_post_hook(size_t osize, int *result);

#define PRE_HOOK	0x1
#define IO_HOOK		0x2
#define POST_HOOK	0x4

#endif /* _FBD_RULE_H */
