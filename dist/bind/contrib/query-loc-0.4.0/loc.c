#include "loc.h"

/* $Id: loc.c,v 1.1 2008-02-15 01:47:15 marka Exp $ */

/* Global variables */

short rr_errno;

/*
   Prints the actual usage
 */
void
usage ()
{
  (void) fprintf (stderr,
		  "Usage: %s: [-v] [-d nnn] hostname\n", progname);
  exit (2);
}

/*
   Panics
 */
void
panic (message)
     char *message;
{
  (void) fprintf (stderr,
		  "%s: %s\n", progname, message);
  exit (2);
}

/*
   ** IN_ADDR_ARPA -- Convert dotted quad string to reverse in-addr.arpa
   ** ------------------------------------------------------------------
   **
   **   Returns:
   **           Pointer to appropriate reverse in-addr.arpa name
   **           with trailing dot to force absolute domain name.
   **           NULL in case of invalid dotted quad input string.
 */

#ifndef ARPA_ROOT
#define ARPA_ROOT "in-addr.arpa"
#endif

char *
in_addr_arpa (dottedquad)
     char *dottedquad;		/* input string with dotted quad */
{
  static char addrbuf[4 * 4 + sizeof (ARPA_ROOT) + 2];
  unsigned int a[4];
  register int n;

  n = sscanf (dottedquad, "%u.%u.%u.%u", &a[0], &a[1], &a[2], &a[3]);
  switch (n)
    {
    case 4:
      (void) sprintf (addrbuf, "%u.%u.%u.%u.%s.",
	     a[3] & 0xff, a[2] & 0xff, a[1] & 0xff, a[0] & 0xff, ARPA_ROOT);
      break;

    case 3:
      (void) sprintf (addrbuf, "%u.%u.%u.%s.",
		      a[2] & 0xff, a[1] & 0xff, a[0] & 0xff, ARPA_ROOT);
      break;

    case 2:
      (void) sprintf (addrbuf, "%u.%u.%s.",
		      a[1] & 0xff, a[0] & 0xff, ARPA_ROOT);
      break;

    case 1:
      (void) sprintf (addrbuf, "%u.%s.",
		      a[0] & 0xff, ARPA_ROOT);
      break;

    default:
      return (NULL);
    }

  while (--n >= 0)
    if (a[n] > 255)
      return (NULL);

  return (addrbuf);
}

/* 
   Returns a human-readable version of the LOC information or
   NULL if it failed. Argument is a name (of a network or a machine)
   and a boolean telling is it is a network name or a machine name.
 */
char *
getlocbyname (name, is_network)
     const char *name;
     short is_network;
{
  char *result;
  struct list_in_addr *list, *p;
  result = findRR (name, T_LOC);
  if (result != NULL)
    {
      if (debug >= 2)
	printf ("LOC record found for the name %s\n", name);
      return result;
    }
  else
    {
      if (!is_network)
	{
	  list = findA (name);
	  if (debug >= 2)
	    printf ("No LOC record found for the name %s, trying addresses\n", name);
	  if (list != NULL)
	    {
	      for (p = list; p != NULL; p = p->next)
		{
		  if (debug >= 2)
		    printf ("Trying address %s\n", inet_ntoa (p->addr));
		  result = getlocbyaddr (p->addr, NULL);
		  if (result != NULL)
		    return result;
		}
	      return NULL;
	    }
	  else
	    {
	      if (debug >= 2)
		printf ("   No A record found for %s\n", name);
	      return NULL;
	    }
	}
      else
	{
	  if (debug >= 2)
	    printf ("No LOC record found for the network name %s\n", name);
	  return NULL;
	}
    }
}

/* 
   Returns a human-readable version of the LOC information or
   NULL if it failed. Argument is an IP address.
 */
char *
getlocbyaddr (addr, mask)
     const struct in_addr addr;
     const struct in_addr *mask;
{
  struct in_addr netaddr;
  u_int32_t a;
  struct in_addr themask;
  char *text_addr, *text_mask;

  if (mask == NULL)
    {
      themask.s_addr = (u_int32_t) 0;
    }
  else
    {
      themask = *mask;
    }

  text_addr = (char *) malloc (256);
  text_mask = (char *) malloc (256);
  strcpy (text_addr, inet_ntoa (addr));
  strcpy (text_mask, inet_ntoa (themask));

  if (debug >= 2)
    printf ("Testing address %s/%s\n", text_addr, text_mask);
  if (mask == NULL)
    {
      a = ntohl (addr.s_addr);
      if (IN_CLASSA (a))
	{
	  netaddr.s_addr = htonl (a & IN_CLASSA_NET);
	  themask.s_addr = htonl(IN_CLASSA_NET);
	}
      else if (IN_CLASSB (a))
	{
	  netaddr.s_addr = htonl (a & IN_CLASSB_NET);
	  themask.s_addr = htonl(IN_CLASSB_NET);
	}
      else if (IN_CLASSC (a))
	{
	  netaddr.s_addr = htonl (a & IN_CLASSC_NET);
	  themask.s_addr = htonl(IN_CLASSC_NET);
	}
      else
	{
	  /* Error */
	  return NULL;
	}
      return getlocbynet (in_addr_arpa (inet_ntoa (netaddr)), addr, &themask);
    }
  else
    {
      netaddr.s_addr = addr.s_addr & themask.s_addr;
      return getlocbynet (in_addr_arpa (inet_ntoa (netaddr)), addr, mask);
    }
}

/* 
   Returns a human-readable LOC.
   Argument is a network name in the 0.z.y.x.in-addr.arpa format
   and the original address
 */
char *
getlocbynet (name, addr, mask)
     char *name;
     struct in_addr addr;
     struct in_addr *mask;
{
  char *network;
  char *result;
  struct list_in_addr *list;
  struct in_addr newmask;
  u_int32_t a;
  char newname[4 * 4 + sizeof (ARPA_ROOT) + 2];
  
  if (debug >= 2)
    printf ("Testing network %s with mask %s\n", name, inet_ntoa(*mask));
  
  /* Check if this network has an A RR */
  list = findA (name);
  if (list != NULL)
    {
      /* Yes, it does. This A record will be used as the
       * new mask for recursion if it is longer than
       * the actual mask. */
      if (mask != NULL && mask->s_addr < list->addr.s_addr)
	{
	  /* compute the new arguments for recursion 
	   * - compute the new network by applying the new mask
	   *   to the address and get the in_addr_arpa representation
	   *   of it.
	   * - the address remains unchanged
	   * - the new mask is the one given in the A record
	   */
	  a = ntohl(addr.s_addr);        /* start from host address */
	  a &= ntohl(list->addr.s_addr); /* apply new mask */
	  newname[sizeof newname - 1] = 0;
	  strncpy(
		 newname,
		 in_addr_arpa(inet_ntoa(inet_makeaddr(a, 0))),
		 sizeof newname);
	  newmask = inet_makeaddr(ntohl(list->addr.s_addr), 0);
	  result = getlocbynet (newname, addr, &newmask);
	  if (result != NULL)
	    {
	      return result;
	    }
	}
      /* couldn't find a LOC. Fall through and try with name */
    }

  /* Check if this network has a name */
  network = findRR (name, T_PTR);
  if (network == NULL)
    {
      if (debug >= 2)
	printf ("No name for network %s\n", name);
      return NULL;
    }
  else
    {
      return getlocbyname (network, TRUE);
    }
}

/* 
   The code for these two functions is stolen from the examples in Liu and Albitz
   book "DNS and BIND" (O'Reilly).
 */

/****************************************************************
 * skipName -- This routine skips over a domain name.  If the   *
 *     domain name expansion fails, it crashes.                 *
 *     dn_skipname() is probably not on your manual             *
 *     page; it is similar to dn_expand() except that it just   *
 *     skips over the name.  dn_skipname() is in res_comp.c if  *
 *     you need to find it.                                     *
 ****************************************************************/
int
skipName (cp, endOfMsg)
     u_char *cp;
     u_char *endOfMsg;
{
  int n;

  if ((n = dn_skipname (cp, endOfMsg)) < 0)
    {
      panic ("dn_skipname failed\n");
    }
  return (n);
}

/****************************************************************
 * skipToData -- This routine advances the cp pointer to the    *
 *     start of the resource record data portion.  On the way,  *
 *     it fills in the type, class, ttl, and data length        *
 ****************************************************************/
int
skipToData (cp, type, class, ttl, dlen, endOfMsg)
     u_char *cp;
     u_short *type;
     u_short *class;
     u_int32_t *ttl;
     u_short *dlen;
     u_char *endOfMsg;
{
  u_char *tmp_cp = cp;		/* temporary version of cp */

  /* Skip the domain name; it matches the name we looked up */
  tmp_cp += skipName (tmp_cp, endOfMsg);

  /*
   * Grab the type, class, and ttl.  GETSHORT and GETLONG
   * are macros defined in arpa/nameser.h.
   */
  GETSHORT (*type, tmp_cp);
  GETSHORT (*class, tmp_cp);
  GETLONG (*ttl, tmp_cp);
  GETSHORT (*dlen, tmp_cp);

  return (tmp_cp - cp);
}


/* 
   Returns a human-readable version of a DNS RR (resource record)
   associated with the name 'domain'.
   If it does not find, ir returns NULL and sets rr_errno to explain why.

   The code for this function is stolen from the examples in Liu and Albitz
   book "DNS and BIND" (O'Reilly).
 */
char *
findRR (domain, requested_type)
     char *domain;
     int requested_type;
{
  char *result, *message;

  union
    {
      HEADER hdr;		/* defined in resolv.h */
      u_char buf[PACKETSZ];	/* defined in arpa/nameser.h */
    }
  response;			/* response buffers */
short found = 0;  
int responseLen;		/* buffer length */

  u_char *cp;			/* character pointer to parse DNS packet */
  u_char *endOfMsg;		/* need to know the end of the message */
  u_short class;		/* classes defined in arpa/nameser.h */
  u_short type;			/* types defined in arpa/nameser.h */
  u_int32_t ttl;		/* resource record time to live */
  u_short dlen;			/* size of resource record data */

  int i, count, dup;		/* misc variables */

  char *ptrList[1];
  int ptrNum = 0;
  struct in_addr addr;

  result = (char *) malloc (256);
  message = (char *) malloc (256);
  /* 
   * Look up the records for the given domain name.
   * We expect the domain to be a fully qualified name, so
   * we use res_query().  If we wanted the resolver search 
   * algorithm, we would have used res_search() instead.
   */
  if ((responseLen =
       res_query (domain,	/* the domain we care about   */
		  C_IN,		/* Internet class records     */
		  requested_type,	/* Look up name server records */
		  (u_char *) & response,	/*response buffer */
		  sizeof (response)))	/*buffer size    */
      < 0)
    {				/*If negative    */
      rr_errno = h_errno;
      return NULL;
    }

  /*
   * Keep track of the end of the message so we don't 
   * pass it while parsing the response.  responseLen is 
   * the value returned by res_query.
   */
  endOfMsg = response.buf + responseLen;

  /*
   * Set a pointer to the start of the question section, 
   * which begins immediately AFTER the header.
   */
  cp = response.buf + sizeof (HEADER);

  /*
   * Skip over the whole question section.  The question 
   * section is comprised of a name, a type, and a class.  
   * QFIXEDSZ (defined in arpa/nameser.h) is the size of 
   * the type and class portions, which is fixed.  Therefore, 
   * we can skip the question section by skipping the 
   * name (at the beginning) and then advancing QFIXEDSZ.
   * After this calculation, cp points to the start of the 
   * answer section, which is a list of NS records.
   */
  cp += skipName (cp, endOfMsg) + QFIXEDSZ;

  count = ntohs (response.hdr.ancount) +
    ntohs (response.hdr.nscount);
  while ((--count >= 0)		/* still more records     */
	 && (cp < endOfMsg))
    {				/* still inside the packet */


      /* Skip to the data portion of the resource record */
      cp += skipToData (cp, &type, &class, &ttl, &dlen, endOfMsg);

      if (type == requested_type)
	{
	  switch (requested_type)
	    {
	    case (T_LOC):
	      loc_ntoa (cp, result);
	      return result;
	      break;
	    case (T_PTR):
	      ptrList[ptrNum] = (char *) malloc (MAXDNAME);
	      if (ptrList[ptrNum] == NULL)
		{
		  panic ("Malloc failed");
		}

	      if (dn_expand (response.buf,	/* Start of the packet   */
			     endOfMsg,	/* End of the packet     */
			     cp,	/* Position in the packet */
			     (char *) ptrList[ptrNum],	/* Result    */
			     MAXDNAME)	/* size of ptrList buffer */
		  < 0)
		{		/* Negative: error    */
		  panic ("dn_expand failed");
		}

	      /*
	       * Check the name we've just unpacked and add it to 
	       * the list if it is not a duplicate.
	       * If it is a duplicate, just ignore it.
	       */
	      for (i = 0, dup = 0; (i < ptrNum) && !dup; i++)
		dup = !strcasecmp (ptrList[i], ptrList[ptrNum]);
	      if (dup)
		free (ptrList[ptrNum]);
	      else
		ptrNum++;
	      strcpy (result, ptrList[0]);
	      return result;
	      break;
	    case (T_A):
	      bcopy ((char *) cp, (char *) &addr, INADDRSZ);
	      strcat (result, " ");
	      strcat (result, inet_ntoa (addr));
	      found = 1;
	      break;
	    default:
	      sprintf (message, "Unexpected type %u", requested_type);
	      panic (message);
	    }
	}

      /* Advance the pointer over the resource record data */
      cp += dlen;

    }				/* end of while */
  if (found) 
  return result;
else
return NULL;
}

struct list_in_addr *
findA (domain)
     char *domain;
{

  struct list_in_addr *result, *end;

  union
    {
      HEADER hdr;		/* defined in resolv.h */
      u_char buf[PACKETSZ];	/* defined in arpa/nameser.h */
    }
  response;			/* response buffers */
  int responseLen;		/* buffer length */

  u_char *cp;			/* character pointer to parse DNS packet */
  u_char *endOfMsg;		/* need to know the end of the message */
  u_short class;		/* classes defined in arpa/nameser.h */
  u_short type;			/* types defined in arpa/nameser.h */
  u_int32_t ttl;		/* resource record time to live */
  u_short dlen;			/* size of resource record data */

  int count;			/* misc variables */

  struct in_addr addr;

  end = NULL;
  result = NULL;

  /* 
   * Look up the records for the given domain name.
   * We expect the domain to be a fully qualified name, so
   * we use res_query().  If we wanted the resolver search 
   * algorithm, we would have used res_search() instead.
   */
  if ((responseLen =
       res_query (domain,	/* the domain we care about   */
		  C_IN,		/* Internet class records     */
		  T_A,
		  (u_char *) & response,	/*response buffer */
		  sizeof (response)))	/*buffer size    */
      < 0)
    {				/*If negative    */
      rr_errno = h_errno;
      return NULL;
    }

  /*
   * Keep track of the end of the message so we don't 
   * pass it while parsing the response.  responseLen is 
   * the value returned by res_query.
   */
  endOfMsg = response.buf + responseLen;

  /*
   * Set a pointer to the start of the question section, 
   * which begins immediately AFTER the header.
   */
  cp = response.buf + sizeof (HEADER);

  /*
   * Skip over the whole question section.  The question 
   * section is comprised of a name, a type, and a class.  
   * QFIXEDSZ (defined in arpa/nameser.h) is the size of 
   * the type and class portions, which is fixed.  Therefore, 
   * we can skip the question section by skipping the 
   * name (at the beginning) and then advancing QFIXEDSZ.
   * After this calculation, cp points to the start of the 
   * answer section, which is a list of NS records.
   */
  cp += skipName (cp, endOfMsg) + QFIXEDSZ;

  count = ntohs (response.hdr.ancount) +
    ntohs (response.hdr.nscount);
  while ((--count >= 0)		/* still more records     */
	 && (cp < endOfMsg))
    {				/* still inside the packet */


      /* Skip to the data portion of the resource record */
      cp += skipToData (cp, &type, &class, &ttl, &dlen, endOfMsg);

      if (type == T_A)
	{
	  bcopy ((char *) cp, (char *) &addr, INADDRSZ);
	  if (end == NULL)
	    {
	      result = (void *) malloc (sizeof (struct list_in_addr));
	      result->addr = addr;
	      result->next = NULL;
	      end = result;
	    }
	  else
	    {
	      end->next = (void *) malloc (sizeof (struct list_in_addr));
	      end = end->next;
	      end->addr = addr;
	      end->next = NULL;
	    }
	}

      /* Advance the pointer over the resource record data */
      cp += dlen;

    }				/* end of while */
  return result;
}
