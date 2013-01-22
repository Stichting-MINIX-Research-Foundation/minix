/*	$NetBSD: ipc_gen.c,v 1.1.1.1 2008/05/18 14:31:25 aymeric Exp $ */

/* Do not edit: automatically built by build/distrib. */
static int
vi_c_bol(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_C_BOL);
}

static int
vi_c_bottom(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_C_BOTTOM);
}

static int
vi_c_del(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_C_DEL);
}

static int
vi_c_down(IPVIWIN *ipvi, u_int32_t val1)
{
	return vi_send_1(ipvi, VI_C_DOWN, val1);
}

static int
vi_c_eol(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_C_EOL);
}

static int
vi_c_insert(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_C_INSERT);
}

static int
vi_c_left(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_C_LEFT);
}

static int
vi_c_pgdown(IPVIWIN *ipvi, u_int32_t val1)
{
	return vi_send_1(ipvi, VI_C_PGDOWN, val1);
}

static int
vi_c_pgup(IPVIWIN *ipvi, u_int32_t val1)
{
	return vi_send_1(ipvi, VI_C_PGUP, val1);
}

static int
vi_c_right(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_C_RIGHT);
}

static int
vi_c_search(IPVIWIN *ipvi, const char *stra, u_int32_t lena, u_int32_t val1)
{
	return vi_send_a1(ipvi, VI_C_SEARCH, stra, lena, val1);
}

static int
vi_c_settop(IPVIWIN *ipvi, u_int32_t val1)
{
	return vi_send_1(ipvi, VI_C_SETTOP, val1);
}

static int
vi_c_top(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_C_TOP);
}

static int
vi_c_up(IPVIWIN *ipvi, u_int32_t val1)
{
	return vi_send_1(ipvi, VI_C_UP, val1);
}

static int
vi_edit(IPVIWIN *ipvi, const char *stra, u_int32_t lena)
{
	return vi_send_a(ipvi, VI_EDIT, stra, lena);
}

static int
vi_editopt(IPVIWIN *ipvi, const char *stra, u_int32_t lena, const char *strb, u_int32_t lenb, u_int32_t val1)
{
	return vi_send_ab1(ipvi, VI_EDITOPT, stra, lena, strb, lenb, val1);
}

static int
vi_editsplit(IPVIWIN *ipvi, const char *stra, u_int32_t lena)
{
	return vi_send_a(ipvi, VI_EDITSPLIT, stra, lena);
}

static int
vi_eof(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_EOF);
}

static int
vi_err(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_ERR);
}

static int
vi_flags(IPVIWIN *ipvi, u_int32_t val1)
{
	return vi_send_1(ipvi, VI_FLAGS, val1);
}

static int
vi_interrupt(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_INTERRUPT);
}

static int
vi_mouse_move(IPVIWIN *ipvi, u_int32_t val1, u_int32_t val2)
{
	return vi_send_12(ipvi, VI_MOUSE_MOVE, val1, val2);
}

static int
vi_quit(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_QUIT);
}

static int
vi_resize(IPVIWIN *ipvi, u_int32_t val1, u_int32_t val2)
{
	return vi_send_12(ipvi, VI_RESIZE, val1, val2);
}

static int
vi_sel_end(IPVIWIN *ipvi, u_int32_t val1, u_int32_t val2)
{
	return vi_send_12(ipvi, VI_SEL_END, val1, val2);
}

static int
vi_sel_start(IPVIWIN *ipvi, u_int32_t val1, u_int32_t val2)
{
	return vi_send_12(ipvi, VI_SEL_START, val1, val2);
}

static int
vi_sighup(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_SIGHUP);
}

static int
vi_sigterm(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_SIGTERM);
}

static int
vi_string(IPVIWIN *ipvi, const char *stra, u_int32_t lena)
{
	return vi_send_a(ipvi, VI_STRING, stra, lena);
}

static int
vi_tag(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_TAG);
}

static int
vi_tagas(IPVIWIN *ipvi, const char *stra, u_int32_t lena)
{
	return vi_send_a(ipvi, VI_TAGAS, stra, lena);
}

static int
vi_tagsplit(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_TAGSPLIT);
}

static int
vi_undo(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_UNDO);
}

static int
vi_wq(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_WQ);
}

static int
vi_write(IPVIWIN *ipvi)
{
	return vi_send_(ipvi, VI_WRITE);
}

static int
vi_writeas(IPVIWIN *ipvi, const char *stra, u_int32_t lena)
{
	return vi_send_a(ipvi, VI_WRITEAS, stra, lena);
}

