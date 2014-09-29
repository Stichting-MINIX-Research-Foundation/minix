#include <minix/drivers.h>
#include <minix/ioctl.h>
#include <sys/ioc_fbd.h>

#include "action.h"
#include "rule.h"

static struct fbd_rule rules[MAX_RULES];
static struct fbd_rule *matches[MAX_RULES];
static int nr_matches;

/*===========================================================================*
 *				rule_ctl				     *
 *===========================================================================*/
int rule_ctl(unsigned long request, endpoint_t endpt, cp_grant_id_t grant)
{
	/* Handle an I/O control request regarding rules. */
	fbd_rulenum_t i;
	int r;

	/* Note that any of the safecopy calls may fail if the ioctl is
	 * improperly defined in userland; never panic if they fail!
	 */
	switch (request) {
	case FBDCADDRULE:
		/* Find a free rule slot. */
		for (i = 1; i <= MAX_RULES; i++)
			if (rules[i-1].num == 0)
				break;

		if (i == MAX_RULES+1)
			return ENOMEM;

		/* Copy in the rule. */
		if ((r = sys_safecopyfrom(endpt, grant, 0,
				(vir_bytes) &rules[i-1], sizeof(rules[0]))) != OK)
			return r;

		/* Mark the rule as active, and return its number. */
		rules[i-1].num = i;

		return i;

	case FBDCDELRULE:
		/* Copy in the given rule number. */
		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &i,
				sizeof(i))) != OK)
			return r;

		/* Fail if the given rule number is not valid or in use.
		 * Allow the caller to determine the maximum rule number.
		 */
		if (i <= 0 || i > MAX_RULES) return EINVAL;

		if (rules[i-1].num != i) return ENOENT;

		/* Mark the rule as not active. */
		rules[i-1].num = 0;

		return OK;

	case FBDCGETRULE:
		/* Copy in just the rule number from the given structure. */
		if ((r = sys_safecopyfrom(endpt, grant,
				offsetof(struct fbd_rule, num), (vir_bytes) &i,
				sizeof(i))) != OK)
			return r;

		/* Fail if the given rule number is not valid or in use.
		 * Allow the caller to determine the maximum rule number.
		 */
		if (i <= 0 || i > MAX_RULES) return EINVAL;

		if (rules[i-1].num != i) return ENOENT;

		/* Copy out the entire rule as is. */
		return sys_safecopyto(endpt, grant, 0, (vir_bytes) &rules[i-1],
				sizeof(rules[0]));

	default:
		return EINVAL;
	}
}

/*===========================================================================*
 *				rule_match				     *
 *===========================================================================*/
static int rule_match(struct fbd_rule *rule, u64_t pos, size_t size, int flag)
{
	/* Check whether the given rule matches the given parameters. As side
	 * effect, update counters in the rule as appropriate.
	 */

	/* Ranges must overlap (start < pos+size && end > pos). */
	if (rule->start >= pos + size ||
			(rule->end != 0 && rule->end <= pos))
		return FALSE;

	/* Flags must match. */
	if (!(rule->flags & flag)) return FALSE;

	/* This is a match, but is it supposed to trigger yet? */
	if (rule->skip > 0) {
		rule->skip--;

		return FALSE;
	}

	return TRUE;
}

/*===========================================================================*
 *				rule_find				     *
 *===========================================================================*/
int rule_find(u64_t pos, size_t size, int flag)
{
	/* Find all matching rules, and return a hook mask. */
	struct fbd_rule *rule;
	int i, hooks;

	nr_matches = 0;
	hooks = 0;

	for (i = 0; i < MAX_RULES; i++) {
		rule = &rules[i];

		if (rule->num == 0) continue;

		if (!rule_match(rule, pos, size, flag))
			continue;

		matches[nr_matches++] = rule;

		/* If the rule has a limited lifetime, update it now. */
		if (rule->count > 0) {
			rule->count--;

			/* Disable the rule from future matching. */
			if (rule->count == 0)
				rule->num = 0;
		}

		hooks |= action_mask(rule);
	}

	return hooks;
}

/*===========================================================================*
 *				rule_pre_hook				     *
 *===========================================================================*/
void rule_pre_hook(iovec_t *iov, unsigned *count, size_t *size,
	u64_t *pos)
{
	int i;

	for (i = 0; i < nr_matches; i++)
		if (action_mask(matches[i]) & PRE_HOOK)
			action_pre_hook(matches[i], iov, count, size, pos);
}

/*===========================================================================*
 *				rule_io_hook				     *
 *===========================================================================*/
void rule_io_hook(char *buf, size_t size, u64_t pos, int flag)
{
	int i;

	for (i = 0; i < nr_matches; i++)
		if (action_mask(matches[i]) & IO_HOOK)
			action_io_hook(matches[i], buf, size, pos, flag);
}

/*===========================================================================*
 *				rule_post_hook				     *
 *===========================================================================*/
void rule_post_hook(size_t osize, int *result)
{
	int i;

	for (i = 0; i < nr_matches; i++)
		if (action_mask(matches[i]) & POST_HOOK)
			action_post_hook(matches[i], osize, result);
}
