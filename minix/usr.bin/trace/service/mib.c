
#include "inc.h"

#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/sched.h>
#include <sys/resource.h>

struct sysctl_tab {
	int id;
	size_t size;
	const struct sysctl_tab *tab;
	int (*proc)(struct trace_proc *, const char *, int, const void *,
	    vir_bytes, size_t);
};
#define NODE(i,t) { .id = i, .size = __arraycount(t), .tab = t }
#define PROC(i,s,p) { .id = i, .size = s, .proc = p }

/*
 * Print CTL_KERN KERN_CLOCKRATE.
 */
static int
put_kern_clockrate(struct trace_proc * proc, const char * name,
	int type __unused, const void * ptr, vir_bytes addr __unused,
	size_t size __unused)
{
	const struct clockinfo *ci;

	ci = (const struct clockinfo *)ptr;

	put_value(proc, "hz", "%d", ci->hz);
	put_value(proc, "tick", "%d", ci->tick);
	if (verbose > 0) {
		put_value(proc, "tickadj", "%d", ci->tickadj);
		put_value(proc, "stathz", "%d", ci->stathz);
		put_value(proc, "profhz", "%d", ci->profhz);
		return TRUE;
	} else
		return FALSE;
}

/*
 * Print CTL_KERN KERN_PROC2.
 */
static int
put_kern_proc2(struct trace_proc * proc, const char * name, int type,
	const void * ptr, vir_bytes addr, size_t size)
{
	const int *mib;
	const char *text;
	unsigned int i;

	if (type == ST_NAME) {
		mib = (const int *)ptr;

		for (i = 0; i < size; i++) {
			text = NULL;

			if (i == 0) {
				switch (mib[i]) {
				case KERN_PROC_ALL: text = "<all>"; break;
				case KERN_PROC_PID: text = "<pid>"; break;
				case KERN_PROC_PGRP: text = "<pgrp>"; break;
				case KERN_PROC_SESSION:
					text = "<session>"; break;
				case KERN_PROC_TTY: text = "<tty>"; break;
				case KERN_PROC_UID: text = "<uid>"; break;
				case KERN_PROC_RUID: text = "<ruid>"; break;
				case KERN_PROC_GID: text = "<gid>"; break;
				case KERN_PROC_RGID: text = "<rgid>"; break;
				}
			} else if (i == 1 && mib[0] == KERN_PROC_TTY) {
				switch ((dev_t)mib[i]) {
				case KERN_PROC_TTY_NODEV:
					text = "<nodev>"; break;
				case KERN_PROC_TTY_REVOKE:
					text = "<revoke>"; break;
				}
			}

			if (!valuesonly && text != NULL)
				put_field(proc, NULL, text);
			else
				put_value(proc, NULL, "%d", mib[i]);
		}

		/*
		 * Save the requested structure length, so that we can later
		 * determine how many elements were returned (see below).
		 */
		proc->sctl_arg = (size == 4) ? mib[2] : 0;

		return 0;
	}

	if (proc->sctl_arg > 0) {
		/* TODO: optionally dump struct kinfo_drivers array */
		put_open(proc, name, 0, "[", ", ");
		if (size > 0)
			put_tail(proc, size / proc->sctl_arg, 0);
		put_close(proc, "]");
	} else
		put_ptr(proc, name, addr);

	return TRUE;
}

/*
 * Print CTL_KERN KERN_PROC_ARGS.
 */
static int
put_kern_proc_args(struct trace_proc * proc, const char * name, int type,
	const void * ptr, vir_bytes addr, size_t size)
{
	const int *mib;
	const char *text;
	unsigned int i;
	int v;

	if (type == ST_NAME) {
		mib = (const int *)ptr;

		for (i = 0; i < size; i++) {
			text = NULL;

			if (i == 1) {
				switch (mib[i]) {
				case KERN_PROC_ARGV: text = "<argv>"; break;
				case KERN_PROC_ENV: text = "<env>"; break;
				case KERN_PROC_NARGV: text = "<nargv>"; break;
				case KERN_PROC_NENV: text = "<nenv>"; break;
				}
			}

			if (!valuesonly && text != NULL)
				put_field(proc, NULL, text);
			else
				put_value(proc, NULL, "%d", mib[i]);
		}

		/* Save the subrequest, so that we can later print data. */
		proc->sctl_arg = (size == 2) ? mib[1] : -999;

		return 0;
	}

	if ((proc->sctl_arg == KERN_PROC_NARGV ||
	    proc->sctl_arg == KERN_PROC_NENV) && size == sizeof(v) &&
	    mem_get_data(proc->pid, addr, &v, sizeof(v)) >= 0) {
		put_open(proc, name, PF_NONAME, "{", ", ");

		put_value(proc, NULL, "%d", v);

		put_close(proc, "}");
	} else
		put_ptr(proc, name, addr);

	return TRUE;
}

/*
 * Print CTL_KERN KERN_CP_TIME.
 */
static int
put_kern_cp_time(struct trace_proc * proc, const char * name __unused,
	int type, const void * ptr, vir_bytes addr __unused, size_t size)
{
	const uint64_t *p;
	unsigned int i;
	const int *mib;

	if (type == ST_NAME) {
		mib = (const int *)ptr;
		for (i = 0; i < size; i++)
			put_value(proc, NULL, "%d", mib[i]);

		return 0;
	}

	p = (const uint64_t *)ptr;

	/* TODO: support for multi-CPU results */
	for (i = 0; i < CPUSTATES; i++)
		put_value(proc, NULL, "%"PRIu64, p[i]);

	return TRUE;
}

/*
 * Print CTL_KERN KERN_CONSDEV.
 */
static int
put_kern_consdev(struct trace_proc * proc, const char * name,
	int type __unused, const void * ptr, vir_bytes addr __unused,
	size_t size __unused)
{

	put_dev(proc, NULL, *(const dev_t *)ptr);

	return TRUE;
}

/*
 * Print CTL_KERN KERN_DRIVERS.
 */
static int
put_kern_drivers(struct trace_proc * proc, const char * name,
	int type __unused, const void * ptr __unused, vir_bytes addr __unused,
	size_t size)
{

	/* TODO: optionally dump struct kinfo_drivers array */
	put_open(proc, name, 0, "[", ", ");
	if (size > 0)
		put_tail(proc, size / sizeof(struct kinfo_drivers), 0);
	put_close(proc, "]");

	return TRUE;
}

/*
 * Print CTL_KERN KERN_BOOTTIME.
 */
static int
put_kern_boottime(struct trace_proc * proc, const char * name,
	int type __unused, const void * ptr __unused, vir_bytes addr,
	size_t size)
{

	if (size == sizeof(struct timeval))
		put_struct_timeval(proc, name, 0, addr);
	else
		put_ptr(proc, name, addr);

	return TRUE;
}

/*
 * Print CTL_KERN KERN_SYSVIPC KERN_SYSVIPC_INFO.
 */
static int
put_kern_sysvipc_info(struct trace_proc * proc, const char * name,
	int type, const void * ptr, vir_bytes addr, size_t size)
{
	const int *mib;
	const char *text;
	unsigned int i;

	/*
	 * TODO: print the obtained structure(s).  For now we are just
	 * concerned with the name components.
	 */
	if (type != ST_NAME) {
		put_ptr(proc, name, addr);

		return TRUE;
	}

	mib = (const int *)ptr;

	for (i = 0; i < size; i++) {
		text = NULL;

		if (i == 0) {
			switch (mib[i]) {
			case KERN_SYSVIPC_SEM_INFO: text = "<sem>"; break;
			case KERN_SYSVIPC_SHM_INFO: text = "<shm>"; break;
			case KERN_SYSVIPC_MSG_INFO: text = "<msg>"; break;
			}
		}

		if (!valuesonly && text != NULL)
			put_field(proc, NULL, text);
		else
			put_value(proc, NULL, "%d", mib[i]);
	}

	return 0;
}

/* The CTL_KERN KERN_SYSVIPC table. */
static const struct sysctl_tab kern_sysvipc_tab[] = {
	PROC(KERN_SYSVIPC_INFO, 0, put_kern_sysvipc_info),
};

/* The CTL_KERN table. */
static const struct sysctl_tab kern_tab[] = {
	PROC(KERN_CLOCKRATE, sizeof(struct clockinfo), put_kern_clockrate),
	PROC(KERN_PROC2, 0, put_kern_proc2),
	PROC(KERN_PROC_ARGS, 0, put_kern_proc_args),
	PROC(KERN_CP_TIME, sizeof(uint64_t) * CPUSTATES, put_kern_cp_time),
	PROC(KERN_CONSDEV, sizeof(dev_t), put_kern_consdev),
	PROC(KERN_DRIVERS, 0, put_kern_drivers),
	NODE(KERN_SYSVIPC, kern_sysvipc_tab),
	PROC(KERN_BOOTTIME, 0, put_kern_boottime),
};

/*
 * Print CTL_VM VM_LOADAVG.
 */
static int
put_vm_loadavg(struct trace_proc * proc, const char * name __unused,
	int type __unused, const void * ptr, vir_bytes addr __unused,
	size_t size __unused)
{
	const struct loadavg *loadavg;
	unsigned int i;

	loadavg = (const struct loadavg *)ptr;

	put_open(proc, "ldavg", 0, "{", ", ");

	for (i = 0; i < __arraycount(loadavg->ldavg); i++)
		put_value(proc, NULL, "%"PRIu32, loadavg->ldavg[i]);

	put_close(proc, "}");

	if (verbose > 0) {
		put_value(proc, "fscale", "%ld", loadavg->fscale);

		return TRUE;
	} else
		return FALSE;
}

/* The CTL_VM table. */
static const struct sysctl_tab vm_tab[] = {
	PROC(VM_LOADAVG, sizeof(struct loadavg), put_vm_loadavg),
};

/*
 * Print CTL_NET PF_ROUTE 0.
 */
static int
put_net_route_rtable(struct trace_proc * proc, const char * name,
	int type, const void * ptr, vir_bytes addr, size_t size)
{
	const int *mib;
	const char *text;
	unsigned int i;

	/*
	 * TODO: print the obtained structure(s).  For now we are just
	 * concerned with the name components.
	 */
	if (type != ST_NAME) {
		put_ptr(proc, name, addr);

		return TRUE;
	}

	mib = (const int *)ptr;

	for (i = 0; i < size; i++) {
		text = NULL;

		switch (i) {
		case 0:
			switch (mib[i]) {
			case AF_UNSPEC: text = "<all>"; break;
			case AF_LINK: text = "<link>"; break;
			case AF_INET: text = "<inet>"; break;
			case AF_INET6: text = "<inet6>"; break;
			/* TODO: add more address families here */
			}
			break;
		case 1:
			switch (mib[i]) {
			case NET_RT_DUMP: text = "<dump>"; break;
			case NET_RT_FLAGS: text = "<flags>"; break;
			case NET_RT_IFLIST: text = "<iflist>"; break;
			}
			break;
		case 2:
			if (mib[1] == NET_RT_IFLIST && mib[i] == 0)
				text = "<all>";
		}

		if (!valuesonly && text != NULL)
			put_field(proc, NULL, text);
		else
			put_value(proc, NULL, "%d", mib[i]);
	}

	return 0;
}

/* The CTL_NET PF_ROUTE table. */
static const struct sysctl_tab net_route_tab[] = {
	PROC(0, 0, put_net_route_rtable),
};

/* The CTL_NET table. */
static const struct sysctl_tab net_tab[] = {
	NODE(PF_ROUTE, net_route_tab),
};

/* The top-level table, which is indexed by identifier. */
static const struct sysctl_tab root_tab[] = {
	[CTL_KERN]	= NODE(0, kern_tab),
	[CTL_VM]	= NODE(0, vm_tab),
	[CTL_NET]	= NODE(0, net_tab),
};

/*
 * This buffer should be large enough to avoid having to perform dynamic
 * allocation in all but highly exceptional cases.  The CTL_KERN subtree is
 * currently the largest, so we base the buffer size on its length.
 * TODO: merge this buffer with ioctlbuf.
 */
static char sysctlbuf[sizeof(struct sysctlnode) * KERN_MAXID];

static const struct flags sysctl_flags[] = {
	FLAG_MASK(SYSCTL_VERS_MASK, SYSCTL_VERS_0),
	FLAG_MASK(SYSCTL_VERS_MASK, SYSCTL_VERSION),
#define SYSCTL_VER_ENTRIES 2 /* the first N entries are for SYSCTL_VERS_MASK */
	FLAG(CTLFLAG_UNSIGNED),
	FLAG(CTLFLAG_OWNDESC),
	FLAG(CTLFLAG_MMAP),
	FLAG(CTLFLAG_ALIAS),
	FLAG(CTLFLAG_ANYNUMBER),
	FLAG(CTLFLAG_ROOT),
	FLAG(CTLFLAG_HEX),
	FLAG(CTLFLAG_IMMEDIATE),
	FLAG(CTLFLAG_OWNDATA),
	FLAG(CTLFLAG_HIDDEN),
	FLAG(CTLFLAG_PERMANENT),
	FLAG(CTLFLAG_PRIVATE),
	FLAG(CTLFLAG_ANYWRITE),
	FLAG_MASK(CTLFLAG_READWRITE, CTLFLAG_READONLY),
	FLAG_MASK(CTLFLAG_READWRITE, CTLFLAG_READWRITE),
	FLAG_MASK(SYSCTL_TYPEMASK, CTLTYPE_NODE),
	FLAG_MASK(SYSCTL_TYPEMASK, CTLTYPE_INT),
	FLAG_MASK(SYSCTL_TYPEMASK, CTLTYPE_STRING),
	FLAG_MASK(SYSCTL_TYPEMASK, CTLTYPE_QUAD),
	FLAG_MASK(SYSCTL_TYPEMASK, CTLTYPE_STRUCT),
	FLAG_MASK(SYSCTL_TYPEMASK, CTLTYPE_BOOL),
};

/*
 * Print the immediate value of a sysctl node.
 */
static void
put_sysctl_imm(struct trace_proc * proc, struct sysctlnode * scn, int use_name)
{
	const char *name;

	name = NULL;

	switch (SYSCTL_TYPE(scn->sysctl_flags)) {
	case CTLTYPE_INT:
		if (use_name)
			name = "sysctl_idata";
		if (scn->sysctl_flags & CTLFLAG_HEX)
			put_value(proc, name, "0x%x", scn->sysctl_idata);
		else if (scn->sysctl_flags & CTLFLAG_UNSIGNED)
			put_value(proc, name, "%u", scn->sysctl_idata);
		else
			put_value(proc, name, "%d", scn->sysctl_idata);
		break;
	case CTLTYPE_BOOL:
		if (use_name)
			name = "sysctl_bdata";
		put_field(proc, name, (scn->sysctl_bdata) ? "true" : "false");
		break;
	case CTLTYPE_QUAD:
		if (use_name)
			name = "sysctl_qdata";
		if (scn->sysctl_flags & CTLFLAG_HEX)
			put_value(proc, name, "0x%"PRIx64, scn->sysctl_qdata);
		else
			put_value(proc, name, "%"PRIu64, scn->sysctl_qdata);
		break;
	}
}

/*
 * Printer for CTL_QUERY data.
 */
static int
put_sysctl_query(struct trace_proc * proc, const char * name, int type,
	const void * data __unused, vir_bytes addr, size_t size)
{
	struct sysctlnode scn;

	if (type == ST_NEWP) {
		if (!put_open_struct(proc, name, 0, addr, &scn, sizeof(scn)))
			return TRUE;

		/* Print just the protocol version, that's all there is. */
		if (verbose > 1)
			put_flags(proc, "sysctl_flags", sysctl_flags,
			    SYSCTL_VER_ENTRIES, "0x%x", scn.sysctl_flags);

		put_close_struct(proc, FALSE /*all*/);
	} else {
		/* TODO: optionally dump struct sysctlnode array */
		put_open(proc, name, 0, "[", ", ");
		if (size > 0)
			put_tail(proc, size / sizeof(scn), 0);
		put_close(proc, "]");
	}

	return TRUE;
}

/*
 * Printer for CTL_CREATE data.
 */
static int
put_sysctl_create(struct trace_proc * proc, const char * name, int type,
	const void * data __unused, vir_bytes addr, size_t size)
{
	struct sysctlnode scn;

	if (!put_open_struct(proc, name, 0, addr, &scn, sizeof(scn)))
		return TRUE;

	if (type == ST_NEWP)
		put_flags(proc, "sysctl_flags", sysctl_flags,
		    COUNT(sysctl_flags), "0x%x", scn.sysctl_flags);

	if (scn.sysctl_num == CTL_CREATE && type == ST_NEWP && !valuesonly)
		put_field(proc, "sysctl_num", "CTL_CREATE");
	else
		put_value(proc, "sysctl_num", "%d", scn.sysctl_num);

	if (type == ST_NEWP) {
		put_buf(proc, "sysctl_name", PF_LOCADDR | PF_STRING,
		    (vir_bytes)scn.sysctl_name, sizeof(scn.sysctl_name));
	}
	if (scn.sysctl_ver != 0 && verbose > 0)
		put_value(proc, "sysctl_ver", "%u", scn.sysctl_ver);

	if (type == ST_NEWP) {
		if (scn.sysctl_flags & CTLFLAG_IMMEDIATE)
			put_sysctl_imm(proc, &scn, TRUE /*use_name*/);

		switch (SYSCTL_TYPE(scn.sysctl_flags)) {
		case CTLTYPE_NODE:
			break;
		case CTLTYPE_STRING:
			if (scn.sysctl_data != NULL)
				put_buf(proc, "sysctl_data", PF_STRING,
				    (vir_bytes)scn.sysctl_data,
				    (scn.sysctl_size > 0) ? scn.sysctl_size :
				    SSIZE_MAX /* hopefully it stops early */);
			if (scn.sysctl_data != NULL || verbose == 0)
				break;
			/* FALLTHROUGH */
		default:
			if (!(scn.sysctl_flags & CTLFLAG_IMMEDIATE) &&
			    verbose > 0)
				put_ptr(proc, "sysctl_data",
				    (vir_bytes)scn.sysctl_data);
			break;
		}

		if (SYSCTL_TYPE(scn.sysctl_flags) == CTLTYPE_STRUCT ||
		    verbose > 0)
			put_value(proc, "sysctl_size", "%zu", scn.sysctl_size);
	}

	put_close_struct(proc, FALSE /*all*/);

	return TRUE;
}

/*
 * Printer for CTL_DESTROY data.
 */
static int
put_sysctl_destroy(struct trace_proc * proc, const char * name, int type,
	const void * data __unused, vir_bytes addr, size_t size)
{
	struct sysctlnode scn;

	if (!put_open_struct(proc, name, 0, addr, &scn, sizeof(scn)))
		return TRUE;

	if (type == ST_NEWP) {
		put_value(proc, "sysctl_num", "%d", scn.sysctl_num);
		if (scn.sysctl_name[0] != '\0')
			put_buf(proc, "sysctl_name", PF_LOCADDR | PF_STRING,
			    (vir_bytes)scn.sysctl_name,
			    sizeof(scn.sysctl_name));
		if (scn.sysctl_ver != 0 && verbose > 0)
			put_value(proc, "sysctl_ver", "%u", scn.sysctl_ver);
	}

	put_close_struct(proc, FALSE /*all*/);

	return TRUE;
}

/*
 * Printer for CTL_CREATE data.
 */
static int
put_sysctl_describe(struct trace_proc * proc, const char * name, int type,
	const void * data __unused, vir_bytes addr, size_t size)
{
	struct sysctlnode scn;

	if (type == ST_NEWP) {
		if (!put_open_struct(proc, name, 0, addr, &scn, sizeof(scn)))
			return TRUE;

		/* Print just the protocol version, that's all there is. */
		if (verbose > 1)
			put_flags(proc, "sysctl_flags", sysctl_flags,
			    SYSCTL_VER_ENTRIES, "0x%x", scn.sysctl_flags);

		put_value(proc, "sysctl_num", "%d", scn.sysctl_num);

		if (scn.sysctl_desc != NULL)
			put_buf(proc, "sysctl_desc", PF_STRING,
			    (vir_bytes)scn.sysctl_desc, 1024 /*no constant!*/);
		else if (verbose > 0)
			put_ptr(proc, "sysctl_desc",
			    (vir_bytes)scn.sysctl_desc);

		put_close_struct(proc, FALSE /*all*/);
	} else {
		/* TODO: optionally dump struct sysctldesc array */
		put_field(proc, name, (size == 0) ? "[]" : "[..]");
	}

	return TRUE;
}

/*
 * Printer for generic data, using the node flags stored in proc->sysctl_flags.
 */
static int
put_sysctl_generic(struct trace_proc * proc, const char * name, int type,
	const void * data __unused, vir_bytes addr, size_t size)
{
	struct sysctlnode scn;
	void *ptr;
	size_t len;

	switch (SYSCTL_TYPE(proc->sctl_flags)) {
	case CTLTYPE_STRING:
		put_buf(proc, name, PF_STRING, addr, size);
		return TRUE;
	case CTLTYPE_INT:
		ptr = &scn.sysctl_idata;
		len = sizeof(scn.sysctl_idata);
		break;
	case CTLTYPE_BOOL:
		ptr = &scn.sysctl_bdata;
		len = sizeof(scn.sysctl_bdata);
		break;
	case CTLTYPE_QUAD:
		ptr = &scn.sysctl_qdata;
		len = sizeof(scn.sysctl_qdata);
		break;
	case CTLTYPE_STRUCT:
	default:
		ptr = NULL;
		len = 0;
		break;
	}

	if (ptr == NULL || len != size ||
	    mem_get_data(proc->pid, addr, ptr, len) < 0) {
		put_ptr(proc, name, addr);
		return TRUE;
	}

	put_open(proc, name, PF_NONAME, "{", ", ");

	scn.sysctl_flags = proc->sctl_flags;

	put_sysctl_imm(proc, &scn, FALSE);

	put_close(proc, "}");

	return TRUE;
}

/*
 * Obtain information about a particular node 'id' in the node directory
 * identified by the MIB path 'name' (length 'namelen').  Return TRUE if the
 * node was found, in which case it is copied into 'scnp'.  Return FALSE if the
 * node was not found or another error occurred.
 */
static int
get_sysctl_node(const int * name, unsigned int namelen, int id,
	struct sysctlnode * scnp)
{
	struct sysctlnode *scn, *escn, *fscn;
	char *buf;
	size_t len, elen;
	int r, mib[CTL_MAXNAME];

	assert(namelen < CTL_MAXNAME);
	assert(id >= 0);

	/* Query the parent, first using our static buffer for the results. */
	memcpy(mib, name, sizeof(mib[0]) * namelen);
	mib[namelen] = CTL_QUERY;
	len = sizeof(sysctlbuf);
	r = sysctl(mib, namelen + 1, sysctlbuf, &len, NULL, 0);
	if (r == -1 && (errno != ENOMEM || len == 0))
		return FALSE;

	/* Even with partial results, check if we already found the node. */
	elen = MIN(len, sizeof(sysctlbuf));
	scn = (struct sysctlnode *)sysctlbuf;
	escn = (struct sysctlnode *)&sysctlbuf[elen];
	fscn = NULL; /* pointer to the node once found, NULL until then */
	for (; scn < escn && fscn == NULL; scn++)
		if (scn->sysctl_num == id)
			fscn = scn;

	/* If our buffer was too small, use a temporary buffer. */
	if (fscn == NULL && r == -1) {
		if ((buf = malloc(len)) == NULL)
			return FALSE;
		if (sysctl(mib, namelen, buf, &len, NULL, 0) == 0) {
			scn = (struct sysctlnode *)sysctlbuf;
			escn = (struct sysctlnode *)&sysctlbuf[len];
			for (; scn < escn && fscn != NULL; scn++)
				if (scn->sysctl_num == id)
					fscn = scn;
		}
		free(buf);
	}

	if (fscn != NULL) {
		memcpy(scnp, fscn, sizeof(*scnp));
		return TRUE;
	} else
		return FALSE;
}

/*
 * Print the name string of one level of a sysctl(2) name, while also gathering
 * information about the target node.  Return 1 if name interpretation should
 * continue as before, meaning this function will also be called for the next
 * name component (if any).  Return 0 if the rest of the name should be printed
 * as numbers, without interpretation.  Return -1 if printing the name is now
 * complete.
 */
static int
put_sysctl_namestr(struct trace_proc * proc, const int * name,
	unsigned int namelen, unsigned int n, int all,
	const struct sysctl_tab ** sctp)
{
	const struct sysctl_tab *sct;
	struct sysctlnode scn;
	const char *namestr;
	int i, r, id, is_last;

	assert(n < namelen);

	id = name[n];
	is_last = (n == namelen - 1 && all);
	namestr = NULL;

	/* Negative identifiers are meta-identifiers. */
	if (id < 0) {
		switch (id) {
		case CTL_EOL:		namestr = "<eol>";		break;
		case CTL_QUERY:		namestr = "<query>";		break;
		case CTL_CREATE:	namestr = "<create>";		break;
		case CTL_CREATESYM:	namestr = "<createsym>";	break;
		case CTL_DESTROY:	namestr = "<destroy>";		break;
		case CTL_MMAP:		namestr = "<mmap>";		break;
		case CTL_DESCRIBE:	namestr = "<describe>";		break;
		}

		/* For some of them, we can print their parameters. */
		if (is_last) {
			switch (id) {
			case CTL_QUERY:
				proc->sctl_proc = put_sysctl_query;
				break;
			case CTL_CREATE:
				proc->sctl_proc = put_sysctl_create;
				break;
			case CTL_DESTROY:
				proc->sctl_proc = put_sysctl_destroy;
				break;
			case CTL_DESCRIBE:
				proc->sctl_proc = put_sysctl_describe;
				break;
			}
		}

		/*
		 * Meta-identifiers are allowed only at the very end of a name,
		 * so if anything follows a meta-identifier, there is no good
		 * way to interpret it.  We just print numbers.
		 */
		r = 0;
	} else if (get_sysctl_node(name, n, id, &scn)) {
		/*
		 * For regular identifiers, first see if we have a callback
		 * function that does the interpretation.  The use of the
		 * callback function depends on whether the current node is of
		 * type CTLTYPE_NODE: if it is, the callback function is
		 * responsible for printing the rest of the name (and we return
		 * -1 here after we are done, #1); if it isn't, then we just
		 * use the callback function to interpret the node value (#2).
		 * If we do not have a callback function, but the current node
		 * is of type CTLTYPE_NODE *and* has a non-NULL callback
		 * function registered in the MIB service, the remote callback
		 * function would interpret the rest of the name, so we simply
		 * print the rest of the name as numbers (returning 0 once we
		 * are done, #3).  Without a MIB-service callback function,
		 * such nodes are just taken as path components and thus we
		 * return 1 to continue resolution (#4).  Finally, if we do not
		 * have a callback function, and the current node is a data
		 * node (i.e., *not* of type CTLTYPE_NODE), we try to interpret
		 * it generically if it is the last component (#5), or we give
		 * up and just print numbers otherwise (#6).
		 */

		/* Okay, so start by looking up the node in our own tables. */
		sct = NULL;
		if (n == 0) {
			/* The top level is ID-indexed for performance. */
			if ((unsigned int)id < __arraycount(root_tab))
				*sctp = &root_tab[id];
			else
				*sctp = NULL;
		} else if (*sctp != NULL) {
			/* Other levels are searched, because of sparseness. */
			sct = (*sctp)->tab; /* NULL if missing or leaf */
			for (i = (int)(*sctp)->size; sct != NULL && i > 0;
			    i--, sct++)
				if (sct->id == id)
					break;
			if (i == 0)
				sct = NULL;
			*sctp = sct;
		}

		/* Now determine what to do. */
		if (SYSCTL_TYPE(scn.sysctl_flags) == CTLTYPE_NODE) {
			if (sct != NULL && sct->proc != NULL) {
				proc->sctl_size = sct->size;
				proc->sctl_proc = sct->proc;
				r = -1; /* #1 */
			} else if (scn.sysctl_func != NULL)
				r = 0; /* #3 */
			else
				r = 1; /* #4 */
		} else {
			if (!is_last)
				r = 0; /* #6 */
			else if (sct != NULL && sct->proc != NULL) {
				/* A nonzero size must match the node size. */
				if (sct->size == 0 ||
				    sct->size == scn.sysctl_size) {
					proc->sctl_size = sct->size;
					proc->sctl_proc = sct->proc;
				}
				r = 0; /* #2 */
			} else {
				proc->sctl_flags = scn.sysctl_flags;
				proc->sctl_proc = put_sysctl_generic;
				r = 0; /* #5 */
			}
		}

		namestr = scn.sysctl_name;
	} else {
		/*
		 * The node was not found.  This basically means that we will
		 * not be able to get any information about deeper nodes
		 * either.  We do not even try: just print numbers.
		 */
		r = 0;
	}

	if (!valuesonly && namestr != NULL)
		put_field(proc, NULL, namestr);
	else
		put_value(proc, NULL, "%d", id);

	/*
	 * Did we determine that the rest of the name should be printed by the
	 * callback function?  Then we might as well make that happen.  The
	 * abuse of the parameter types is not great, oh well.
	 */
	if (r == -1)
		(void)proc->sctl_proc(proc, NULL, ST_NAME, &name[n + 1], 0,
		    namelen - n - 1);

	return r;
}

/*
 * Print the sysctl(2) name parameter, and gather information needed to print
 * the oldp and newp parameters later.
 */
static void
put_sysctl_name(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, unsigned int namelen)
{
	const struct sysctl_tab *sct = NULL;
	int r, all, namebuf[CTL_MAXNAME];
	unsigned int n;

	if (namelen > CTL_MAXNAME) {
		namelen = CTL_MAXNAME;
		all = 0;
	} else
		all = 1;

	if ((flags & PF_FAILED) || valuesonly > 1 || namelen > CTL_MAXNAME ||
	    (namelen > 0 && !(flags & PF_LOCADDR) &&
	    mem_get_data(proc->pid, addr, namebuf,
	    namelen * sizeof(namebuf[0])) < 0)) {
		if (flags & PF_LOCADDR)
			put_field(proc, name, "&..");
		else
			put_ptr(proc, name, addr);
		return;
	} else if (namelen > 0 && (flags & PF_LOCADDR))
		memcpy(namebuf, (void *)addr, sizeof(namebuf[0]) * namelen);

	/*
	 * Print the path name of the node as possible, and find information
	 * about the target node as we go along.  See put_sysctl_namestr() for
	 * the meaning of 'r'.
	 */
	put_open(proc, name, PF_NONAME, "[", ".");
	for (n = 0, r = 1; n < namelen; n++) {
		if (r == 1) {
			if ((r = put_sysctl_namestr(proc, namebuf, namelen, n,
			    all, &sct)) < 0)
				break;
		} else
			put_value(proc, NULL, "%d", namebuf[n]);
	}
	if (!all)
		put_field(proc, NULL, "..");
	put_close(proc, "]");
}

/*
 * Print the sysctl(2) oldp or newp parameter.  PF_ALT means that the given
 * parameter is newp rather than oldp, in which case PF_FAILED will not be set.
 */
static void
put_sysctl_data(struct trace_proc * proc, const char * name, int flags,
	vir_bytes addr, size_t len)
{
	char *ptr;
	int type, all;

	if ((flags & PF_FAILED) || addr == 0 || valuesonly > 1 ||
	    proc->sctl_proc == NULL || proc->sctl_size > sizeof(sysctlbuf) ||
	    (proc->sctl_size > 0 && (proc->sctl_size != len ||
	    mem_get_data(proc->pid, addr, sysctlbuf, proc->sctl_size) < 0))) {
		put_ptr(proc, name, addr);
		return;
	}

	type = (flags & PF_ALT) ? ST_NEWP : ST_OLDP;
	ptr = (proc->sctl_size > 0) ? sysctlbuf : NULL;

	/*
	 * The rough idea here: we have a "simple" mode and a "flexible" mode,
	 * depending on whether a size was specified in our table.  For the
	 * simple mode, we only call the callback function when we have been
	 * able to copy in the data.  A surrounding {} block will be printed
	 * automatically, the callback function only has to print the data
	 * fields.  The simple mode is basically for structures.  In contrast,
	 * the flexible mode leaves both the copying and the printing entirely
	 * to the callback function, which thus may print the pointer on copy
	 * failure (in which case the surrounding {}s would get in the way).
	 */
	if (ptr != NULL)
		put_open(proc, name, 0, "{", ", ");

	all = proc->sctl_proc(proc, name, type, ptr, addr, len);

	if (ptr != NULL) {
		if (all == FALSE)
			put_field(proc, NULL, "..");
		put_close(proc, "}");
	}
}

static int
mib_sysctl_out(struct trace_proc * proc, const message * m_out)
{
	unsigned int namelen;

	/* Reset the sysctl-related state. */
	proc->sctl_flags = 0;
	proc->sctl_size = 0;
	proc->sctl_proc = NULL;
	proc->sctl_arg = 0;

	namelen = m_out->m_lc_mib_sysctl.namelen;

	/* As part of processing the name, we initialize the state. */
	if (namelen <= CTL_SHORTNAME)
		put_sysctl_name(proc, "name", PF_LOCADDR,
		    (vir_bytes)&m_out->m_lc_mib_sysctl.name, namelen);
	else
		put_sysctl_name(proc, "name", 0, m_out->m_lc_mib_sysctl.namep,
		    namelen);

	put_value(proc, "namelen", "%u", namelen);

	if (m_out->m_lc_mib_sysctl.oldp == 0 || valuesonly > 1) {
		put_sysctl_data(proc, "oldp", 0,
		    m_out->m_lc_mib_sysctl.oldp,
		    m_out->m_lc_mib_sysctl.oldlen);
		/* If oldp is NULL, oldlen may contain garbage; don't print. */
		if (m_out->m_lc_mib_sysctl.oldp != 0)
			put_value(proc, "oldlen", "%zu",    /* {%zu} is more */
			    m_out->m_lc_mib_sysctl.oldlen); /* correct..     */
		else
			put_value(proc, "oldlen", "%d", 0);
		put_sysctl_data(proc, "newp", PF_ALT,
		    m_out->m_lc_mib_sysctl.newp,
		    m_out->m_lc_mib_sysctl.newlen);
		put_value(proc, "newlen", "%zu",
		    m_out->m_lc_mib_sysctl.newlen);
		return CT_DONE;
	} else
		return CT_NOTDONE;
}

static void
mib_sysctl_in(struct trace_proc * proc, const message * m_out,
	const message * m_in, int failed)
{
	int err;

	if (m_out->m_lc_mib_sysctl.oldp != 0 && valuesonly <= 1) {
		put_sysctl_data(proc, "oldp", failed,
		    m_out->m_lc_mib_sysctl.oldp,
		    m_in->m_mib_lc_sysctl.oldlen /* the returned length */);
		put_value(proc, "oldlen", "%zu", /* {%zu} is more correct.. */
		    m_out->m_lc_mib_sysctl.oldlen);
		put_sysctl_data(proc, "newp", PF_ALT,
		    m_out->m_lc_mib_sysctl.newp,
		    m_out->m_lc_mib_sysctl.newlen);
		put_value(proc, "newlen", "%zu",
		    m_out->m_lc_mib_sysctl.newlen);
		put_equals(proc);
	}

	put_result(proc);

	/*
	 * We want to print the returned old length in the following cases:
	 * 1. the call succeeded, the old pointer was NULL, and no new data was
	 *    supplied;
	 * 2. the call succeeded, the old pointer was not NULL, and the
	 *    returned old length is different from the supplied old length.
	 * 3. the call failed with ENOMEM or EEXIST, and the old pointer was
	 *    not NULL (an undocumented NetBSD feature, used by sysctl(8)).
	 */
	if (/*#1*/ (!failed && m_out->m_lc_mib_sysctl.oldp == 0 &&
	    (m_out->m_lc_mib_sysctl.newp == 0 ||
	    m_out->m_lc_mib_sysctl.newlen == 0)) ||
	    /*#2*/ (!failed && m_out->m_lc_mib_sysctl.oldp != 0 &&
	    m_out->m_lc_mib_sysctl.oldlen != m_in->m_mib_lc_sysctl.oldlen) ||
	    /*#3*/ (failed && call_errno(proc, &err) &&
	    (err == ENOMEM || err == EEXIST) &&
	    m_out->m_lc_mib_sysctl.oldp != 0)) {
		put_open(proc, NULL, 0, "(", ", ");
		put_value(proc, "oldlen", "%zu", m_in->m_mib_lc_sysctl.oldlen);
		put_close(proc, ")");
	}
}

#define MIB_CALL(c) [((MIB_ ## c) - MIB_BASE)]

static const struct call_handler mib_map[] = {
	MIB_CALL(SYSCTL) = HANDLER("sysctl", mib_sysctl_out, mib_sysctl_in),
};

const struct calls mib_calls = {
	.endpt = MIB_PROC_NR,
	.base = MIB_BASE,
	.map = mib_map,
	.count = COUNT(mib_map)
};
