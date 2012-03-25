#include <minix/drivers.h>
#include <sys/ioc_fbd.h>
#include <assert.h>

#include "rule.h"

/*===========================================================================*
 *				get_rand				     *
 *===========================================================================*/
static u32_t get_rand(u32_t max)
{
	/* Las Vegas algorithm for getting a random number in the range from
	 * 0 to max, inclusive.
	 */
	u32_t val, top;

	/* Get an initial random number. */
	val = lrand48() ^ (lrand48() << 1);

	/* Make 'max' exclusive. If it wraps, we can use the full width. */
	if (++max == 0) return val;

	/* Find the largest multiple of the given range, and return a random
	 * number from the range, throwing away any random numbers not below
	 * this largest multiple.
	 */
	top = (((u32_t) -1) / max) * max;

	while (val >= top)
		val = lrand48() ^ (lrand48() << 1);

	return val % max;
}

/*===========================================================================*
 *				get_range				     *
 *===========================================================================*/
static size_t get_range(struct fbd_rule *rule, u64_t pos, size_t *size,
	u64_t *skip)
{
	/* Compute the range within the given request range that is affected
	 * by the given rule, and optionally the number of bytes preceding
	 * the range that are also affected by the rule.
	 */
	u64_t delta;
	size_t off;
	int to_eof;

	to_eof = cmp64(rule->start, rule->end) >= 0;

	if (cmp64(pos, rule->start) > 0) {
		if (skip != NULL) *skip = sub64(pos, rule->start);

		off = 0;
	}
	else {
		if (skip != NULL) *skip = cvu64(0);

		delta = sub64(rule->start, pos);

		assert(ex64hi(delta) == 0);

		off = ex64lo(delta);
	}

	if (!to_eof) {
		assert(cmp64(pos, rule->end) < 0);

		delta = sub64(rule->end, pos);

		if (cmp64u(delta, *size) < 0)
			*size = ex64lo(delta);
	}

	assert(*size > off);

	*size -= off;

	return off;
}

/*===========================================================================*
 *				limit_range				     *
 *===========================================================================*/
static void limit_range(iovec_t *iov, unsigned *count, size_t size)
{
	/* Limit the given vector to the given size.
	 */
	size_t chunk;
	int i;

	for (i = 0; i < *count && size > 0; i++) {
		chunk = MIN(iov[i].iov_size, size);

		if (chunk == size)
			iov[i].iov_size = size;

		size -= chunk;
	}

	*count = i;
}

/*===========================================================================*
 *				action_io_corrupt			     *
 *===========================================================================*/
static void action_io_corrupt(struct fbd_rule *rule, char *buf, size_t size,
	u64_t pos, int UNUSED(flag))
{
	u64_t skip;
	u32_t val;

	buf += get_range(rule, pos, &size, &skip);

	switch (rule->params.corrupt.type) {
	case FBD_CORRUPT_ZERO:
		memset(buf, 0, size);
		break;

	case FBD_CORRUPT_PERSIST:
		/* Non-dword-aligned positions and sizes are not supported;
		 * not by us, and not by the driver.
		 */
		if (ex64lo(pos) & (sizeof(val) - 1)) break;
		if (size & (sizeof(val) - 1)) break;

		/* Consistently produce the same pattern for the same range. */
		val = ex64lo(skip);

		for ( ; size >= sizeof(val); size -= sizeof(val)) {
			*((u32_t *) buf) = val ^ 0xdeadbeefUL;

			val += sizeof(val);
			buf += sizeof(val);
		}

		break;

	case FBD_CORRUPT_RANDOM:
		while (size--)
			*buf++ = get_rand(255);

		break;

	default:
		printf("FBD: unknown corruption type %d\n",
			rule->params.corrupt.type);
	}
}

/*===========================================================================*
 *				action_pre_error			     *
 *===========================================================================*/
static void action_pre_error(struct fbd_rule *rule, iovec_t *iov,
	unsigned *count, size_t *size, u64_t *pos)
{
	/* Limit the request to the part that precedes the matched range. */
	*size = get_range(rule, *pos, size, NULL);

	limit_range(iov, count, *size);
}

/*===========================================================================*
 *				action_post_error			     *
 *===========================================================================*/
static void action_post_error(struct fbd_rule *rule, size_t UNUSED(osize),
	int *result)
{
	/* Upon success of the first part, return the specified error code. */
	if (*result >= 0 && rule->params.error.code != OK)
		*result = rule->params.error.code;
}

/*===========================================================================*
 *				action_pre_misdir			     *
 *===========================================================================*/
static void action_pre_misdir(struct fbd_rule *rule, iovec_t *UNUSED(iov),
	unsigned *UNUSED(count), size_t *UNUSED(size), u64_t *pos)
{
	/* Randomize the request position to fall within the range (and have
	 * the alignment) given by the rule.
	 */
	u32_t range, choice;

	/* Unfortunately, we cannot interpret 0 as end as "up to end of disk"
	 * here, because we have no idea about the actual disk size, and the
	 * resulting address must of course be valid..
	 */
	range = div64u(add64u(sub64(rule->params.misdir.end,
		rule->params.misdir.start), 1), rule->params.misdir.align);

	if (range > 0)
		choice = get_rand(range - 1);
	else
		choice = 0;

	*pos = add64(rule->params.misdir.start,
		mul64u(choice, rule->params.misdir.align));
}

/*===========================================================================*
 *				action_pre_losttorn			     *
 *===========================================================================*/
static void action_pre_losttorn(struct fbd_rule *rule, iovec_t *iov,
	unsigned *count, size_t *size, u64_t *UNUSED(pos))
{
	if (*size > rule->params.losttorn.lead)
		*size = rule->params.losttorn.lead;

	limit_range(iov, count, *size);
}

/*===========================================================================*
 *				action_post_losttorn			     *
 *===========================================================================*/
static void action_post_losttorn(struct fbd_rule *UNUSED(rule), size_t osize,
	int *result)
{
	/* On success, pretend full completion. */

	if (*result < 0) return;

	*result = osize;
}

/*===========================================================================*
 *				action_mask				     *
 *===========================================================================*/
int action_mask(struct fbd_rule *rule)
{
	/* Return the hook mask for the given rule's action type. */

	switch (rule->action) {
	case FBD_ACTION_CORRUPT:	return IO_HOOK;
	case FBD_ACTION_ERROR:		return PRE_HOOK | POST_HOOK;
	case FBD_ACTION_MISDIR:		return PRE_HOOK;
	case FBD_ACTION_LOSTTORN:	return PRE_HOOK | POST_HOOK;
	default:
		printf("FBD: unknown action type %d\n", rule->action);
		return 0;
	}
}

/*===========================================================================*
 *				action_pre_hook				     *
 *===========================================================================*/
void action_pre_hook(struct fbd_rule *rule, iovec_t *iov,
	unsigned *count, size_t *size, u64_t *pos)
{
	switch (rule->action) {
	case FBD_ACTION_ERROR:
		action_pre_error(rule, iov, count, size, pos);
		break;

	case FBD_ACTION_MISDIR:
		action_pre_misdir(rule, iov, count, size, pos);
		break;

	case FBD_ACTION_LOSTTORN:
		action_pre_losttorn(rule, iov, count, size, pos);
		break;

	default:
		printf("FBD: bad action type %d for PRE hook\n", rule->action);
	}
}

/*===========================================================================*
 *				action_io_hook				     *
 *===========================================================================*/
void action_io_hook(struct fbd_rule *rule, char *buf, size_t size,
	u64_t pos, int flag)
{
	switch (rule->action) {
	case FBD_ACTION_CORRUPT:
		action_io_corrupt(rule, buf, size, pos, flag);
		break;

	default:
		printf("FBD: bad action type %d for IO hook\n", rule->action);
	}
}

/*===========================================================================*
 *				action_post_hook			     *
 *===========================================================================*/
void action_post_hook(struct fbd_rule *rule, size_t osize, int *result)
{
	switch (rule->action) {
	case FBD_ACTION_ERROR:
		action_post_error(rule, osize, result);
		return;

	case FBD_ACTION_LOSTTORN:
		action_post_losttorn(rule, osize, result);
		return;

	default:
		printf("FBD: bad action type %d for POST hook\n",
			rule->action);
	}
}
