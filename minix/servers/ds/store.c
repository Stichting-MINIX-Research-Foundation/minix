#include "inc.h"
#include "store.h"

/* Allocate space for the data store. */
static struct data_store ds_store[NR_DS_KEYS];
static struct subscription ds_subs[NR_DS_SUBS];

/*===========================================================================*
 *			      alloc_data_slot				     *
 *===========================================================================*/
static struct data_store *alloc_data_slot(void)
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
static struct subscription *alloc_sub_slot(void)
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
static struct data_store *lookup_entry(const char *key_name, int type)
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
static struct data_store *lookup_label_entry(unsigned num)
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
static struct subscription *lookup_sub(const char *owner)
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
static char *ds_getprocname(endpoint_t e)
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
static endpoint_t ds_getprocep(const char *s)
{
/* Get a process endpoint given its name. */
	struct data_store *dsp;

	if((dsp = lookup_entry(s, DSF_TYPE_LABEL)) != NULL)
		return dsp->u.u32;
	panic("ds_getprocep: process endpoint not found");
}

/*===========================================================================*
 *				 check_auth				     *
 *===========================================================================*/
static int check_auth(const struct data_store *p, endpoint_t ep, int perm)
{
/* Check authorization for a given type of permission. */
	char *source;

	if(!(p->flags & perm))
		return 1;

	source = ds_getprocname(ep);
	return source && !strcmp(p->owner, source);
}

/*===========================================================================*
 *				get_key_name				     *
 *===========================================================================*/
static int get_key_name(const message *m_ptr, char *key_name)
{
/* Get key name given an input message. */
  int r;

  if (m_ptr->m_ds_req.key_len > DS_MAX_KEYLEN || m_ptr->m_ds_req.key_len < 2) {
	printf("DS: bogus key length (%d) from %d\n", m_ptr->m_ds_req.key_len,
		m_ptr->m_source);
	return EINVAL;
  }

  /* Copy name from caller. */
  r = sys_safecopyfrom(m_ptr->m_source,
	(cp_grant_id_t) m_ptr->m_ds_req.key_grant, 0, 
	(vir_bytes) key_name, m_ptr->m_ds_req.key_len);
  if(r != OK) {
	printf("DS: publish: copy failed from %d: %d\n", m_ptr->m_source, r);
	return r;
  }

  key_name[DS_MAX_KEYLEN-1] = '\0';

  return OK;
}

/*===========================================================================*
 *				check_sub_match				     *
 *===========================================================================*/
static int check_sub_match(const struct subscription *subp,
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
static void update_subscribers(struct data_store *dsp, int set)
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
		ipc_notify(ep);
	}
}

/*===========================================================================*
 *		               map_service                                   *
 *===========================================================================*/
static int map_service(const struct rprocpub *rpub)
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
  strcpy(dsp->owner, "rs");
  dsp->flags = DSF_IN_USE | DSF_TYPE_LABEL;

  /* Update subscribers having a matching subscription. */
  update_subscribers(dsp, 1);

  return(OK);
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *info)
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
		(vir_bytes) rprocpub, sizeof(rprocpub))) != OK) {
		panic("sys_safecopyfrom failed: %d", r);
	}
	for(i=0;i < NR_BOOT_PROCS;i++) {
		if(rprocpub[i].in_use) {
			if((r = map_service(&rprocpub[i])) != OK) {
				panic("unable to map service: %d", r);
			}
		}
	}

	return(OK);
}

/*===========================================================================*
 *				do_publish				     *
 *===========================================================================*/
int do_publish(message *m_ptr)
{
  struct data_store *dsp;
  char key_name[DS_MAX_KEYLEN];
  char *source;
  int flags = m_ptr->m_ds_req.flags;
  size_t length;
  int r;

  /* Lookup the source. */
  source = ds_getprocname(m_ptr->m_source);
  if(source == NULL)
	  return EPERM;

  /* Only RS can publish labels. */
  if((flags & DSF_TYPE_LABEL) && m_ptr->m_source != RS_PROC_NR)
	  return EPERM;

  /* Get key name. */
  if((r = get_key_name(m_ptr, key_name)) != OK)
	return r;

  /* Lookup the entry. */
  dsp = lookup_entry(key_name, flags & DSF_MASK_TYPE);
  /* If type is LABEL, also try to lookup the entry by num. */
  if((flags & DSF_TYPE_LABEL) && (dsp == NULL))
	dsp = lookup_label_entry(m_ptr->m_ds_req.val_in.ep);

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
	dsp->u.u32 = m_ptr->m_ds_req.val_in.u32;
	break;
  case DSF_TYPE_LABEL:
	dsp->u.u32 = m_ptr->m_ds_req.val_in.ep;
	break;
  case DSF_TYPE_STR:
  case DSF_TYPE_MEM:
	length = m_ptr->m_ds_req.val_len;
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
	r = sys_safecopyfrom(m_ptr->m_source, m_ptr->m_ds_req.val_in.grant,
	        0, (vir_bytes) dsp->u.mem.data, length);
	if(r != OK) {
		printf("DS: publish: memory map/copy failed from %d: %d\n",
			m_ptr->m_source, r);
		free(dsp->u.mem.data);
		return r;
	}
	dsp->u.mem.length = length;
	if(flags & DSF_TYPE_STR) {
		((char*)dsp->u.mem.data)[length-1] = '\0';
	}
	break;
  default:
	return EINVAL;
  }

  /* Set attributes. */
  strcpy(dsp->key, key_name);
  strcpy(dsp->owner, source);
  dsp->flags = DSF_IN_USE | (flags & DSF_MASK_INTERNAL);

  /* Update subscribers having a matching subscription. */
  update_subscribers(dsp, 1);

  return(OK);
}

/*===========================================================================*
 *				do_retrieve				     *
 *===========================================================================*/
int do_retrieve(message *m_ptr)
{
  struct data_store *dsp;
  char key_name[DS_MAX_KEYLEN];
  int flags = m_ptr->m_ds_req.flags;
  int type = flags & DSF_MASK_TYPE;
  size_t length;
  int r;

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
	m_ptr->m_ds_reply.val_out.u32 = dsp->u.u32;
	break;
  case DSF_TYPE_LABEL:
	m_ptr->m_ds_reply.val_out.ep = dsp->u.u32;
	break;
  case DSF_TYPE_STR:
  case DSF_TYPE_MEM:
	length = MIN(m_ptr->m_ds_req.val_len, dsp->u.mem.length);
	r = sys_safecopyto(m_ptr->m_source, m_ptr->m_ds_req.val_in.grant, 0,
		(vir_bytes) dsp->u.mem.data, length);
	if(r != OK) {
		printf("DS: retrieve: copy failed to %d: %d\n",	
			m_ptr->m_source, r);
		return r;
	}
	m_ptr->m_ds_reply.val_len = length;
	break;
  default:
	return EINVAL;
  }

  return OK;
}

/*===========================================================================*
 *				do_retrieve_label			     *
 *===========================================================================*/
int do_retrieve_label(const message *m_ptr)
{
  struct data_store *dsp;
  int r;

  /* Lookup the label entry. */
  if((dsp = lookup_label_entry(m_ptr->m_ds_req.val_in.ep)) == NULL)
	return ESRCH;

  /* Copy the key name. */
  r = sys_safecopyto(m_ptr->m_source,
	(cp_grant_id_t) m_ptr->m_ds_req.key_grant, (vir_bytes) 0,
	(vir_bytes) dsp->key, strlen(dsp->key) + 1);
  if(r != OK) {
	printf("DS: copy failed from %d: %d\n", m_ptr->m_source, r);
	return r;
  }

  return OK;
}

/*===========================================================================*
 *				do_subscribe				     *
 *===========================================================================*/
int do_subscribe(message *m_ptr)
{
  char regex[DS_MAX_KEYLEN+2];
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
  } else if(!(m_ptr->m_ds_req.flags & DSF_OVERWRITE)) {
	/* The subscription exists but we can't overwrite, return error. */
	return EEXIST;
  }

  /* Copy key name from the caller. Anchor the subscription with "^regexp$" so
   * substrings don't match. The caller will probably not expect this,
   * and the usual case is for a complete match.
   */
  regex[0] = '^';
  if((r = get_key_name(m_ptr, regex+1)) != OK)
	return r;
  strcat(regex, "$");

  /* Compile regular expression. */
  if((e=regcomp(&subp->regex, regex, REG_EXTENDED)) != 0) {
	regerror(e, &subp->regex, errbuf, sizeof(errbuf));
	printf("DS: subscribe: regerror: %s\n", errbuf);
	return EINVAL;
  }

  /* If type_set = 0, then subscribe all types. */
  type_set = m_ptr->m_ds_req.flags & DSF_MASK_TYPE;
  if(type_set == 0)
	  type_set = DSF_MASK_TYPE;

  subp->flags = DSF_IN_USE | type_set;
  strcpy(subp->owner, owner);
  for(b = 0; b < BITMAP_CHUNKS(NR_DS_KEYS); b++)
	subp->old_subs[b] = 0;

  /* See if caller requested an instant initial list. */
  if(m_ptr->m_ds_req.flags & DSF_INITIAL) {
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
		ipc_notify(m_ptr->m_source);
  }

  return OK;
}

/*===========================================================================*
 *				do_check				     *
 *===========================================================================*/
int do_check(message *m_ptr)
{
  struct subscription *subp;
  char *owner;
  endpoint_t entry_owner_e;
  int r, i;

  /* Find the subscription owner. */
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
	(cp_grant_id_t) m_ptr->m_ds_req.key_grant, (vir_bytes) 0, 
	(vir_bytes) ds_store[i].key, strlen(ds_store[i].key) + 1);
  if(r != OK) {
	printf("DS: check: copy failed from %d: %d\n", m_ptr->m_source, r);
	return r;
  }

  /* Copy the type and the owner of the original entry. */
  entry_owner_e = ds_getprocep(ds_store[i].owner);
  m_ptr->m_ds_req.flags = ds_store[i].flags & DSF_MASK_TYPE;
  m_ptr->m_ds_req.owner = entry_owner_e;

  /* Mark the entry as no longer updated for the subscriber. */
  UNSET_BIT(subp->old_subs, i);

  return OK;
}

/*===========================================================================*
 *				do_delete				     *
 *===========================================================================*/
int do_delete(message *m_ptr)
{
  struct data_store *dsp;
  char key_name[DS_MAX_KEYLEN];
  char *source;
  char *label;
  int type = m_ptr->m_ds_req.flags & DSF_MASK_TYPE;
  int i, r;

  /* Lookup the source. */
  source = ds_getprocname(m_ptr->m_source);
  if(source == NULL)
	  return EPERM;

  /* Get key name. */
  if((r = get_key_name(m_ptr, key_name)) != OK)
	return r;

  /* Lookup the entry. */
  if((dsp = lookup_entry(key_name, type)) == NULL)
	return ESRCH;

  /* Only the owner can delete. */
  if(strcmp(dsp->owner, source))
	return EPERM;

  switch(type) {
  case DSF_TYPE_U32:
	break;
  case DSF_TYPE_LABEL:
	label = dsp->key;

	/* Clean up subscriptions. */
	for (i = 0; i < NR_DS_SUBS; i++) {
		if ((ds_subs[i].flags & DSF_IN_USE)
			&& !strcmp(ds_subs[i].owner, label)) {
			ds_subs[i].flags = 0;
		}
	}

	/* Clean up data entries. */
	for (i = 0; i < NR_DS_KEYS; i++) {
		if ((ds_store[i].flags & DSF_IN_USE)
			&& !strcmp(ds_store[i].owner, label)) {
			update_subscribers(&ds_store[i], 0);

			ds_store[i].flags = 0;
		}
	}
	break;
  case DSF_TYPE_STR:
  case DSF_TYPE_MEM:
	free(dsp->u.mem.data);
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
 *				do_getsysinfo				     *
 *===========================================================================*/
int do_getsysinfo(const message *m_ptr)
{
  vir_bytes src_addr;
  size_t length;
  int s;

  switch(m_ptr->m_lsys_getsysinfo.what) {
  case SI_DATA_STORE:
	src_addr = (vir_bytes)ds_store;
	length = sizeof(struct data_store) * NR_DS_KEYS;
	break;
  default:
  	return EINVAL;
  }

  if (length != m_ptr->m_lsys_getsysinfo.size)
	return EINVAL;

  if (OK != (s=sys_datacopy(SELF, src_addr,
		m_ptr->m_source, m_ptr->m_lsys_getsysinfo.where, length))) {
	printf("DS: copy failed: %d\n", s);
	return s;
  }

  return OK;
}

