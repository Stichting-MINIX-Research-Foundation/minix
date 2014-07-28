#ifndef _FBD_ACTION_H
#define _FBD_ACTION_H

extern int action_mask(struct fbd_rule *rule);

extern void action_pre_hook(struct fbd_rule *rule, iovec_t *iov,
	unsigned *count, size_t *size, u64_t *pos);
extern void action_io_hook(struct fbd_rule *rule, char *buf, size_t size,
	u64_t pos, int flag);
extern void action_post_hook(struct fbd_rule *rule, size_t osize, int *result);

#endif /* _FBD_ACTION_H */
