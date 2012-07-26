/*
 * Copyright (C) 2001 Jeff McNeil <jeff@snapcase.g-rock.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * Change Log
 *
 * Tue May  1 19:19:54 EDT 2001 - Jeff McNeil
 * Update to objectClass code, and add_to_rr_list function
 * (I need to rename that) to support the dNSZone schema,
 * ditched dNSDomain2 schema support. Version 0.3-ALPHA
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <isc/buffer.h>
#include <isc/entropy.h>
#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/result.h>
#include <dns/rdatatype.h>

#include <ldap.h>

#define DNS_OBJECT 6
#define DNS_TOP	   2

#define VERSION    "0.4-ALPHA"

#define NO_SPEC 0 
#define WI_SPEC  1

/* Global Zone Pointer */
char *gbl_zone = NULL;

typedef struct LDAP_INFO
{
  char *dn;
  LDAPMod **attrs;
  struct LDAP_INFO *next;
  int attrcnt;
}
ldap_info;

/* usage Info */
void usage ();

/* Add to the ldap dit */
void add_ldap_values (ldap_info * ldinfo);

/* Init an ldap connection */
void init_ldap_conn ();

/* Ldap error checking */
void ldap_result_check (char *msg, char *dn, int err);

/* Put a hostname into a char ** array */
char **hostname_to_dn_list (char *hostname, char *zone, unsigned int flags);

/* Find out how many items are in a char ** array */
int get_attr_list_size (char **tmp);

/* Get a DN */
char *build_dn_from_dc_list (char **dc_list, unsigned int ttl, int flag);

/* Add to RR list */
void add_to_rr_list (char *dn, char *name, char *type, char *data,
		     unsigned int ttl, unsigned int flags);

/* Error checking */
void isc_result_check (isc_result_t res, char *errorstr);

/* Generate LDIF Format files */
void generate_ldap (dns_name_t * dnsname, dns_rdata_t * rdata,
		    unsigned int ttl);

/* head pointer to the list */
ldap_info *ldap_info_base = NULL;

char *argzone, *ldapbase, *binddn, *bindpw = NULL;
char *ldapsystem = "localhost";
static char *objectClasses[] =
  { "top", "dNSZone", NULL };
static char *topObjectClasses[] = { "top", NULL };
LDAP *conn;
unsigned int debug = 0;

#ifdef DEBUG
debug = 1;
#endif

int
main (int *argc, char **argv)
{
  isc_mem_t *mctx = NULL;
  isc_entropy_t *ectx = NULL;
  isc_result_t result;
  char *basedn;
  ldap_info *tmp;
  LDAPMod *base_attrs[2];
  LDAPMod base;
  isc_buffer_t buff;
  char *zonefile;
  char fullbasedn[1024];
  char *ctmp;
  dns_fixedname_t fixedzone, fixedname;
  dns_rdataset_t rdataset;
  char **dc_list;
  dns_rdata_t rdata = DNS_RDATA_INIT;
  dns_rdatasetiter_t *riter;
  dns_name_t *zone, *name;
  dns_db_t *db = NULL;
  dns_dbiterator_t *dbit = NULL;
  dns_dbnode_t *node;
  extern char *optarg;
  extern int optind, opterr, optopt;
  int create_base = 0;
  int topt;

  if ((int) argc < 2)
    {
      usage ();
      exit (-1);
    }

  while ((topt = getopt ((int) argc, argv, "D:w:b:z:f:h:?dcv")) != -1)
    {
      switch (topt)
	{
	case 'v':
		printf("%s\n", VERSION);
		exit(0);
	case 'c':
	  create_base++;
	  break;
	case 'd':
	  debug++;
	  break;
	case 'D':
	  binddn = strdup (optarg);
	  break;
	case 'w':
	  bindpw = strdup (optarg);
	  break;
	case 'b':
	  ldapbase = strdup (optarg);
	  break;
	case 'z':
	  argzone = strdup (optarg);
	  // We wipe argzone all to hell when we parse it for the DN */
	  gbl_zone = strdup(argzone);
	  break;
	case 'f':
	  zonefile = strdup (optarg);
	  break;
	case 'h':
	  ldapsystem = strdup (optarg);
	  break;
	case '?':
	default:
	  usage ();
	  exit (0);
	}
    }

  if ((argzone == NULL) || (zonefile == NULL))
    {
      usage ();
      exit (-1);
    }

  if (debug)
    printf ("Initializing ISC Routines, parsing zone file\n");

  result = isc_mem_create (0, 0, &mctx);
  isc_result_check (result, "isc_mem_create");

  result = isc_entropy_create(mctx, &ectx);
  isc_result_check (result, "isc_entropy_create");

  result = isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE);
  isc_result_check (result, "isc_hash_create");

  isc_buffer_init (&buff, argzone, strlen (argzone));
  isc_buffer_add (&buff, strlen (argzone));
  dns_fixedname_init (&fixedzone);
  zone = dns_fixedname_name (&fixedzone);
  result = dns_name_fromtext (zone, &buff, dns_rootname, 0, NULL);
  isc_result_check (result, "dns_name_fromtext");

  result = dns_db_create (mctx, "rbt", zone, dns_dbtype_zone,
			  dns_rdataclass_in, 0, NULL, &db);
  isc_result_check (result, "dns_db_create");

  result = dns_db_load (db, zonefile);
  isc_result_check (result, "Check Zone Syntax: dns_db_load");

  result = dns_db_createiterator (db, 0, &dbit);
  isc_result_check (result, "dns_db_createiterator");

  result = dns_dbiterator_first (dbit);
  isc_result_check (result, "dns_dbiterator_first");

  dns_fixedname_init (&fixedname);
  name = dns_fixedname_name (&fixedname);
  dns_rdataset_init (&rdataset);
  dns_rdata_init (&rdata);

  while (result == ISC_R_SUCCESS)
    {
      node = NULL;
      result = dns_dbiterator_current (dbit, &node, name);

      if (result == ISC_R_NOMORE)
	break;

      isc_result_check (result, "dns_dbiterator_current");

      riter = NULL;
      result = dns_db_allrdatasets (db, node, NULL, 0, &riter);
      isc_result_check (result, "dns_db_allrdatasets");

      result = dns_rdatasetiter_first (riter);
      //isc_result_check(result, "dns_rdatasetiter_first");

      while (result == ISC_R_SUCCESS)
	{
	  dns_rdatasetiter_current (riter, &rdataset);
	  result = dns_rdataset_first (&rdataset);
	  isc_result_check (result, "dns_rdatasetiter_current");

	  while (result == ISC_R_SUCCESS)
	    {
	      dns_rdataset_current (&rdataset, &rdata);
	      generate_ldap (name, &rdata, rdataset.ttl);
	      dns_rdata_reset (&rdata);
	      result = dns_rdataset_next (&rdataset);
	    }
	  dns_rdataset_disassociate (&rdataset);
	  result = dns_rdatasetiter_next (riter);

	}
      dns_rdatasetiter_destroy (&riter);
      result = dns_dbiterator_next (dbit);

    }

  /* Initialize the LDAP Connection */
  if (debug)
    printf ("Initializing LDAP Connection to %s as %s\n", ldapsystem, binddn);

  init_ldap_conn ();

  if (create_base)
    {
      if (debug)
	printf ("Creating base zone DN %s\n", argzone);

      dc_list = hostname_to_dn_list (argzone, argzone, DNS_TOP);
      basedn = build_dn_from_dc_list (dc_list, 0, NO_SPEC);

      for (ctmp = &basedn[strlen (basedn)]; ctmp >= &basedn[0]; ctmp--)
	{
	  if ((*ctmp == ',') || (ctmp == &basedn[0]))
	    {
	      base.mod_op = LDAP_MOD_ADD;
	      base.mod_type = "objectClass";
	      base.mod_values = topObjectClasses;
	      base_attrs[0] = &base;
	      base_attrs[1] = NULL;

	      if (ldapbase)
		{
		  if (ctmp != &basedn[0])
		    sprintf (fullbasedn, "%s,%s", ctmp + 1, ldapbase);
		  else
		    sprintf (fullbasedn, "%s,%s", ctmp, ldapbase);

		}
	      else
		{
		  if (ctmp != &basedn[0])
		    sprintf (fullbasedn, "%s", ctmp + 1);
		  else
		    sprintf (fullbasedn, "%s", ctmp);
		}
	      result = ldap_add_s (conn, fullbasedn, base_attrs);
	      ldap_result_check ("intial ldap_add_s", fullbasedn, result);
	    }

	}
    }
  else
    {
      if (debug)
	printf ("Skipping zone base dn creation for %s\n", argzone);
    }

  for (tmp = ldap_info_base; tmp != NULL; tmp = tmp->next)
    {

      if (debug)
	printf ("Adding DN: %s\n", tmp->dn);

      add_ldap_values (tmp);
    }

  if (debug)
	printf("Operation Complete.\n");

  /* Cleanup */
  isc_hash_destroy();
  isc_entropy_detach(&ectx);
  isc_mem_destroy(&mctx);

  return 0;
}


/* Check the status of an isc_result_t after any isc routines.
 * I should probably rename this function, as not to cause any
 * confusion with the isc* routines. Will exit on error. */
void
isc_result_check (isc_result_t res, char *errorstr)
{
  if (res != ISC_R_SUCCESS)
    {
      fprintf (stderr, " %s: %s\n", errorstr, isc_result_totext (res));
      exit (-1);
    }
}


/* Takes DNS information, in bind data structure format, and adds textual
 * zone information to the LDAP run queue. */
void
generate_ldap (dns_name_t * dnsname, dns_rdata_t * rdata, unsigned int ttl)
{
  unsigned char name[DNS_NAME_MAXTEXT + 1];
  unsigned int len;
  unsigned char type[20];
  unsigned char data[2048];
  char **dc_list;
  char *dn;

  isc_buffer_t buff;
  isc_result_t result;

  isc_buffer_init (&buff, name, sizeof (name));
  result = dns_name_totext (dnsname, ISC_TRUE, &buff);
  isc_result_check (result, "dns_name_totext");
  name[isc_buffer_usedlength (&buff)] = 0;

  isc_buffer_init (&buff, type, sizeof (type));
  result = dns_rdatatype_totext (rdata->type, &buff);
  isc_result_check (result, "dns_rdatatype_totext");
  type[isc_buffer_usedlength (&buff)] = 0;

  isc_buffer_init (&buff, data, sizeof (data));
  result = dns_rdata_totext (rdata, NULL, &buff);
  isc_result_check (result, "dns_rdata_totext");
  data[isc_buffer_usedlength (&buff)] = 0;

  dc_list = hostname_to_dn_list (name, argzone, DNS_OBJECT);
  len = (get_attr_list_size (dc_list) - 2);
  dn = build_dn_from_dc_list (dc_list, ttl, WI_SPEC);

  if (debug)
    printf ("Adding %s (%s %s) to run queue list.\n", dn, type, data);

  add_to_rr_list (dn, dc_list[len], type, data, ttl, DNS_OBJECT);
}


/* Locate an item in the Run queue linked list, by DN. Used by functions
 * which add items to the run queue.
 */
ldap_info *
locate_by_dn (char *dn)
{
  ldap_info *tmp;
  for (tmp = ldap_info_base; tmp != (ldap_info *) NULL; tmp = tmp->next)
    {
      if (!strncmp (tmp->dn, dn, strlen (dn)))
	return tmp;
    }
  return (ldap_info *) NULL;
}



/* Take textual zone data, and add to the LDAP Run queue. This works like so:
 * If locate_by_dn does not return, alloc a new ldap_info structure, and then
 * calloc a LDAPMod array, fill in the default "everyone needs this" information,
 * including object classes and dc's. If it locate_by_dn does return, then we'll
 * realloc for more LDAPMod structs, and appened the new data.  If an LDAPMod exists
 * for the parameter we're adding, then we'll realloc the mod_values array, and 
 * add the new value to the existing LDAPMod. Finnaly, it assures linkage exists
 * within the Run queue linked ilst*/

void
add_to_rr_list (char *dn, char *name, char *type,
		char *data, unsigned int ttl, unsigned int flags)
{
  int i;
  int x;
  ldap_info *tmp;
  int attrlist;
  char ldap_type_buffer[128];
  char charttl[64];


  if ((tmp = locate_by_dn (dn)) == NULL)
    {

      /* There wasn't one already there, so we need to allocate a new one,
       * and stick it on the list */

      tmp = (ldap_info *) malloc (sizeof (ldap_info));
      if (tmp == (ldap_info *) NULL)
	{
	  fprintf (stderr, "malloc: %s\n", strerror (errno));
	  ldap_unbind_s (conn);
	  exit (-1);
	}

      tmp->dn = strdup (dn);
      tmp->attrs = (LDAPMod **) calloc (sizeof (LDAPMod *), flags);
      if (tmp->attrs == (LDAPMod **) NULL)
	{
	  fprintf (stderr, "calloc: %s\n", strerror (errno));
	  ldap_unbind_s (conn);
	  exit (-1);
	}

      for (i = 0; i < flags; i++)
	{
	  tmp->attrs[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
	  if (tmp->attrs[i] == (LDAPMod *) NULL)
	    {
	      fprintf (stderr, "malloc: %s\n", strerror (errno));
	      exit (-1);
	    }
	}
      tmp->attrs[0]->mod_op = LDAP_MOD_ADD;
      tmp->attrs[0]->mod_type = "objectClass";

      if (flags == DNS_OBJECT)
	tmp->attrs[0]->mod_values = objectClasses;
      else
	{
	  tmp->attrs[0]->mod_values = topObjectClasses;
	  tmp->attrs[1] = NULL;
	  tmp->attrcnt = 2;
	  tmp->next = ldap_info_base;
	  ldap_info_base = tmp;
	  return;
	}

      tmp->attrs[1]->mod_op = LDAP_MOD_ADD;
      tmp->attrs[1]->mod_type = "relativeDomainName";
      tmp->attrs[1]->mod_values = (char **) calloc (sizeof (char *), 2);

      if (tmp->attrs[1]->mod_values == (char **)NULL)
	       exit(-1);

      tmp->attrs[1]->mod_values[0] = strdup (name);
      tmp->attrs[1]->mod_values[2] = NULL;

      sprintf (ldap_type_buffer, "%sRecord", type);

      tmp->attrs[2]->mod_op = LDAP_MOD_ADD;
      tmp->attrs[2]->mod_type = strdup (ldap_type_buffer);
      tmp->attrs[2]->mod_values = (char **) calloc (sizeof (char *), 2);

       if (tmp->attrs[2]->mod_values == (char **)NULL)
	       exit(-1);

      tmp->attrs[2]->mod_values[0] = strdup (data);
      tmp->attrs[2]->mod_values[1] = NULL;

      tmp->attrs[3]->mod_op = LDAP_MOD_ADD;
      tmp->attrs[3]->mod_type = "dNSTTL";
      tmp->attrs[3]->mod_values = (char **) calloc (sizeof (char *), 2);

      if (tmp->attrs[3]->mod_values == (char **)NULL)
	      exit(-1);

      sprintf (charttl, "%d", ttl);
      tmp->attrs[3]->mod_values[0] = strdup (charttl);
      tmp->attrs[3]->mod_values[1] = NULL;

      tmp->attrs[4]->mod_op = LDAP_MOD_ADD;
      tmp->attrs[4]->mod_type = "zoneName";
      tmp->attrs[4]->mod_values = (char **)calloc(sizeof(char *), 2);
      tmp->attrs[4]->mod_values[0] = gbl_zone;
      tmp->attrs[4]->mod_values[1] = NULL;

      tmp->attrs[5] = NULL;
      tmp->attrcnt = flags;
      tmp->next = ldap_info_base;
      ldap_info_base = tmp;
    }
  else
    {

      for (i = 0; tmp->attrs[i] != NULL; i++)
	{
	  sprintf (ldap_type_buffer, "%sRecord", type);
	  if (!strncmp
	      (ldap_type_buffer, tmp->attrs[i]->mod_type,
	       strlen (tmp->attrs[i]->mod_type)))
	    {
	      attrlist = get_attr_list_size (tmp->attrs[i]->mod_values);
	      tmp->attrs[i]->mod_values =
		(char **) realloc (tmp->attrs[i]->mod_values,
				   sizeof (char *) * (attrlist + 1));

	      if (tmp->attrs[i]->mod_values == (char **) NULL)
		{
		  fprintf (stderr, "realloc: %s\n", strerror (errno));
		  ldap_unbind_s (conn);
		  exit (-1);
		}
	      for (x = 0; tmp->attrs[i]->mod_values[x] != NULL; x++);

	      tmp->attrs[i]->mod_values[x] = strdup (data);
	      tmp->attrs[i]->mod_values[x + 1] = NULL;
	      return;
	    }
	}
      tmp->attrs =
	(LDAPMod **) realloc (tmp->attrs,
			      sizeof (LDAPMod) * ++(tmp->attrcnt));
      if (tmp->attrs == NULL)
	{
	  fprintf (stderr, "realloc: %s\n", strerror (errno));
	  ldap_unbind_s (conn);
	  exit (-1);
	}

      for (x = 0; tmp->attrs[x] != NULL; x++);
      tmp->attrs[x] = (LDAPMod *) malloc (sizeof (LDAPMod));
      tmp->attrs[x]->mod_op = LDAP_MOD_ADD;
      tmp->attrs[x]->mod_type = strdup (ldap_type_buffer);
      tmp->attrs[x]->mod_values = (char **) calloc (sizeof (char *), 2);
      tmp->attrs[x]->mod_values[0] = strdup (data);
      tmp->attrs[x]->mod_values[1] = NULL;
      tmp->attrs[x + 1] = NULL;
    }
}

/* Size of a mod_values list, plus the terminating NULL field. */
int
get_attr_list_size (char **tmp)
{
  int i = 0;
  char **ftmp = tmp;
  while (*ftmp != NULL)
    {
      i++;
      ftmp++;
    }
  return ++i;
}


/* take a hostname, and split it into a char ** of the dc parts,
 * example, we have www.domain.com, this function will return:
 * array[0] = com, array[1] = domain, array[2] = www. */

char **
hostname_to_dn_list (char *hostname, char *zone, unsigned int flags)
{
  char *tmp;
  static char *dn_buffer[64];
  int i = 0;
  char *zname;
  char *hnamebuff;

  zname = strdup (hostname);

  if (flags == DNS_OBJECT)
    {

      if (strlen (zname) != strlen (zone))
	{
	  tmp = &zname[strlen (zname) - strlen (zone)];
	  *--tmp = '\0';
	  hnamebuff = strdup (zname);
	  zname = ++tmp;
	}
      else
	hnamebuff = "@";
    }
  else
    {
      zname = zone;
      hnamebuff = NULL;
    }

  for (tmp = strrchr (zname, '.'); tmp != (char *) 0;
       tmp = strrchr (zname, '.'))
    {
      *tmp++ = '\0';
      dn_buffer[i++] = tmp;
    }
  dn_buffer[i++] = zname;
  dn_buffer[i++] = hnamebuff;
  dn_buffer[i] = NULL;

  return dn_buffer;
}


/* build an sdb compatible LDAP DN from a "dc_list" (char **).
 * will append dNSTTL information to each RR Record, with the 
 * exception of "@"/SOA. */

char *
build_dn_from_dc_list (char **dc_list, unsigned int ttl, int flag)
{
  int size;
  int x;
  static char dn[1024];
  char tmp[128];

  bzero (tmp, sizeof (tmp));
  bzero (dn, sizeof (dn));
  size = get_attr_list_size (dc_list);
  for (x = size - 2; x > 0; x--)
    {
    if (flag == WI_SPEC)
    {
      if (x == (size - 2) && (strncmp (dc_list[x], "@", 1) == 0) && (ttl))
	sprintf (tmp, "relativeDomainName=%s + dNSTTL=%d,", dc_list[x], ttl);
      else if (x == (size - 2))
	      sprintf(tmp, "relativeDomainName=%s,",dc_list[x]);
      else
	      sprintf(tmp,"dc=%s,", dc_list[x]);
    }
    else
    {
	    sprintf(tmp, "dc=%s,", dc_list[x]);
    }


      strncat (dn, tmp, sizeof (dn) - strlen (dn));
    }

  sprintf (tmp, "dc=%s", dc_list[0]);
  strncat (dn, tmp, sizeof (dn) - strlen (dn));

	    fflush(NULL);
  return dn;
}


/* Initialize LDAP Conn */
void
init_ldap_conn ()
{
  int result;
  conn = ldap_open (ldapsystem, LDAP_PORT);
  if (conn == NULL)
    {
      fprintf (stderr, "Error opening Ldap connection: %s\n",
	       strerror (errno));
      exit (-1);
    }

  result = ldap_simple_bind_s (conn, binddn, bindpw);
  ldap_result_check ("ldap_simple_bind_s", "LDAP Bind", result);
}

/* Like isc_result_check, only for LDAP */
void
ldap_result_check (char *msg, char *dn, int err)
{
  if ((err != LDAP_SUCCESS) && (err != LDAP_ALREADY_EXISTS))
    {
      fprintf(stderr, "Error while adding %s (%s):\n",
		      dn, msg);
      ldap_perror (conn, dn);
      ldap_unbind_s (conn);
      exit (-1);
    }
}



/* For running the ldap_info run queue. */
void
add_ldap_values (ldap_info * ldinfo)
{
  int result;
  char dnbuffer[1024];


  if (ldapbase != NULL)
    sprintf (dnbuffer, "%s,%s", ldinfo->dn, ldapbase);
  else
    sprintf (dnbuffer, "%s", ldinfo->dn);

  result = ldap_add_s (conn, dnbuffer, ldinfo->attrs);
  ldap_result_check ("ldap_add_s", dnbuffer, result);
}




/* name says it all */
void
usage ()
{
  fprintf (stderr,
	   "zone2ldap -D [BIND DN] -w [BIND PASSWORD] -b [BASE DN] -z [ZONE] -f [ZONE FILE] -h [LDAP HOST]
	   [-c Create LDAP Base structure][-d Debug Output (lots !)] \n ");}
