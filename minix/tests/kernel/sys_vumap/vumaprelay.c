/* Test for sys_vumap() - by D.C. van Moolenbroek */
#include <minix/drivers.h>
#include <assert.h>

#include "com.h"

static int do_request(message *m)
{
	struct vumap_vir vvec[MAPVEC_NR + 3];
	struct vumap_phys pvec[MAPVEC_NR + 3];
	int r, r2, access, vcount, pcount;
	size_t offset;

	assert(m->m_type == VTR_RELAY);

	vcount = m->VTR_VCOUNT;
	pcount = m->VTR_PCOUNT;
	offset = m->VTR_OFFSET;
	access = m->VTR_ACCESS;

	r2 = sys_safecopyfrom(m->m_source, m->VTR_VGRANT, 0, (vir_bytes) vvec,
		sizeof(vvec[0]) * vcount);
	assert(r2 == OK);

	r = sys_vumap(m->m_source, vvec, vcount, offset, access, pvec,
		&pcount);

	if (pcount >= 1 && pcount <= MAPVEC_NR + 3) {
		r2 = sys_safecopyto(m->m_source, m->VTR_PGRANT, 0,
			(vir_bytes) pvec, sizeof(pvec[0]) * pcount);
		assert(r2 == OK);
	}

	m->VTR_PCOUNT = pcount;

	return r;
}

static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	return OK;
}

static void sef_cb_signal_handler(int sig)
{
	if (sig == SIGTERM)
		exit(0);
}

static void sef_local_startup(void)
{
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_signal_handler(sef_cb_signal_handler);

	sef_startup();
}

int main(int argc, char **argv)
{
	message m;
	int r;

	env_setargs(argc, argv);

	sef_local_startup();

	for (;;) {
		if ((r = sef_receive(ANY, &m)) != OK)
			panic("sef_receive failed (%d)\n", r);

		m.m_type = do_request(&m);

		ipc_send(m.m_source, &m);
	}

	return 0;
}
