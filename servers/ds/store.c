/* Implementation of the Data Store. */

#include "inc.h"

/* Allocate space for the data store. */
PRIVATE struct data_store ds_store[NR_DS_KEYS];
PRIVATE int nr_in_use;

PRIVATE _PROTOTYPE(int find_key, (int key, struct data_store **dsp));
PRIVATE _PROTOTYPE(int set_owner, (struct data_store *dsp, void *auth_ptr));
PRIVATE _PROTOTYPE(int is_authorized, (struct data_store *dsp, void *auth_ptr));


PRIVATE int set_owner(dsp, ap)
struct data_store *dsp;				/* data store structure */
void *ap;					/* authorization pointer */
{
  /* Authorize the caller. */
  return(TRUE);
}


PRIVATE int is_authorized(dsp, ap)
struct data_store *dsp;				/* data store structure */
void *ap;					/* authorization pointer */
{
  /* Authorize the caller. */
  return(TRUE);
}


PRIVATE int find_key(key, dsp)
int key;					/* key to look up */
struct data_store **dsp;			/* store pointer here */
{
  register int i;

  *dsp = NULL;
  for (i=0; i<NR_DS_KEYS; i++) {
      if ((ds_store[i].ds_flags & DS_IN_USE) && ds_store[i].ds_key == key) {
	  *dsp = &ds_store[i];
          return(TRUE);				/* report success */
      }
  }
  return(FALSE);				/* report not found */
}


PUBLIC int do_publish(m_ptr)
message *m_ptr;					/* request message */
{
  struct data_store *dsp;

  /* Store (key,value)-pair. First see if key already exists. If so, 
   * check if the caller is allowed to overwrite the value. Otherwise
   * find a new slot and store the new value. 
   */
  if (find_key(m_ptr->DS_KEY, &dsp)) {			/* look up key */
      if (! is_authorized(dsp,m_ptr->DS_AUTH)) {	/* check if owner */
          return(EPERM);
      }
  } 
  else {						/* find a new slot */
      if (nr_in_use >= NR_DS_KEYS) {
	  return(EAGAIN);				/* store is full */
      } else {
          dsp = &ds_store[nr_in_use];			/* new slot found */
	  dsp->ds_key = m_ptr->DS_KEY;
	  if (! set_owner(dsp,m_ptr->DS_AUTH)) {	/* associate owner */
	      return(EINVAL);
	  }
	  dsp->ds_nr_subs = 0;				/* nr of subscribers */
	  dsp->ds_flags = DS_IN_USE;			/* initialize slot */
	  nr_in_use ++;
      }
  }

  /* At this point we have a data store pointer and know the caller is 
   * authorize to write to it. Set all fields as requested.
   */
  dsp->ds_val_l1 = m_ptr->DS_VAL_L1;		/* store all data */
  dsp->ds_val_l2 = m_ptr->DS_VAL_L2;	

  /* If the data is public. Check if there are any subscribers to this key. 
   * If so, notify all subscribers so that they can retrieve the data, if 
   * they're still interested.
   */
  if ((dsp->ds_flags & DS_PUBLIC) && dsp->ds_nr_subs > 0) {

      /* Subscriptions are not yet implemented. */
  }

  return(OK);
}


PUBLIC int do_retrieve(m_ptr)
message *m_ptr;					/* request message */
{
  struct data_store *dsp;

  /* Retrieve data. Look up the key in the data store. Return an error if it
   * is not found. If this data is private, only the owner may retrieve it.
   */ 
  if (find_key(m_ptr->DS_KEY, &dsp)) {			/* look up key */

      /* If the data is not public, the caller must be authorized. */
      if (! dsp->ds_flags & DS_PUBLIC) {		/* check if private */
          if (! is_authorized(dsp,m_ptr->DS_AUTH)) {	/* authorize call */
              return(EPERM);				/* not allowed */
          }
      }

      /* Data is public or the caller is authorized to retrieve it. */
      printf("DS retrieves data: key %d (found %d), l1 %u, l2 %u\n",
		m_ptr->DS_KEY, dsp->ds_key, dsp->ds_val_l1, dsp->ds_val_l2);
      m_ptr->DS_VAL_L1 = dsp->ds_val_l1;		/* return value */
      m_ptr->DS_VAL_L2 = dsp->ds_val_l2;		/* return value */
      return(OK);					/* report success */
  }
  return(ESRCH);					/* key not found */
}


PUBLIC int do_subscribe(m_ptr)
message *m_ptr;					/* request message */
{
  /* Subscribe to a key of interest. Only existing and public keys can be
   * subscribed to. All updates to the key will cause a notification message
   * to be sent to the subscribed. On success, directly return a copy of the
   * data for the given key. 
   */
  return(ENOSYS);
}


/*===========================================================================*
 *				do_getsysinfo				     *
 *===========================================================================*/
PUBLIC int do_getsysinfo(m_ptr)
message *m_ptr;
{
  vir_bytes src_addr, dst_addr;
  int dst_proc;
  size_t len;
  int s;

  switch(m_ptr->m1_i1) {
  case SI_DATA_STORE:
  	src_addr = (vir_bytes) ds_store;
  	len = sizeof(struct data_store) * NR_DS_KEYS;
  	break; 
  default:
  	return(EINVAL);
  }

  dst_proc = m_ptr->m_source;
  dst_addr = (vir_bytes) m_ptr->m1_p1;
  if (OK != (s=sys_datacopy(SELF, src_addr, dst_proc, dst_addr, len)))
  	return(s);
  return(OK);
}

