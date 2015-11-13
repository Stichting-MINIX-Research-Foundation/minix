
#include "inc.h"

#include <minix/rs.h>

static const struct flags rss_flags[] = {
	FLAG(RSS_COPY),
	FLAG(RSS_REUSE),
	FLAG(RSS_NOBLOCK),
	FLAG(RSS_REPLICA),
	FLAG(RSS_SELF_LU),
	FLAG(RSS_SYS_BASIC_CALLS),
	FLAG(RSS_VM_BASIC_CALLS),
	FLAG(RSS_NO_BIN_EXP),
};

static void
put_struct_rs_start(struct trace_proc * proc, const char * name,
	vir_bytes addr)
{
	struct rs_start buf;

	if (!put_open_struct(proc, name, 0, addr, &buf, sizeof(buf)))
		return;

	if (verbose > 0)
		put_flags(proc, "rss_flags", rss_flags, COUNT(rss_flags),
		    "0x%x", buf.rss_flags);
	put_buf(proc, "rss_cmd", 0, (vir_bytes)buf.rss_cmd, buf.rss_cmdlen);
	put_buf(proc, "rss_progname", 0, (vir_bytes)buf.rss_progname,
	    buf.rss_prognamelen);
	put_buf(proc, "rss_label", 0, (vir_bytes)buf.rss_label.l_addr,
	    buf.rss_label.l_len);
	if (verbose > 0 || buf.rss_major != 0)
		put_value(proc, "rss_major", "%d", buf.rss_major);
	if (verbose > 0 || buf.devman_id != 0)
		put_value(proc, "devman_id", "%d", buf.devman_id);
	put_value(proc, "rss_uid", "%u", buf.rss_uid);
	if (verbose > 0) {
		put_endpoint(proc, "rss_sigmgr", buf.rss_sigmgr);
		put_endpoint(proc, "rss_scheduler", buf.rss_sigmgr);
	}
	if (verbose > 1) {
		put_value(proc, "rss_priority", "%d", buf.rss_priority);
		put_value(proc, "rss_quantum", "%d", buf.rss_quantum);
	}
	if (verbose > 0) {
		put_value(proc, "rss_period", "%ld", buf.rss_period);
		put_buf(proc, "rss_script", 0, (vir_bytes)buf.rss_script,
		    buf.rss_scriptlen);
	}

	put_close_struct(proc, FALSE /*all*/); /* TODO: the remaining fields */
}

/* This function is shared between rs_up and rs_edit. */
static int
rs_up_out(struct trace_proc * proc, const message * m_out)
{

	put_struct_rs_start(proc, "addr", (vir_bytes)m_out->m_rs_req.addr);

	return CT_DONE;
}

/*
 * This function is shared between rs_down, rs_refresh, rs_restart, and
 * rs_clone.
 */
static int
rs_label_out(struct trace_proc * proc, const message * m_out)
{

	/*
	 * We are not using PF_STRING here, because unlike in most places
	 * (including rs_lookup), the string length does not include the
	 * terminating NULL character.
	 */
	put_buf(proc, "label", 0, (vir_bytes)m_out->m_rs_req.addr,
	    m_out->m_rs_req.len);

	return CT_DONE;
}

static int
rs_update_out(struct trace_proc * proc, const message * m_out)
{

	/*
	 * FIXME: this is a value from the wrong message union, and that is
	 * actually a minix bug.
	 */
	put_struct_rs_start(proc, "addr", (vir_bytes)m_out->m_rs_req.addr);

	/* TODO: interpret these fields */
	put_value(proc, "state", "%d", m_out->m_rs_update.state);
	put_value(proc, "maxtime", "%d", m_out->m_rs_update.prepare_maxtime);

	return CT_DONE;
}

static int
rs_lookup_out(struct trace_proc * proc, const message * m_out)
{

	put_buf(proc, "label", PF_STRING, (vir_bytes)m_out->m_rs_req.name,
	    m_out->m_rs_req.name_len);

	return CT_DONE;
}

static void
rs_lookup_in(struct trace_proc * proc, const message * __unused m_out,
	const message * m_in, int failed)
{

	if (!failed)
		put_endpoint(proc, NULL, m_in->m_rs_req.endpoint);
	else
		put_result(proc);
}

#define RS_CALL(c) [((RS_ ## c) - RS_RQ_BASE)]

static const struct call_handler rs_map[] = {
	RS_CALL(UP) = HANDLER("rs_up", rs_up_out, default_in),
	RS_CALL(DOWN) = HANDLER("rs_down", rs_label_out, default_in),
	RS_CALL(REFRESH) = HANDLER("rs_refresh", rs_label_out, default_in),
	RS_CALL(RESTART) = HANDLER("rs_restart", rs_label_out, default_in),
	RS_CALL(SHUTDOWN) = HANDLER("rs_shutdown", default_out, default_in),
	RS_CALL(CLONE) = HANDLER("rs_clone", rs_label_out, default_in),
	RS_CALL(UPDATE) = HANDLER("rs_update", rs_update_out, default_in),
	RS_CALL(EDIT) = HANDLER("rs_edit", rs_up_out, default_in),
	RS_CALL(LOOKUP) = HANDLER("rs_lookup", rs_lookup_out, rs_lookup_in),
};

const struct calls rs_calls = {
	.endpt = RS_PROC_NR,
	.base = RS_RQ_BASE,
	.map = rs_map,
	.count = COUNT(rs_map)
};
