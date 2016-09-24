
#include "syslib.h"
#include <assert.h>
#include <minix/sysutil.h>
#include <minix/gcov.h>

static sef_cb_gcov_t sef_cb_gcov = SEF_CB_GCOV_FLUSH_DEFAULT;

int do_sef_gcov_request(message *);

/*===========================================================================*
 *                            do_sef_gcov_request             		     *
 *===========================================================================*/
int do_sef_gcov_request(message *m_ptr)
{
	if(!sef_cb_gcov)
		return ENOSYS;

	sef_cb_gcov(m_ptr);

	return OK;
}

/*===========================================================================*
 *                            sef_setcb_gcov             		     *
 *===========================================================================*/
void sef_setcb_gcov(sef_cb_gcov_t cb)
{
	sef_cb_gcov = cb;
}
