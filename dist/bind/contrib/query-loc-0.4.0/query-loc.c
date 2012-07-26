#include        "loc.h"

/* $Id: query-loc.c,v 1.1 2008-02-15 01:47:15 marka Exp $ */

/* Global variables */
char *progname;
short debug;

int
main (argc, argv)
     int argc;
     char *argv[];
{
  extern char *optarg;
  extern int optind;

  short verbose = FALSE;
  char *host;

  char ch;

  char *loc = NULL;
  struct in_addr addr;
  struct hostent *hp;

  progname = argv[0];
  while ((ch = getopt (argc, argv, "vd:")) != EOF)
    {
      switch (ch)
	{
	case 'v':
	  verbose = TRUE;
	  break;
	case 'd':
	  debug = atoi (optarg);
	  if (debug <= 0)
	    {
	      (void) fprintf (stderr,
			      "%s: illegal debug value.\n", progname);
	      exit (2);
	    }
	  break;
	default:
	  usage ();
	}
    }
  argc -= optind;
  argv += optind;
  if (argc != 1)
    {
      usage ();
    }
  if (verbose || debug)
    {
      printf ("\nThis is %s, version %s.\n\n", progname, VERSION);
    }
  host = argv[0];
  (void) res_init ();

  if ((addr.s_addr = inet_addr (host)) == INADDR_NONE)
    {
      if (debug >= 1)
	printf ("%s is a name\n", host);
      loc = getlocbyname (host, FALSE);
    }
  else
    {
      if (debug >= 1)
	printf ("%s is an IP address ", host);
      hp = (struct hostent *) gethostbyaddr
	((char *) &addr, sizeof (addr), AF_INET);
      if (hp)
	{
	  if (debug >= 1)
	    printf ("and %s is its official name\n",
		    hp->h_name);
	  loc = getlocbyname (hp->h_name, FALSE);
	}
      else
	{
	  if (debug >= 1)
	    printf ("which has no name\n");
	  loc = getlocbyaddr (addr, NULL);
	}
    }
  if (loc == NULL)
    {
      printf ("No LOCation found for %s\n", host);
      exit (1);
    }
  else
    {
      if (verbose || debug)
	printf ("LOCation for %s is ", host);
      printf ("%s\n", loc);
      exit (0);
    }
}
