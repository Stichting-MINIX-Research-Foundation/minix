/* This file contains some utility routines for RS.
 *
 * Changes:
 *   Nov 22, 2009: Created    (Cristiano Giuffrida)
 */

#include "inc.h"

#include <minix/ds.h>

/*===========================================================================*
 *				 init_service				     *
 *===========================================================================*/
PUBLIC int init_service(rp, type)
struct rproc *rp;				/* pointer to process slot */
int type;					/* type of initialization */
{
  int r;
  message m;
  struct rprocpub *rpub;

  rpub = rp->r_pub;

  rp->r_flags |= RS_INITIALIZING;              /* now initializing */
  rp->r_check_tm = rp->r_alive_tm + 1;         /* expect reply within period */

  m.m_type = RS_INIT;
  m.RS_INIT_TYPE = type;
  m.RS_INIT_RPROCTAB_GID = rinit.rproctab_gid;
  r = asynsend(rpub->endpoint, &m);

  return r;
}

/*===========================================================================*
 *				publish_service				     *
 *===========================================================================*/
PUBLIC int publish_service(rp)
struct rproc *rp;				/* pointer to process slot */
{
/* A new system service has been started. Publish the necessary information. */
  int s;
  struct rprocpub *rpub;

  rpub = rp->r_pub;

  /* Register its label with DS. */
  s= ds_publish_label(rpub->label, rpub->endpoint, DSF_OVERWRITE);
  if (s != OK) {
      return s;
  }
  if (rs_verbose) {
      printf("RS: publish_service: DS label registration done: %s -> %d\n", 
          rpub->label, rpub->endpoint);
  }

  return(OK);
}

/*===========================================================================*
 *			      fill_call_mask                                 *
 *===========================================================================*/
PUBLIC void fill_call_mask(calls, tot_nr_calls, call_mask, call_base, is_init)
int *calls;                     /* the unordered set of calls */
int tot_nr_calls;               /* the total number of calls */
bitchunk_t *call_mask;          /* the call mask to fill in */
int call_base;                  /* the base offset for the calls */
int is_init;                    /* set when initializing a call mask */
{
/* Fill a call mask from an unordered set of calls. */
  int i;
  int call_mask_size, nr_calls;

  call_mask_size = BITMAP_CHUNKS(tot_nr_calls);

  /* Count the number of calls to fill in. */
  nr_calls = 0;
  for(i=0; calls[i] != SYS_NULL_C; i++) {
      nr_calls++;
  }

  /* See if all calls are allowed and call mask must be completely filled. */
  if(nr_calls == 1 && calls[0] == SYS_ALL_C) {
      for(i=0; i < call_mask_size; i++) {
          call_mask[i] = (~0);
      }
  }
  else {
      /* When initializing, reset the mask first. */
      if(is_init) {
          for(i=0; i < call_mask_size; i++) {
              call_mask[i] = 0;
          }
      }
      /* Enter calls bit by bit. */
      for(i=0; i < nr_calls; i++) {
          SET_BIT(call_mask, calls[i] - call_base);
      }
  }
}

