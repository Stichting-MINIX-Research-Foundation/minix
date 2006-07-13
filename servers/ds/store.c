/* Implementation of the Data Store. */

#include "inc.h"

/* Allocate space for the data store. */
PRIVATE struct data_store ds_store[NR_DS_KEYS];
PRIVATE struct subscription ds_subs[NR_DS_SUBS];
PRIVATE int nr_in_use;

PRIVATE _PROTOTYPE(int find_key, (char *key, struct data_store **dsp, int t));
PRIVATE _PROTOTYPE(int set_owner, (struct data_store *dsp, int auth));
PRIVATE _PROTOTYPE(int is_authorized, (struct data_store *dsp, int auth));
PRIVATE _PROTOTYPE(void check_subscribers, (struct data_store *dsp));


/*===========================================================================*
 *				ds_init				     	     *
 *===========================================================================*/
PUBLIC void ds_init(void)
{
	int i;

	/* Reset data store: data and subscriptions. */

	for(i = 0; i < NR_DS_KEYS; i++) {
		int b;
		ds_store[i].ds_flags = 0;
		for(b = 0; b < BITMAP_CHUNKS(NR_DS_SUBS); b++) {
			ds_store[i].ds_old_subs[b] = 0;
		}
	}
	for(i = 0; i < NR_DS_SUBS; i++)
		ds_subs[i].sub_flags = 0;

	return;
}

PRIVATE int set_owner(dsp, auth)
struct data_store *dsp;				/* data store structure */
int auth;
{
  return(TRUE);
}


PRIVATE int is_authorized(dsp, ap)
struct data_store *dsp;				/* data store structure */
int ap;						/* authorization value */
{
  /* Authorize the caller. */
  return(TRUE);
}


PRIVATE int find_key(key_name, dsp, type)
char *key_name;					/* key to look up */
struct data_store **dsp;			/* store pointer here */
int type;					/* type info */
{
  register int i;

  *dsp = NULL;
  for (i=0; i<NR_DS_KEYS; i++) {
      if ((ds_store[i].ds_flags & DS_IN_USE)		/* valid slot? */
      &&  ((ds_store[i].ds_flags & type) == type)	/* right type? */
      && !strcmp(ds_store[i].ds_key, key_name)) {	/* matching name? */
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
  char key_name[DS_MAX_KEYLEN];
  int r, type;

  /* Store (key,value)-pair. First see if key already exists. If so, 
   * check if the caller is allowed to overwrite the value. Otherwise
   * find a new slot and store the new value. 
   */
  if (m_ptr->DS_KEY_LEN > DS_MAX_KEYLEN || m_ptr->DS_KEY_LEN < 2) {
	printf("DS: bogus key length (%d) from %d\n", m_ptr->DS_KEY_LEN,
		m_ptr->m_source);
	return EINVAL;
  }

  /* Check type info. */
  type = m_ptr->DS_FLAGS & DS_TYPE_MASK;
  if(type != DS_TYPE_U32 && type != DS_TYPE_STR) {
	printf("DS: bogus type code %lx from %d\n", type, m_ptr->m_source);
	return EINVAL;
  }

  /* Copy name from caller. */
  if ((r=sys_safecopyfrom(m_ptr->m_source,
	(cp_grant_id_t) m_ptr->DS_KEY_GRANT, 0, 
	(vir_bytes) key_name, m_ptr->DS_KEY_LEN, D)) != OK) {
	printf("DS: publish: copy failed from %d: %d\n", m_ptr->m_source, r);
	return r;
  }

  /* Make sure name is 0-terminated. */
  key_name[DS_MAX_KEYLEN-1] = '\0';

  /* See if it already exists. */
  if (!find_key(key_name, &dsp, type)) {		/* look up key */
      if (nr_in_use >= NR_DS_KEYS) {
	  return(EAGAIN);				/* store is full */
      } else {
          dsp = &ds_store[nr_in_use];			/* new slot found */
	  strcpy(dsp->ds_key, key_name);
	  dsp->ds_flags = DS_IN_USE | m_ptr->DS_FLAGS;	/* initialize slot */
	  nr_in_use ++;
      }
  }

  /* At this point we have a data store pointer and know the caller is 
   * authorize to write to it. Set all fields as requested.
   */
  switch(type) {
	case DS_TYPE_U32:
	  dsp->ds_val.ds_val_u32 = (u32_t) m_ptr->DS_VAL;	/* store data */
	  break;
	case DS_TYPE_STR:
	  /* store string data: check size, then do copy */
	  if(m_ptr->DS_VAL_LEN < 1 || m_ptr->DS_VAL_LEN > DS_MAX_VALLEN) {
	    printf("DS: publish: bogus len from %d: %d\n",
	      m_ptr->m_source, m_ptr->DS_VAL_LEN);
	    return EINVAL;
	  }

	  if((r=sys_safecopyfrom(m_ptr->m_source, m_ptr->DS_VAL, 0,
		  (vir_bytes) dsp->ds_val.ds_val_str,
		  m_ptr->DS_VAL_LEN, D)) != OK) {
		  printf("DS: publish: str copy failed from %d: %d\n",
		  m_ptr->m_source, r);
		  return r;
	  }
	  break;
	default:
          panic(__FILE__, "Impossible type.", type);
	  break;
  }

  /* If anyone has a matching subscription, update them. */
  check_subscribers(dsp);

  return(OK);
}


/*===========================================================================*
 *				do_retrieve				     *
 *===========================================================================*/
PUBLIC int do_retrieve(m_ptr)
message *m_ptr;					/* request message */
{
  struct data_store *dsp;
  char key_name[DS_MAX_KEYLEN];
  int r, type;
  size_t len;

  if (m_ptr->DS_KEY_LEN > DS_MAX_KEYLEN || m_ptr->DS_KEY_LEN < 1) {
	printf("DS: bogus key length (%d) from %d\n", m_ptr->DS_KEY_LEN,
		m_ptr->m_source);
	return EINVAL;
  }

  /* Copy name from caller. */
  if ((r=sys_safecopyfrom(m_ptr->m_source,
	(cp_grant_id_t) m_ptr->DS_KEY_GRANT, 0, 
	(vir_bytes) key_name, m_ptr->DS_KEY_LEN, D)) != OK) {
	printf("DS: retrieve: copy failed from %d: %d\n", m_ptr->m_source, r);
	return r;
  }

  /* Make sure name is 0-terminated. */
  key_name[DS_MAX_KEYLEN-1] = '\0';


  /* Retrieve data. Look up the key in the data store. Return an error if it
   * is not found. If this data is private, only the owner may retrieve it.
   */ 
  type = m_ptr->DS_FLAGS & DS_TYPE_MASK;
  if (find_key(key_name, &dsp, type)) {	/* look up key */
      /* Data is public or the caller is authorized to retrieve it. */
      switch(type) {
	case DS_TYPE_U32:
	        m_ptr->DS_VAL = dsp->ds_val.ds_val_u32;  /* return value */
		break;
	case DS_TYPE_STR:
		len = strlen(dsp->ds_val.ds_val_str) + 1;
		if(len > m_ptr->DS_VAL_LEN)
			len = m_ptr->DS_VAL_LEN;
  		if ((r=sys_safecopyto(m_ptr->m_source, m_ptr->DS_VAL,
			0, (vir_bytes) dsp->ds_val.ds_val_str,len, D)) != OK) {
			printf("DS: retrieve: copy failed to %d: %d\n",	
				m_ptr->m_source, r);
			return r;
		}
		break;
	default:
                panic(__FILE__, "retrieve: impossible type.", type);
		/* not reached. */
		break;

      }
      return(OK);					/* report success */
  }
  return(ESRCH);					/* key not found */
}

/*===========================================================================*
 *				do_check				     *
 *===========================================================================*/
PUBLIC int do_check(m_ptr)
message *m_ptr;					/* request message */
{
/* This routine goes through all subscriptions for a client,
 * and checks all data items if it has been flagged (i.e.,
 * created or updated) matching that subscription. Return
 * a message and copy the key and value for every one.
 */
	struct data_store *dsp;
	int r, s, d, type = m_ptr->DS_FLAGS & DS_TYPE_MASK;
	if(!type) return EINVAL;
	for(s = 0; s < NR_DS_SUBS; s++) {
		int len;
		if(!(ds_subs[s].sub_flags & DS_IN_USE))
			continue;
		if(m_ptr->m_source != ds_subs[s].sub_owner)
			continue;
		for(d = 0;  d < NR_DS_KEYS; d++) {
			
			/* No match if this is no value, it's
			 * not flagged, or the type is wrong.
			 */

			if(!(ds_store[d].ds_flags & DS_IN_USE))
				continue;
			if(!GET_BIT(ds_store[d].ds_old_subs, s))
				continue;
			if(type != (ds_store[d].ds_flags & DS_TYPE_MASK))
				continue;

			/* We have a match. Unflag it for this
			 * subscription.
			 */
			UNSET_BIT(ds_store[d].ds_old_subs, s);
			len = strlen(ds_store[d].ds_key)+1;
			if(len > m_ptr->DS_KEY_LEN) 
				len = m_ptr->DS_KEY_LEN;

			/* Copy the key into client. */
  			if ((r=sys_safecopyto(m_ptr->m_source,
				(cp_grant_id_t) m_ptr->DS_KEY_GRANT, 0,
				(vir_bytes) ds_store[d].ds_key,
				len, D)) != OK)
				return r;

			/* Now copy the value. */
			switch(type) {
				case DS_TYPE_STR:
					len = strlen(ds_store[d].
						ds_val.ds_val_str)+1;
					if(len > m_ptr->DS_VAL_LEN)
						len = m_ptr->DS_VAL_LEN;
  					if ((r=sys_safecopyto(m_ptr->m_source,
						m_ptr->DS_VAL, 0,
						(vir_bytes) ds_store[d].
						  ds_val.ds_val_str,
						len, D)) != OK)
						return r;
					break;
				case DS_TYPE_U32:
					m_ptr->DS_VAL =
						ds_store[d].ds_val.ds_val_u32;
					break;
				default:
          				panic(__FILE__,
						"Check impossible type.",
						type);
			}

			return OK;
		}
	}

	return(ESRCH);					/* key not found */
}

PUBLIC int do_subscribe(m_ptr)
message *m_ptr;					/* request message */
{
  char regex[DS_MAX_KEYLEN+3];
  int s, type, e, d, n = 0;
  char errbuf[80];

  /* Subscribe to a key of interest.
   * All updates to the key will cause a notification message
   * to be sent to the subscribed. On success, directly return a copy of the
   * data for the given key. 
   */
  if(m_ptr->DS_KEY_LEN < 2 || m_ptr->DS_KEY_LEN > DS_MAX_KEYLEN)
	return EINVAL;

  /* Copy name from caller. Anchor the subscription with "^regexp$" so
   * substrings don't match. The caller probably will not expect this,
   * and the usual case is for a complete match.
   */
  regex[0] = '^';
  if ((s=sys_safecopyfrom(m_ptr->m_source,
	(cp_grant_id_t) m_ptr->DS_KEY_GRANT, 0, 
	(vir_bytes) regex + 1, m_ptr->DS_KEY_LEN, D)) != OK) {
	printf("DS: retrieve: copy failed from %d: %d\n", m_ptr->m_source, s);
	return s;
  }

  regex[DS_MAX_KEYLEN-1] = '\0';
  strcat(regex, "$");

  /* Find subscription slot. */
  for(s = 0; s < NR_DS_SUBS; s++)
	if(!(ds_subs[s].sub_flags & DS_IN_USE))
		break;

  if(s >= NR_DS_SUBS) {
	printf("DS: no space for subscription by %d.\n", m_ptr->m_source);
	return ENOSPC;
  }

  /* Compile regular expression. */
  if((e=regcomp(&ds_subs[s].sub_regex, regex, REG_EXTENDED)) != 0) {
	regerror(e, &ds_subs[s].sub_regex, errbuf, sizeof(errbuf));
	printf("DS: subscribe: regerror: %s\n", errbuf);
	return EINVAL;
  }
  type = (m_ptr->DS_FLAGS & DS_TYPE_MASK);
  ds_subs[s].sub_flags = DS_IN_USE | type;
  ds_subs[s].sub_owner = m_ptr->m_source;

  /* Caller requested an instant initial list? */
  if(m_ptr->DS_FLAGS & DS_INITIAL) {
	for(d = 0; d < NR_DS_KEYS; d++) {
	  if(!(ds_store[d].ds_flags & DS_IN_USE))
	     continue;
	  if(regexec(&ds_subs[s].sub_regex, ds_store[d].ds_key,
		0, NULL, 0) == 0) {
		SET_BIT(ds_store[d].ds_old_subs, s);
		n = 1;
	  }
      }

      /* Any matches? */
      if(n) notify(ds_subs[s].sub_owner);
   }

   return OK;
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
  if (OK != (s=sys_datacopy(SELF, src_addr, dst_proc, dst_addr, len))) {
	printf("DS: copy failed: %d\n", s);
  	return(s);
  }
  return(OK);
}

/*===========================================================================*
 *				check_subscribers			     *
 *===========================================================================*/
PRIVATE void
check_subscribers(struct data_store *dsp)
{
/* Send subscribers whose subscriptions match this (new
 * or updated) data item a notify(), and flag the subscriptions
 * as updated.
 */
	int i;
	for(i = 0; i < NR_DS_SUBS; i++) {
		if(ds_subs[i].sub_flags & DS_IN_USE) {
			if(regexec(&ds_subs[i].sub_regex, dsp->ds_key, 
				0, NULL, 0) == 0) {
				SET_BIT(dsp->ds_old_subs, i);
				notify(ds_subs[i].sub_owner);
			} else {
				UNSET_BIT(dsp->ds_old_subs, i);
			}
		} 
	}
}

