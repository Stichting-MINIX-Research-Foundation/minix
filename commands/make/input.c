/*************************************************************************
 *
 *  m a k e :   i n p u t . c
 *
 *  Parse a makefile
 *========================================================================
 * Edition history
 *
 *  #    Date                         Comments                       By
 * --- -------- ---------------------------------------------------- ---
 *   1    ??                                                         ??
 *   2 23.08.89 new name tree structure introduced to speed up make,
 *              testname introduced to shrink the memory usage       RAL
 *   3 30.08.89 indention changed                                    PSH,RAL
 *   4 03.09.89 fixed LZ eliminated                                  RAL
 *   5 06.09.89 ; command added                                      RAL
 * ------------ Version 2.0 released ------------------------------- RAL
 *
 *************************************************************************/


#include "h.h"


static struct name *lastrrp;
static struct name *freerp = (struct name *)NULL;

void init()
{
  if( (suffparray = (struct name **) malloc( sizesuffarray *
           sizeof(struct name *)))  == (struct name **) NULL)
     fatal("No memory for suffarray",(char *)0,0);
  if ((*suffparray = (struct name *)malloc(sizeof (struct name)))
                          == (struct name *)0)
     fatal("No memory for name",(char *)0,0);
  (*suffparray)->n_next = (struct name *)0;

  if ((str1 = (char *) malloc(LZ1)) == ((char *)0))
     fatal("No memory for str1",(char *)0,0);
  str1s.ptr = &str1;
  str1s.len = LZ1;
  if ((str2 = (char *) malloc(LZ2)) == (char *)0)
     fatal("No memory for str2",(char *)0,0);
  str2s.ptr = &str2;
  str2s.len = LZ2;
}

void strrealloc(strs)
struct str *strs;
{
  strs->len *= 2;
  *strs->ptr = (char *) realloc(*strs->ptr, strs->len + 16);
  if(*strs->ptr == (char *) NULL)
       fatal("No memory for string reallocation",(char *)0,0);
}

/*
 *	Intern a name.  Return a pointer to the name struct
 */
struct name *newname(name)
char *name;
{
  register struct name *rp;
  register struct name *rrp;
  register char        *cp;

  register int           i;
  register char         *suff;   /* ptr. to suffix in current name */
  register struct name **sp;     /* ptr. to ptr. to chain of names */

  if ( (suff = suffix(name)) != (char *)NULL) {
     for (i = 1, sp = suffparray, sp++;
          i <= maxsuffarray && strcmp(suff, (*sp)->n_name) != 0;
          sp++,i++);
     if (i > maxsuffarray) {
        if ( i >= sizesuffarray) { /* must realloc suffarray */
           sizesuffarray *= 2;
           if( (suffparray = (struct name **) realloc((char *) suffparray,
                sizesuffarray * sizeof(struct name *))) == (struct name **) NULL)
              fatal("No memory for suffarray",(char *)0,0);
        }
        maxsuffarray++;
        sp = &suffparray[i];
        if ((*sp = (struct name *)malloc(sizeof (struct name)))
                                   == (struct name *)0)
           fatal("No memory for name",(char *)0,0);
        (*sp)->n_next = (struct name *)0;
        if ((cp = (char *) malloc(strlen(suff)+1)) == (char *)0)
           fatal("No memory for name",(char *)0,0);
        strcpy(cp, suff);
        (*sp)->n_name = cp;
     }
  }
  else
     sp = suffparray;

  for ( rp = (*sp)->n_next, rrp = *sp; rp; rp = rp->n_next, rrp = rrp->n_next )
     if (strcmp(name, rp->n_name) == 0)  return rp;

  if ( freerp ==  (struct name *)NULL) {
     if ((rp = (struct name *)malloc(sizeof (struct name))) == (struct name *)0)
        fatal("No memory for name",(char *)0,0);
  }
  else  {
     rp = freerp;
     freerp =  (struct name *)NULL;
  }
  rrp->n_next = rp;
  rp->n_next = (struct name *)0;
  if ((cp = (char *) malloc(strlen(name)+1)) == (char *)0)
     fatal("No memory for name",(char *)0,0);
  strcpy(cp, name);
  rp->n_name = cp;
  rp->n_line = (struct line *)0;
  rp->n_time = (time_t)0;
  rp->n_flag = 0;
  lastrrp = rrp;

  return rp;
}

/*
 *     Test a name.
 *     If the name already exists return the ptr. to its name structure.
 *     Else if the file exists 'intern' the name and return the ptr.
 *     Otherwise don't waste memory and return a NULL pointer
 */
struct name *testname(name)
char *name;
{
  register struct name *rp;

  lastrrp = (struct name *)NULL;
  rp = newname( name);
  if (rp->n_line || rp->n_flag & N_EXISTS)
     return(rp);
  modtime(rp);
  if (rp->n_flag & N_EXISTS)
     return(rp);
  if (lastrrp != (struct name *)NULL) {
     free (rp->n_name);
     lastrrp->n_next = (struct name *)NULL;
     freerp = rp;
  }
  return((struct name *)NULL);
}



/*
 *	Add a dependant to the end of the supplied list of dependants.
 *	Return the new head pointer for that list.
 */
struct depend *newdep(np, dp)
struct name   *np;
struct depend *dp;
{
  register struct depend *rp;
  register struct depend *rrp;


  if ((rp = (struct depend *)malloc(sizeof (struct depend)))
          == (struct depend *)0)
	fatal("No memory for dependant",(char *)0,0);
  rp->d_next = (struct depend *)0;
  rp->d_name = np;

  if (dp == (struct depend *)0)  return rp;

  for (rrp = dp; rrp->d_next; rrp = rrp->d_next) ;

  rrp->d_next = rp;

  return dp;
}


/*
 *	Add a command to the end of the supplied list of commands.
 *	Return the new head pointer for that list.
 */
struct cmd *newcmd(str, cp)
char       *str;
struct cmd *cp;
{
  register struct cmd *rp;
  register struct cmd *rrp;
  register char       *rcp;


  if (rcp = strrchr(str, '\n'))  *rcp = '\0';	/*  Loose newline  */

  while (isspace(*str))  str++;

  if (*str == '\0')  return cp;		/*  If nothing left, the exit  */

  if ((rp = (struct cmd *)malloc(sizeof (struct cmd))) == (struct cmd *)0)
	fatal("No memory for command",(char *)0,0);
  rp->c_next = (struct cmd *)0;
  if ((rcp = (char *) malloc(strlen(str)+1)) == (char *)0)
	fatal("No memory for command",(char *)0,0);
  strcpy(rcp, str);
  rp->c_cmd = rcp;

  if (cp == (struct cmd *)0)  return rp;

  for (rrp = cp; rrp->c_next; rrp = rrp->c_next) ;

  rrp->c_next = rp;

  return cp;
}


/*
 *	Add a new 'line' of stuff to a target.  This check to see
 *	if commands already exist for the target.  If flag is set,
 *	the line is a double colon target.
 *
 *	Kludges:
 *	i)  If the new name begins with a '.', and there are no dependents,
 *	    then the target must cease to be a target.  This is for .SUFFIXES.
 *	ii) If the new name begins with a '.', with no dependents and has
 *	    commands, then replace the current commands.  This is for
 *	    redefining commands for a default rule.
 *	Neither of these free the space used by dependents or commands,
 *	since they could be used by another target.
 */

void newline(np, dp, cp, flag)
struct name   *np;
struct depend *dp;
struct cmd    *cp;
int            flag;
{
  bool                  hascmds = FALSE;  /*  Target has commands  */
  register struct line *rp;
  register struct line *rrp;


  /* Handle the .SUFFIXES case */
  if (np->n_name[0] == '.' && !dp && !cp) {
	for (rp = np->n_line; rp; rp = rrp) {
		rrp = rp->l_next;
		free(rp);
	}
	np->n_line = (struct line *)0;
	np->n_flag &= ~N_TARG;
	return;
  }

  /* This loop must happen since rrp is used later. */
  for ( rp = np->n_line, rrp = (struct line *)0; rp; rrp = rp, rp = rp->l_next)
	if (rp->l_cmd)  hascmds = TRUE;

  if (hascmds && cp && !(np->n_flag & N_DOUBLE))
	/* Handle the implicit rules redefinition case */
	if (np->n_name[0] == '.' && dp == (struct depend *)0) {
		np->n_line->l_cmd = cp;
		return;
	}
	else
		error("Commands defined twice for target %s", np->n_name);
  if (np->n_flag & N_TARG)
	if (!(np->n_flag & N_DOUBLE) != !flag)		/* like xor */
		error("Inconsistent rules for target %s", np->n_name);

  if ((rp = (struct line *)malloc(sizeof (struct line))) == (struct line *)0)
	fatal("No memory for line",(char *)0,0);
  rp->l_next = (struct line *)0;
  rp->l_dep = dp;
  rp->l_cmd = cp;

  if (rrp)
         rrp->l_next = rp;
  else
         np->n_line = rp;

  np->n_flag |= N_TARG;
  if (flag)  np->n_flag |= N_DOUBLE;
}


/*
 *	Parse input from the makefile, and construct a tree structure
 *	of it.
 */
void input(fd)
FILE *fd;
{
  char          *p;		/*  General  */
  char          *q;
  register char *a;
  struct name   *np;
  struct depend *dp;
  struct cmd    *cp;
  bool dbl;


  if (getline(&str1s, fd))  return;	/*  Read the first line  */

  for(;;) {
	if (*str1 == TABCHAR)	/*  Rules without targets  */
		error("Rules not allowed here",(char *)0);

	p = str1;

	while (isspace(*p))  p++;	/*  Find first target  */


	while (((q = strchr(p, '=')) != (char *)0) &&
	    (p != q) && (q[-1] == '\\'))	/*  Find value */
	{
		a = q - 1;	/*  Del \ chr; move rest back  */
		p = q;
		while(*a++ = *q++)
			;
	}

	if (q != (char *)0) {

		*q++ = '\0';		/*  Separate name and val  */
		while (isspace(*q))
			q++;
		if (p = strrchr(q, '\n'))
			*p = '\0';

		p = str1;
		if ((a = gettok(&p)) == (char *)0)
			error("No macro name",(char *)0);

		setmacro(a, q);

		if (getline(&str1s, fd))
			return;
		continue;
	}

	/* include? */
	p = str1;
	while (isspace(*p)) p++;
	if (strncmp(p, "include", 7) == 0 && isspace(p[7])) {
		char *old_makefile = makefile;
		int old_lineno = lineno;
		FILE *ifd;

		p += 8;
		memmove(str1, p, strlen(p)+1);
		expand(&str1s);
		p = str1;
		while (isspace(*p)) p++;

		if ((q = malloc(strlen(p)+1)) == (char *)0)
			fatal("No memory for include",(char *)0,0);

		strcpy(q, p);
		p = q;
		while ((makefile = gettok(&q)) != (char *)0) {
			if ((ifd = fopen(makefile, "r")) == (FILE *)0)
				fatal("Can't open %s: %s", makefile, errno);
			lineno = 0;
			input(ifd);
			fclose(ifd);
		}
		free(p);
		makefile = old_makefile;
		lineno = old_lineno;

		if (getline(&str1s, fd))
			return;
		continue;
	}

	/* Search for commands on target line --- do not expand them ! */
	q = str1;
	cp = (struct cmd *)0;
	if ((a = strchr(q, ';')) != (char *)0) {
		*a++ = '\0';	/*  Separate dependents and commands */
		if ( a) cp = newcmd(a, cp);
	}

	expand(&str1s);
	p = str1;

	while (isspace(*p)) p++;

	while (((q = strchr(p, ':')) != (char *)0) &&
	    (p != q) && (q[-1] == '\\'))	/*  Find dependents  */
	{
		a = q - 1;	/*  Del \ chr; move rest back  */
		p = q;
		while(*a++ = *q++) ;
	}

	if (q == (char *)0)
		error("No targets provided",(char *)0);

	*q++ = '\0';	/*  Separate targets and dependents  */

	if (*q == ':') {		/* Double colon */
		dbl = 1;
		q++;
	}
	else
		dbl = 0;

	for (dp = (struct depend *)0; ((p = gettok(&q)) != (char *)0);)
				/*  get list of dep's */
	{
		np = newname(p);		/*  Intern name  */
		dp = newdep(np, dp);		/*  Add to dep list */
	}

	*((q = str1) + strlen(str1) + 1) = '\0';
		/*  Need two nulls for gettok (Remember separation)  */

	if (getline(&str2s, fd) == FALSE) {		/*  Get commands  */
		while (*str2 == TABCHAR) {
			cp = newcmd(&str2[0], cp);
			if (getline(&str2s, fd))
				break;
		}
	}

	while ((p = gettok(&q)) != (char *)0)	/* Get list of targ's */
	{
		np = newname(p);		/*  Intern name  */
		newline(np, dp, cp, dbl);
		if (!firstname && p[0] != '.')
			firstname = np;
	}

	if (feof(fd))				/*  EOF?  */
		return;

	while (strlen(str2) >= str1s.len) strrealloc(&str1s);
	strcpy(str1, str2);
  }
}
