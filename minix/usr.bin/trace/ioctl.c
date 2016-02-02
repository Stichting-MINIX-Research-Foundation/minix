
#include "inc.h"

#include <sys/ioctl.h>

static char ioctlbuf[IOCPARM_MASK];

static const struct {
	const char *(*name)(unsigned long);
	int (*arg)(struct trace_proc *, unsigned long, void *, int);
	int is_svrctl;
} ioctl_table[] = {
	{ block_ioctl_name,	block_ioctl_arg,	FALSE	},
	{ char_ioctl_name,	char_ioctl_arg,		FALSE	},
	{ net_ioctl_name,	net_ioctl_arg,		FALSE	},
	{ svrctl_name,		svrctl_arg,		TRUE	},
};

/*
 * Print an IOCTL request code, and save certain values in the corresponding
 * process structure in order to be able to print the IOCTL argument.
 */
void
put_ioctl_req(struct trace_proc * proc, const char * name, unsigned long req,
	int is_svrctl)
{
	const char *text;
	size_t size;
	unsigned int i, group, cmd;
	int r, w, big;

	proc->ioctl_index = -1;

	if (valuesonly > 1) {
		put_value(proc, name, "0x%lx", req);

		return;
	}

	/*
	 * Lookups are bruteforce across the IOCTL submodules; they're all
	 * checked.  We could use the group letter but that would create more
	 * issues than it solves.  Our hope is that at least the compiler is
	 * smart about looking up particular codes in each switch statement,
	 * although in the worst case, it's a full O(n) lookup.
	 */
	for (i = 0; i < COUNT(ioctl_table); i++) {
		/* IOCTLs and SVRCTLs are considered different name spaces. */
		if (ioctl_table[i].is_svrctl != is_svrctl)
			continue;

		if ((text = ioctl_table[i].name(req)) != NULL) {
			proc->ioctl_index = i;

			if (valuesonly)
				break;

			put_field(proc, name, text);

			return;
		}
	}

	r = _MINIX_IOCTL_IOR(req);
	w = _MINIX_IOCTL_IOW(req);
	big = _MINIX_IOCTL_BIG(req);
	size = (size_t)(big ? _MINIX_IOCTL_SIZE_BIG(req) : IOCPARM_LEN(req));
	group = big ? 0 : IOCGROUP(req);
	cmd = req & 0xff; /* shockingly there is no macro for this.. */

	/*
	 * Not sure why an entire bit is wasted on IOC_VOID (legacy reasons?),
	 * but since the redundancy is there, we might as well check whether
	 * this is a valid IOCTL request.  Also, we expect the group to be a
	 * printable character.  If either check fails, print just a number.
	 */
	if (((req & IOC_VOID) && (r || w || big || size > 0)) ||
	    (!(req & IOC_VOID) && ((!r && !w) || size == 0)) ||
	    (!big && (group < 32 || group > 127))) {
		put_value(proc, name, "0x%lx", req);

		return;
	}

	if (big) {
		/* For big IOCTLs, "R" becomes before "W" (old MINIX style). */
		put_value(proc, name, "_IO%s%s_BIG(%u,%zu)",
		    r ? "R" : "", w ? "W" : "", cmd, size);
	} else if (IOCGROUP(req) >= 32 && IOCGROUP(req) < 127) {
		/* For normal IOCTLs, "W" comes before "R" (NetBSD style). */
		put_value(proc, name, "_IO%s%s('%c',%u,%zu)",
		    w ? "W" : "", r ? "R" : "", group, cmd, size);
	}
}

/*
 * Print the supplied (out) part of an IOCTL argument, as applicable.  For
 * efficiency reasons, this function assumes that put_ioctl_req() has been
 * called for the corresponding IOCTL already, so that the necessary fields in
 * the given proc structure are set as expected.
 */
int
put_ioctl_arg_out(struct trace_proc * proc, const char * name,
	unsigned long req, vir_bytes addr, int is_svrctl)
{
	size_t size;
	int dir, all;

	dir = (_MINIX_IOCTL_IOW(req) ? IF_OUT : 0) |
	    (_MINIX_IOCTL_IOR(req) ? IF_IN : 0);

	if (dir == 0) {
		proc->ioctl_index = -1; /* no argument to print at all */

		return CT_DONE;
	}

	/* No support for printing big-IOCTL contents just yet. */
	if (valuesonly > 1 || _MINIX_IOCTL_BIG(req) ||
	    proc->ioctl_index == -1) {
		put_ptr(proc, name, addr);

		return CT_DONE;
	}

	assert(proc->ioctl_index >= 0);
	assert((unsigned int)proc->ioctl_index < COUNT(ioctl_table));
	assert(ioctl_table[proc->ioctl_index].is_svrctl == is_svrctl);

	proc->ioctl_flags =
	    ioctl_table[proc->ioctl_index].arg(proc, req, NULL, dir);

	if (proc->ioctl_flags == 0) { /* no argument printing for this IOCTL */
		put_ptr(proc, name, addr);

		proc->ioctl_index = -1; /* forget about the IOCTL handler */

		return CT_DONE;
	}

	/*
	 * If this triggers, the IOCTL handler returns a direction that is not
	 * part of the actual IOCTL, and the handler should be fixed.
	 */
	if (proc->ioctl_flags & ~dir) {
		output_flush(); /* show the IOCTL name for debugging */

		assert(0);
	}

	if (!(proc->ioctl_flags & IF_OUT))
		return CT_NOTDONE;

	size = IOCPARM_LEN(req);

	if (size > sizeof(ioctlbuf) ||
	    mem_get_data(proc->pid, addr, ioctlbuf, size) < 0) {
		put_ptr(proc, name, addr);

		/* There's no harm in trying the _in side later anyhow.. */
		return CT_DONE;
	}

	put_open(proc, name, 0, "{", ", ");

	all = ioctl_table[proc->ioctl_index].arg(proc, req, ioctlbuf, IF_OUT);

	if (!all)
		put_field(proc, NULL, "..");

	put_close(proc, "}");

	return CT_DONE;
}

/*
 * Print the returned (in) part of an IOCTL argument, as applicable.  This
 * function assumes that it is preceded by a call to put_ioctl_arg_out for this
 * process.
 */
void
put_ioctl_arg_in(struct trace_proc * proc, const char * name, int failed,
	unsigned long req, vir_bytes addr, int is_svrctl)
{
	size_t size;
	int all;

	if (valuesonly > 1 || _MINIX_IOCTL_BIG(req) ||
	    proc->ioctl_index == -1) {
		put_result(proc);

		return;
	}

	assert(proc->ioctl_index >= 0);
	assert((unsigned int)proc->ioctl_index < COUNT(ioctl_table));
	assert(ioctl_table[proc->ioctl_index].is_svrctl == is_svrctl);
	assert(proc->ioctl_flags != 0);

	if (proc->ioctl_flags & IF_OUT)
		put_result(proc);
	if (!(proc->ioctl_flags & IF_IN))
		return;

	size = IOCPARM_LEN(req);

	if (failed || size > sizeof(ioctlbuf) ||
	    mem_get_data(proc->pid, addr, ioctlbuf, size) < 0) {
		if (!(proc->ioctl_flags & IF_OUT)) {
			put_ptr(proc, name, addr);
			put_equals(proc);
			put_result(proc);
		} else if (!failed)
			put_field(proc, NULL, "{..}");

		return;
	}

	put_open(proc, name, 0, "{", ", ");

	all = ioctl_table[proc->ioctl_index].arg(proc, req, ioctlbuf, IF_IN);

	if (!all)
		put_field(proc, NULL, "..");

	put_close(proc, "}");

	if (!(proc->ioctl_flags & IF_OUT)) {
		put_equals(proc);
		put_result(proc);
	}
}
