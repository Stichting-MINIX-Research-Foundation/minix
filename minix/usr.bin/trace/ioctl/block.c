
#include "inc.h"

#include <sys/ioctl.h>
#include <minix/partition.h>
#include <sys/mtio.h>

const char *
block_ioctl_name(unsigned long req)
{

	switch (req) {
	NAME(BIOCTRACEBUF);
	NAME(BIOCTRACECTL);
	NAME(BIOCTRACEGET);	/* big IOCTL, not printing argument */
	NAME(DIOCSETP);
	NAME(DIOCGETP);
	NAME(DIOCEJECT);	/* no argument */
	NAME(DIOCTIMEOUT);
	NAME(DIOCOPENCT);
	NAME(DIOCFLUSH);	/* no argument */
	NAME(DIOCGETWC);
	NAME(DIOCSETWC);
	NAME(FBDCADDRULE);
	NAME(FBDCDELRULE);
	NAME(FBDCGETRULE);
	NAME(MIOCRAMSIZE);
	NAME(MTIOCGET);		/* TODO: print argument */
	NAME(MTIOCTOP);		/* TODO: print argument */
	NAME(VNDIOCCLR);
	NAME(VNDIOCGET);
	NAME(VNDIOCSET);
	}

	return NULL;
}

static const struct flags fbd_flags[] = {
	FLAG(FBD_FLAG_READ),
	FLAG(FBD_FLAG_WRITE),
};

static void
put_fbd_action(struct trace_proc * proc, const char * name, int action)
{
	const char *text = NULL;

	if (!valuesonly) {
		switch (action) {
		TEXT(FBD_ACTION_CORRUPT);
		TEXT(FBD_ACTION_ERROR);
		TEXT(FBD_ACTION_MISDIR);
		TEXT(FBD_ACTION_LOSTTORN);
		}
	}

	if (text != NULL)
		put_field(proc, name, text);
	else
		put_value(proc, name, "%d", action);
}

static const struct flags vnd_flags[] = {
	FLAG(VNDIOF_HASGEOM),
	FLAG(VNDIOF_READONLY),
	FLAG(VNDIOF_FORCE),
};

int
block_ioctl_arg(struct trace_proc * proc, unsigned long req, void * ptr,
	int dir)
{
	struct part_geom *part;
	struct fbd_rule *rule;
	struct vnd_ioctl *vnd;
	struct vnd_user *vnu;
	int i;

	switch (req) {
	case BIOCTRACEBUF:
		if (ptr == NULL)
			return IF_OUT;

		put_value(proc, NULL, "%zu", *(size_t *)ptr);
		return IF_ALL;

	case BIOCTRACECTL:
		if (ptr == NULL)
			return IF_OUT;

		i = *(int *)ptr;
		if (!valuesonly && i == BTCTL_START)
			put_field(proc, NULL, "BTCTL_START");
		else if (!valuesonly && i == BTCTL_STOP)
			put_field(proc, NULL, "BTCTL_STOP");
		else
			put_value(proc, NULL, "%d", i);
		return IF_ALL;

	case DIOCSETP:
		if ((part = (struct part_geom *)ptr) == NULL)
			return IF_OUT;

		put_value(proc, "base", "%"PRIu64, part->base);
		put_value(proc, "size", "%"PRIu64, part->size);
		return IF_ALL;

	case DIOCGETP:
		if ((part = (struct part_geom *)ptr) == NULL)
			return IF_IN;

		put_value(proc, "base", "%"PRIu64, part->base);
		put_value(proc, "size", "%"PRIu64, part->size);
		if (verbose > 0) {
			put_value(proc, "cylinders", "%u", part->cylinders);
			put_value(proc, "heads", "%u", part->heads);
			put_value(proc, "sectors", "%u", part->sectors);
			return IF_ALL;
		} else
			return 0;

	case DIOCTIMEOUT:
		/* Print the old timeout only if verbosity is high enough. */
		if (ptr == NULL)
			return IF_OUT | ((verbose > 0) ? IF_IN : 0);

		/* Same action for out and in. */
		put_value(proc, NULL, "%d", *(int *)ptr);
		return IF_ALL;

	case DIOCOPENCT:
		if (ptr == NULL)
			return IF_IN;

		put_value(proc, NULL, "%d", *(int *)ptr);
		return IF_ALL;

	case DIOCSETWC:
	case DIOCGETWC:
		if (ptr == NULL)
			return dir; /* out or in, depending on the request */

		put_value(proc, NULL, "%d", *(int *)ptr);
		return IF_ALL;

	case FBDCDELRULE:
		if (ptr == NULL)
			return IF_OUT;

		put_value(proc, NULL, "%d", *(fbd_rulenum_t *)ptr);
		return IF_ALL;

	case FBDCGETRULE:
		if ((rule = (struct fbd_rule *)ptr) == NULL)
			return IF_OUT | IF_IN;

		if (dir == IF_OUT) {
			put_value(proc, "num", "%d", rule->num);
			return IF_ALL;
		}

		/*
		 * The returned result is the same as what is passed to the
		 * add request, so we can use the same code to print both.
		 */
		/* FALLTHROUGH */
	case FBDCADDRULE:
		if ((rule = (struct fbd_rule *)ptr) == NULL)
			return IF_OUT;

		if (rule->start != 0 || rule->end != 0 || verbose > 0) {
			put_value(proc, "start", "%"PRIu64, rule->start);
			put_value(proc, "end", "%"PRIu64, rule->end);
		}
		if (rule->flags != (FBD_FLAG_READ | FBD_FLAG_WRITE) ||
		    verbose > 0)
			put_flags(proc, "flags", fbd_flags, COUNT(fbd_flags),
			    "0x%x", rule->flags);
		if (rule->skip != 0 || verbose > 0)
			put_value(proc, "skip", "%u", rule->skip);
		if (rule->count != 0 || verbose > 0)
			put_value(proc, "count", "%u", rule->count);
		put_fbd_action(proc, "action", rule->action);

		return 0; /* TODO: optionally print the union fields */

	case MIOCRAMSIZE:
		if (ptr == NULL)
			return IF_OUT;

		put_value(proc, NULL, "%"PRIu32, *(u32_t *)ptr);
		return IF_ALL;

	case VNDIOCSET:
		if ((vnd = (struct vnd_ioctl *)ptr) == NULL)
			return IF_OUT | IF_IN;

		if (dir == IF_OUT) {
			put_value(proc, "vnd_fildes", "%d", vnd->vnd_fildes);
			put_flags(proc, "vnd_flags", vnd_flags,
			    COUNT(vnd_flags), "0x%x", vnd->vnd_flags);
			return 0; /* TODO: print geometry if given */
		} else {
			put_value(proc, "vnd_size", "%"PRIu64, vnd->vnd_size);
			return IF_ALL;
		}

	case VNDIOCCLR:
		if ((vnd = (struct vnd_ioctl *)ptr) == NULL)
			return IF_OUT;

		put_flags(proc, "vnd_flags", vnd_flags, COUNT(vnd_flags),
		    "0x%x", vnd->vnd_flags);
		return IF_ALL;

	case VNDIOCGET:
		if ((vnu = (struct vnd_user *)ptr) == NULL)
			return IF_IN;

		put_value(proc, "vnu_unit", "%d", vnu->vnu_unit);
		put_dev(proc, "vnu_dev", vnu->vnu_dev);
		put_value(proc, "vnu_ino", "%"PRId64, vnu->vnu_ino);
		return IF_ALL;

	default:
		return 0;
	}
}
