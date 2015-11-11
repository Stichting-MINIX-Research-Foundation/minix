#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/com.h>

#include <minix/type.h>
#include <minix/ds.h>
#include "ds/store.h"

#include <magic_analysis.h>
#include <st/state_transfer.h>

const char* sef_sf_typename_keys[] = { "dsi_u", NULL };
#define dsi_u_idx 0

/*===========================================================================*
 *		           sef_cb_sf_transfer_dsi_u                          *
 *===========================================================================*/
static int sef_cb_sf_transfer_dsi_u(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info) {
	static struct dsi_mem noxfer_dsi_mem_stub;
	static const struct _magic_type* _magic_dsi_mem_type = NULL;
	struct data_store* dsp;
	_magic_selement_t parent_selement;
	int ret = EGENERIC;
	VOLATILE int keep_stubs = (int)&noxfer_dsi_mem_stub;
	assert(keep_stubs);

	if(magic_selement_get_parent(selement, &parent_selement) == NULL) {
		ST_CB_PRINT(ST_CB_ERR, "sef_cb_sf_transfer_dsi_u: magic_selement_get_parent failed", selement, sel_analyzed, sel_stats, cb_info);
		return EINVAL;
	}
	dsp = (struct data_store*) parent_selement.address;
	if(!(dsp->flags & DSF_IN_USE)) {
		/* Skip when unused. */
		return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
	}
	switch(dsp->flags & DSF_MASK_TYPE) {
		case DSF_TYPE_U32:
		case DSF_TYPE_LABEL:
			/* Identity transfer when no ptr is involved. */
			ret = st_cb_transfer_identity(selement, sel_analyzed, sel_stats, cb_info);
			break;
		case DSF_TYPE_STR:
		case DSF_TYPE_MEM:
			/* Transfer as dsp->u.mem struct. */
			if(!_magic_dsi_mem_type && !(_magic_dsi_mem_type = magic_type_lookup_by_name("dsi_mem"))) {
				ST_CB_PRINT(ST_CB_ERR, "sef_cb_sf_transfer_dsi_u: type dsi_mem not found", selement, sel_analyzed, sel_stats, cb_info);
				return ENOENT;
			}
			st_cb_selement_type_cast(_magic_dsi_mem_type, _magic_dsi_mem_type, selement, sel_analyzed, sel_stats, cb_info);
			ret = st_cb_transfer_selement_generic(selement, sel_analyzed, sel_stats, cb_info);
			break;
		default:
			/* Unknown? Report error. */
			ST_CB_PRINT(ST_CB_ERR, "sef_cb_sf_transfer_dsi_u: bad flags", selement, sel_analyzed, sel_stats, cb_info);
			ret = EFAULT;
			break;
	}
	return ret;
}

/*===========================================================================*
 *		         sef_cb_sf_transfer_typename                         *
 *===========================================================================*/
static int sef_cb_sf_transfer_typename(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info) {
    const char *typename_key = ST_TYPE_NAME_KEY(selement->type);
	if(ST_TYPE_NAME_MATCH(sef_sf_typename_keys[dsi_u_idx],typename_key))
	{
		return sef_cb_sf_transfer_dsi_u(selement, sel_analyzed, sel_stats, cb_info);
	}

	return ST_CB_NOT_PROCESSED;
}

/*===========================================================================*
 *		         _magic_ds_st_init                         *
 *===========================================================================*/
void _magic_ds_st_init(void)
{
  st_register_typename_keys(sef_sf_typename_keys);
  st_setcb_selement_transfer(sef_cb_sf_transfer_typename, ST_CB_TYPE_TYPENAME);
}

