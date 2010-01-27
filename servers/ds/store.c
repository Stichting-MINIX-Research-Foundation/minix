#include "inc.h"
#include "store.h"

/* Allocate space for the data store. */
PRIVATE struct data_store ds_store[NR_DS_KEYS];
PRIVATE struct subscription ds_subs[NR_DS_SUBS];

/*===========================================================================*
 *			      alloc_data_slot				     *
 *===========================================================================*/
PRIVATE struct data_store *alloc_data_slot(void)
{
/* Allocate a new data slot. */
  int i;

  for (i = 0; i < NR_DS_KEYS; i++) {
	if (!(ds_store[i].flags & DSF_IN_USE))
		return &ds_store[i];
  }

  return NULL;
}

/*===========================================================================*
 *				alloc_sub_slot				     *
 *===========================================================================*/
PRIVATE struct subscription *alloc_sub_slot(void)
{
/* Allocate a new subscription slot. */
  int i;

  for (i = 0; i < NR_DS_SUBS; i++) {
	if (!(ds_subs[i].flags & DSF_IN_USE))
		return &ds_subs[i];
  }

  return NULL;
}

/*===========================================================================*
 *				lookup_entry				     *
 *===========================================================================*/
PRIVATE struct data_store *lookup_entry(const char *key_name, int type)
{
/* Lookup an existing entry by key and type. */
  int i;

  for (i = 0; i < NR_DS_KEYS; i++) {
	if ((ds_store[i].flags & DSF_IN_USE) /* used */
		&& (ds_store[i].flags & type) /* same type*/
		&& !strcmp(ds_store[i].key, key_name)) /* same key*/
		return &ds_store[i];
  }

  return NULL;
}

/*===========================================================================*
 *			     lookup_label_entry				     *
 *===========================================================================*/
PRIVATE struct data_store *lookup_label_entry(unsigned num)
{
/* Lookup an existing label entry by num. */
  int i;

  for (i = 0; i < NR_DS_KEYS; i++) {
	if ((ds_store[i].flags & DSF_IN_USE)
		&& (ds_store[i].flags & DSF_TYPE_LABEL)
		&& (ds_store[i].u.u32 == num))
		return &ds_store[i];
  }

  return NULL;
}

/*===========================================================================*
 *			      lookup_sub				     *
 *===========================================================================*/
PRIVATE struct subscription *lookup_sub(const char *owner)
{
/* Lookup an existing subscription given its owner. */
  int i;

  for (i = 0; i < NR_DS_SUBS; i++) {
	if ((ds_subs[i].flags & DSF_IN_USE) /* used */
		&& !strcmp(ds_subs[i].owner, owner)) /* same key*/
		return &ds_subs[i];
  }

  return NULL;
}

/*===========================================================================*
 *				ds_getprocname				     *
 *===========================================================================*/
PRIVATE char *ds_getprocname(endpoint_t e)
{
/* Get a process name given its endpoint. */
	struct data_store *dsp;

	static char *first_proc_name = "ds";
	endpoint_t first_proc_ep = DS_PROC_NR;

	if(e == first_proc_ep)
		return first_proc_name;

	if((dsp = lookup_label_entry(e)) != NULL)
		return dsp->key;

	return NULL;
}

/*===========================================================================*
 *				ds_getprocep				     *
 *===========================================================================*/
PRIVATE endpoint_t ds_getprocep(char *s)
{
/* Get a process endpoint given its name. */
	struct data_store *dsp;

	if((dsp = lookup_entry(s, DSF_TYPE_LABEL)) != NULL)
		return dsp->u.u32;
	return -1;
}

/*===========================================================================*
 *				 check_auth				     *
 *===========================================================================*/
PRIVATE int check_auth(struct data_store *p, endpoint_t ep, int perm)
{
/* Check authorization for a given type of permission. */
	if(!(p->flags & perm))
		return 1;

	return !strcmp(p->owner, ds_getprocname(ep));
}

/*===========================================================================*
 *				get_key_name				     *
 *===========================================================================*/
PRIVATE int get_key_name(message *m_ptr, char *key_name)
{
/* Get key name given an input message. */
  int r;

  if (m_ptr->DS_KEY_LEN > DS_MAX_KEYLEN || m_ptr->DS_KEY_LEN < 2) {
	printf("DS: bogus key length (%d) from %d\n", m_ptr->DS_KEY_LEN,
		m_ptr->m_source);
	return EINVAL;
  }

  /* Copy name from caller. */
  r = sys_safecopyfrom(m_ptr->m_source,
	(cp_grant_id_t) m_ptr->DS_KEY_GRANT, 0, 
	(vir_bytes) key_name, m_ptr->DS_KEY_LEN, D);
  if(r != OK) {
	printf("DS: publish: copy failed from %d: %d\n", m_ptr->m_source, r);
	return r;
  }

  key_name[DS_MAX_KEYLEN-1] = '\0';

  return OK;
}

/*===========================================================================*
 *			     check_snapshot_index			     *
 *===========================================================================*/
PRIVATE int check_snapshot_index(struct data_store *dsp, int index)
{
/* See if the given snapshot index is valid. */
  int min;

  min = dsp->u.map.sindex < NR_DS_SNAPSHOT
	? 0
	: dsp->u.map.sindex - NR_DS_SNAPSHOT + 1;

  return (index >= min && index <= dsp->u.map.sindex) ? 0 : 1;
}

/*===========================================================================*
 *				check_sub_match				     *
 *===========================================================================*/
PRIVATE int check_sub_match(struct subscription *subp,
		struct data_store *dsp, endpoint_t ep)
{
/* Check if an entry matches a subscription. Return 1 in case of match. */
  return (check_auth(dsp, ep, DSF_PRIV_SUBSCRIBE)
	  && regexec(&subp->regex, dsp->key, 0, NULL, 0) == 0)
	  ? 1 : 0;
}

/*===========================================================================*
 *			     update_subscribers				     *
 *===========================================================================*/
PRIVATE void update_subscribers(struct data_store *dsp, int set)
{
/* If set = 1, set bit in the sub bitmap of any subscription matching the given
 * entry, otherwise clear it. In both cases, notify the subscriber.
 */
	int i;
	int nr = dsp - ds_store;
	endpoint_t ep;

	for(i = 0; i < NR_DS_SUBS; i++) {
		if(!(ds_subs[i].flags & DSF_IN_USE))
			continue;
		if(!(ds_subs[i].flags & dsp->flags & DSF_MASK_TYPE))
			continue;

		ep = ds_getprocep(ds_subs[i].owner);
		if(!check_sub_match(&ds_subs[i], dsp, ep))
			continue;

		if(set == 1) {
			SET_BIT(ds_subs[i].old_subs, nr);
		} else {
			UNSET_BIT(ds_subs[i].old_subs, nr);
		}
		notify(ep);
	}
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
PUBLIC int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the data store server. */
	int i, r;
	struct rprocpub rprocpub[NR_BOOT_PROCS];

	/* Reset data store: data and subscriptions. */
	for(i = 0; i < NR_DS_KEYS; i++) {
		ds_store[i].flags = 0;
	}
	for(i = 0; i < NR_DS_SUBS; i++) {
		ds_subs[i].flags = 0;
	}

	/* Map all the services in the boot image. */
	if((r = sys_safecopyfrom(RS_PROC_NR, info->rproctab_gid, 0,
		(vir_bytes) rprocpub, sizeof(rprocpub), S)) != OK) {
		panic("DS", "sys_safecopyfrom failed", r);
	}
	for(i=0;i < NR_BOOT_PROCS;i++) {
		if(rprocpub[i].in_use) {
			if((r = map_service(&rprocpub[i])) != OK) {
				panic("DS", "unable to map service", r);
			}
		}
	}

	return(OK);
}

/*===========================================================================*
 *		               map_service                                   *
 *===========================================================================*/
PUBLIC int map_service(rpub)
struct rprocpub *rpub;
{
/* Map a new service by registering its label. */
  struct data_store *dsp;

  /* Allocate a new data slot. */
  if((dsp = alloc_data_slot()) == NULL) {
	return ENOMEM;
  }

  /* Set attributes. */
  strcpy(dsp->key, rpub->label);
  dsp->u.u32 = (u32_t) rpub->endpoint;
  strcpy(dsp->owner, ds_getprocname(DS_PROC_NR));
  dsp->flags = DSF_IN_USE | DSF_TYPE_LABEL;

  /* Update subscribers having a matching subscription. */
  update_subscribers(dsp, 1);

  return(OK);
}

/*===========================================================================*
 *				do_publish				     *
 *===========================================================================*/
PUBLIC int do_publish(message *m_ptr)
{
  struct data_store *dsp;
  char key_name[DS_MAX_KEYLEN];
  int flags = m_ptr->DS_FLAGS;
  size_t length;
  int r;

  /* MAP should not be overwritten. */
  if((flags & DSF_TYPE_MAP) && (flags & DSF_OVERWRITE))
	return EINVAL;

  /* Get key name. */
  if((r = get_key_name(m_ptr, key_name)) != OK)
	return r;

  /* Lookup the entry. */
  dsp = lookup_entry(key_name, flags & DSF_MASK_TYPE);
  /* If type is LABEL, also try to lookup the entry by num. */
  if((flags & DSF_TYPE_LABEL) && (dsp == NULL))
	dsp = lookup_label_entry(m_ptr->DS_VAL);

  if(dsp == NULL) {
	/* The entry doesn't exist, allocate a new data slot. */
	if((dsp = alloc_data_slot()) == NULL)
		return ENOMEM;
  } else if (flags & DSF_OVERWRITE) {
	/* Overwrite. */
	if(!check_auth(dsp, m_ptr->m_source, DSF_PRIV_OVERWRITE))
		return EPERM;
  } else {
	/* Don't overwrite and return error. */
	return EEXIST;
  }

  /* Store! */
  switch(flags & DSF_MASK_TYPE) {
  case DSF_TYPE_U32:
  case DSF_TYPE_LABEL:
	dsp->u.u32 = m_ptr->DS_VAL;
	break;
  case DSF_TYPE_STR:
	strncpy(dsp->u.string, (char *)(&m_ptr->DS_STRING), DS_MAX_STRLEN);
	dsp->u.string[DS_MAX_KEYLEN - 1] = '\0';
	break;
  case DSF_TYPE_MEM:
	length = m_ptr->DS_VAL_LEN;
	/* Allocate a new data buffer if necessary. */
	if(!(dsp->flags & DSF_IN_USE)) {
		if((dsp->u.mem.data = malloc(length)) == NULL)
			return ENOMEM;
		dsp->u.mem.reallen = length;
	} else if(length > dsp->u.mem.reallen) {
		free(dsp->u.mem.data);
		if((dsp->u.mem.data = malloc(length)) == NULL)
			return ENOMEM;
		dsp->u.mem.reallen = length;
	}

	/* Copy the memory range. */
	r = sys_safecopyfrom(m_ptr->m_source, m_ptr->DS_VAL, 0,
		(vir_bytes) dsp->u.mem.data, length, D);
	if(r != OK) {
		printf("DS: publish: memory map/copy failed from %d: %d\n",
			m_ptr->m_source, r);
		free(dsp->u.mem.data);
		return r;
	}
	dsp->u.mem.length = length;
	break;
  case DSF_TYPE_MAP:
  	/* Allocate buffer, the address should be aligned by CLICK_SIZE. */
	length = m_ptr->DS_VAL_LEN;
	if((dsp->u.map.realpointer = malloc(length + CLICK_SIZE)) == NULL)
		return ENOMEM;
	dsp->u.map.data = (void*) CLICK_CEIL(dsp->u.map.realpointer);

	/* Map memory. */
	r = sys_safemap(m_ptr->m_source, m_ptr->DS_VAL, 0,
		(vir_bytes) dsp->u.map.data, length, D, 0);
	if(r != OK) {
		printf("DS: publish: memory map/copy failed from %d: %d\n",
			m_ptr->m_source, r);
		free(dsp->u.map.realpointer);
		return r;
	}
	dsp->u.map.length = length;
	dsp->u.map.sindex = -1;
	break;
  default:
	return EINVAL;
  }

  /* Set attributes. */
  strcpy(dsp->key, key_name);
  strcpy(dsp->owner, ds_getprocname(m_ptr->m_source));
  dsp->flags = DSF_IN_USE | (flags & DSF_MASK_INTERNAL);

  /* Update subscribers having a matching subscription. */
  update_subscribers(dsp, 1);

  return(OK);
}

/*===========================================================================*
 *				do_retrieve				     *
 *===========================================================================*/
PUBLIC int do_retrieve(message *m_ptr)
{
  struct data_store *dsp;
  char key_name[DS_MAX_KEYLEN];
  int flags = m_ptr->DS_FLAGS;
  int type = flags & DSF_MASK_TYPE;
  size_t length;
  void *data;
  int index, r;

  /* Get key name. */
  if((r = get_key_name(m_ptr, key_name)) != OK)
	return r;

  /* Lookup the entry. */
  if((dsp = lookup_entry(key_name, type)) == NULL)
	return ESRCH;
  if(!check_auth(dsp, m_ptr->m_source, DSF_PRIV_RETRIEVE))
	return EPERM;

  /* Copy the requested data. */
  switch(type) {
  case DSF_TYPE_U32:
  case DSF_TYPE_LABEL:
	m_ptr->DS_VAL = dsp->u.u32;
	break;
  case DSF_TYPE_STR:
	strncpy((char *)(&m_ptr->DS_STRING), dsp->u.string, DS_MAX_STRLEN);
	break;
  case DSF_TYPE_MEM:
	length = MIN(m_ptr->DS_VAL_LEN, dsp->u.mem.length);
	r = sys_safecopyto(m_ptr->m_source, m_ptr->DS_VAL, 0,
		(vir_bytes) dsp->u.mem.data, length, D);
	if(r != OK) {
		printf("DS: retrieve: copy failed to %d: %d\n",	
			m_ptr->m_source, r);
		return r;
	}
	m_ptr->DS_VAL_LEN = length;
	break;
  case DSF_TYPE_MAP:
	/* The caller requested to map a mapped memory range.
	 * Create a MAP grant for the caller, the caller will do the
	 * safemap itself later.
	 */
	if(flags & DSMF_MAP_MAPPED) {
		cp_grant_id_t gid;
		gid = cpf_grant_direct(m_ptr->m_source,
				(vir_bytes)dsp->u.map.data,
				dsp->u.map.length,
				CPF_READ|CPF_WRITE|CPF_MAP);
		if(!GRANT_VALID(gid))
			return -1;
		m_ptr->DS_VAL = gid;
		m_ptr->DS_VAL_LEN = dsp->u.map.length;
	}

	/* The caller requested a copy of a mapped mem range or a snapshot. */
	else if(flags & (DSMF_COPY_MAPPED|DSMF_COPY_SNAPSHOT)) {
		if(flags & DSMF_COPY_MAPPED) {
			data = dsp->u.map.data;
		} else {
			index = m_ptr->DS_NR_SNAPSHOT;
			if(check_snapshot_index(dsp, index))
				return EINVAL;
			data = dsp->u.map.snapshots[index % NR_DS_SNAPSHOT];
		}

		length = MIN(m_ptr->DS_VAL_LEN, dsp->u.map.length);
		r = sys_safecopyto(m_ptr->m_source, m_ptr->DS_VAL, 0,
			(vir_bytes) data, length, D);
		if(r != OK) {
			printf("DS: retrieve: copy failed to %d: %d\n",	
				m_ptr->m_source, r);
			return r;
		}
		m_ptr->DS_VAL_LEN = length;
	}
	else {
		return EINVAL;
	}
	break;
  default:
	return EINVAL;
  }

  return OK;
}

/*===========================================================================*
 *				do_retrieve_label			     *
 *===========================================================================*/
PUBLIC int do_retrieve_label(message *m_ptr)
{
  struct data_store *dsp;
  int r;

  /* Lookup the label entry. */
  if((dsp = lookup_label_entry(m_ptr->DS_VAL)) == NULL)
	return ESRCH;

  /* Copy the key name. */
  r = sys_safecopyto(m_ptr->m_source,
	(cp_grant_id_t) m_ptr->DS_KEY_GRANT, 0,
	(vir_bytes) dsp->key, strlen(dsp->key) + 1, D);
  if(r != OK) {
	printf("DS: copy failed from %d: %d\n", m_ptr->m_source, r);
	return r;
  }

  return OK;
}

/*===========================================================================*
 *				do_subscribe				     *
 *===========================================================================*/
PUBLIC int do_subscribe(message *m_ptr)
{
  char regex[DS_MAX_KEYLEN+3];
  struct subscription *subp;
  char errbuf[80];
  char *owner;
  int type_set;
  int r, e, b;

  /* Find the owner. */
  owner = ds_getprocname(m_ptr->m_source);
  if(owner == NULL)
	  return ESRCH;

  /* See if the owner already has an existing subscription. */
  if((subp = lookup_sub(owner)) == NULL) {
	/* The subscription doesn't exist, allocate a new one. */
	if((subp = alloc_sub_slot()) == NULL)
		return EAGAIN;
  } else if(!(m_ptr->DS_FLAGS & DSF_OVERWRITE)) {
	/* The subscription exists but we can't overwrite, return error. */
	return EEXIST;
  }

  /* Copy key name from the caller. Anchor the subscription with "^regexp$" so
   * substrings don't match. The caller will probably not expect this,
   * and the usual case is for a complete match.
   */
  regex[0] = '^';
  if((r = get_key_name(m_ptr, regex)) != OK)
	return r;
  regex[DS_MAX_KEYLEN-1] = '\0';
  strcat(regex, "$");

  /* Compile regular expression. */
  if((e=regcomp(&subp->regex, regex, REG_EXTENDED)) != 0) {
	regerror(e, &subp->regex, errbuf, sizeof(errbuf));
	printf("DS: subscribe: regerror: %s\n", errbuf);
	return EINVAL;
  }

  /* If type_set = 0, then subscribe all types. */
  type_set = m_ptr->DS_FLAGS & DSF_MASK_TYPE;
  if(type_set == 0)
	  type_set = DSF_MASK_TYPE;

  subp->flags = DSF_IN_USE | type_set;
  strcpy(subp->owner, owner);
  for(b = 0; b < BITMAP_CHUNKS(NR_DS_SUBS); b++)
	subp->old_subs[b] = 0;

  /* See if caller requested an instant initial list. */
  if(m_ptr->DS_FLAGS & DSF_INITIAL) {
	int i, match_found = FALSE;
	for(i = 0; i < NR_DS_KEYS; i++) {
		if(!(ds_store[i].flags & DSF_IN_USE))
			continue;
		if(!(ds_store[i].flags & type_set))
			continue;
		if(!check_sub_match(subp, &ds_store[i], m_ptr->m_source))
			continue;

		SET_BIT(subp->old_subs, i);
		match_found = TRUE;
	}

	/* Notify in case of match. */
	if(match_found)
		notify(m_ptr->m_source);
  }

  return OK;
}

/*===========================================================================*
 *				do_check				     *
 *===========================================================================*/
PUBLIC int do_check(message *m_ptr)
{
  struct subscription *subp;
  char *owner;
  int r, i;

  /* Find the owner. */
  owner = ds_getprocname(m_ptr->m_source);
  if(owner == NULL)
	  return ESRCH;

  /* Lookup the owner's subscription. */
  if((subp = lookup_sub(owner)) == NULL)
	return ESRCH;

  /* Look for an updated entry the subscriber is interested in. */
  for(i = 0; i < NR_DS_KEYS; i++) {
	if(GET_BIT(subp->old_subs, i))
		break;
  }
  if(i == NR_DS_KEYS)
	return ENOENT;

  /* Copy the key name. */
  r = sys_safecopyto(m_ptr->m_source,
	(cp_grant_id_t) m_ptr->DS_KEY_GRANT, 0, 
	(vir_bytes) ds_store[i].key, strlen(ds_store[i].key), D);
  if(r != OK) {
	printf("DS: check: copy failed from %d: %d\n", m_ptr->m_source, r);
	return r;
  }

  /* Copy the type. */
  m_ptr->DS_FLAGS = ds_store[i].flags & DSF_MASK_TYPE;

  /* Mark the entry as no longer updated for the subscriber. */
  UNSET_BIT(subp->old_subs, i);

  return OK;
}

/*===========================================================================*
 *				do_delete				     *
 *===========================================================================*/
PUBLIC int do_delete(message *m_ptr)
{
  struct data_store *dsp;
  char key_name[DS_MAX_KEYLEN];
  int type = m_ptr->DS_FLAGS & DSF_MASK_TYPE;
  int top, i, r;

  /* Get key name. */
  if((r = get_key_name(m_ptr, key_name)) != OK)
	return r;

  /* Lookup the entry. */
  if((dsp = lookup_entry(key_name, type)) == NULL)
	return ESRCH;

  /* Only the owner can delete. */
  if(strcmp(dsp->owner, ds_getprocname(m_ptr->m_source)))
	return EPERM;

  switch(type) {
  case DSF_TYPE_U32:
  case DSF_TYPE_STR:
  case DSF_TYPE_LABEL:
	break;
  case DSF_TYPE_MEM:
	free(dsp->u.mem.data);
	break;
  case DSF_TYPE_MAP:
	/* Unmap the mapped data. */
	r = sys_safeunmap(D, (vir_bytes)dsp->u.map.data);
	if(r != OK)
		return r;

	/* Revoke all the mapped grants. */
	r = sys_saferevmap_addr((vir_bytes)dsp->u.map.data);
	if(r != OK)
		return r;

	/* Free snapshots. */
	top = MIN(NR_DS_SNAPSHOT - 1, dsp->u.map.sindex);
	for(i = 0; i <= top; i++) {
		free(dsp->u.map.snapshots[i]);
	}

	free(dsp->u.map.realpointer);
	break;
  default:
	return EINVAL;
  }

  /* Update subscribers having a matching subscription. */
  update_subscribers(dsp, 0);

  /* Clear the entry. */
  dsp->flags = 0;

  return OK;
}

/*===========================================================================*
 *				do_snapshot				     *
 *===========================================================================*/
PUBLIC int do_snapshot(message *m_ptr)
{
  struct data_store *dsp;
  struct dsi_map *p;
  char key_name[DS_MAX_KEYLEN];
  int i, r;

  /* Get key name. */
  if((r = get_key_name(m_ptr, key_name)) != OK)
	return r;

  /* Lookup the entry. */
  if((dsp = lookup_entry(key_name, DSF_TYPE_MAP)) == NULL)
	return ESRCH;

  if(!check_auth(dsp, m_ptr->m_source, DSF_PRIV_SNAPSHOT))
	return EPERM;

  /* Find a snapshot slot. */
  p = &dsp->u.map;
  p->sindex++;
  i = p->sindex % DS_MAX_KEYLEN;
  if(p->sindex < DS_MAX_KEYLEN) {
	if((p->snapshots[i] = malloc(p->length)) == NULL) {
		p->sindex--;
		return ENOMEM;
	}
  }

  /* Store the snapshot. */
  memcpy(p->snapshots[i], p->data, p->length);

  /* Copy the snapshot index. */
  m_ptr->DS_NR_SNAPSHOT = p->sindex;

  return OK;
}

/*===========================================================================*
 *				do_getsysinfo				     *
 *===========================================================================*/
PUBLIC int do_getsysinfo(message *m_ptr)
{
  vir_bytes src_addr;
  size_t length;
  int s;

  switch(m_ptr->m1_i1) {
  case SI_DATA_STORE:
	src_addr = (vir_bytes)ds_store;
	length = sizeof(struct data_store) * NR_DS_KEYS;
	break;
  case SI_SUBSCRIPTION:
	src_addr = (vir_bytes)ds_subs;
	length = sizeof(struct subscription) * NR_DS_SUBS;
	break;
  default:
  	return EINVAL;
  }

  if (OK != (s=sys_datacopy(SELF, src_addr,
		m_ptr->m_source, (vir_bytes)m_ptr->m1_p1, length))) {
	printf("DS: copy failed: %d\n", s);
	return s;
  }

  return OK;
}

