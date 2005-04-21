/*************************************************************************
 *
 *  m a k e :   m a k e . c
 *
 *  Do the actual making for make plus system dependent stuff
 *========================================================================
 * Edition history
 *
 *  #    Date                         Comments                       By
 * --- -------- ---------------------------------------------------- ---
 *   1    ??                                                         ??
 *   2 01.07.89 $<,$* bugs fixed                                     RAL
 *   3 23.08.89 (time_t)time((time_t*)0) bug fixed, N_EXISTS added   RAL
 *   4 30.08.89 leading sp. in cmd. output eliminated, indention ch. PSH,RAL
 *   5 03.09.89 :: time fixed, error output -> stderr, N_ERROR intr.
 *              fixed LZ elimintaed                                  RAL
 *   6 07.09.89 implmacro, DF macros,debug stuff added               RAL
 *   7 09.09.89 tos support added                                    PHH,RAL
 *   8 17.09.89 make1 arg. fixed, N_EXEC introduced                  RAL
 * ------------ Version 2.0 released ------------------------------- RAL
 *     18.05.90 fixed -n bug with silent rules.  (Now echos them.)   PAN
 *
 *************************************************************************/

#include "h.h"
#include <sys/wait.h>
#include <unistd.h>

_PROTOTYPE(static void tellstatus, (FILE *out, char *name, int status));

static bool  execflag;

/*
 *	Exec a shell that returns exit status correctly (/bin/esh).
 *	The standard EON shell returns the process number of the last
 *	async command, used by the debugger (ugg).
 *	[exec on eon is like a fork+exec on unix]
 */
int dosh(string, shell)
char *string;
char *shell;
{
  int number;

#ifdef unix
  return system(string);
#endif
#ifdef tos
  return Tosexec(string);
#endif
#ifdef eon
  return ((number = execl(shell, shell,"-c", string, 0)) == -1) ?
	-1:	/* couldn't start the shell */
	wait(number);	/* return its exit status */
#endif
#ifdef os9
  int	status, pid;

  strcat(string, "\n");
  if ((number = os9fork(shell, strlen(string), string, 0, 0, 0)) == -1)
	return -1;		/* Couldn't start a shell */
  do {
	if ((pid = wait(&status)) == -1)
		return -1;	/* child already died!?!? */
  } while (pid != number);

  return status;
#endif
}


#ifdef unix
/*
 *    Make a file look very outdated after an error trying to make it.
 *    Don't remove, this keeps hard links intact.  (kjb)
 */
int makeold(name) char *name;
{
  struct utimbuf a;

  a.actime = a.modtime = 0;	/* The epoch */

  return utime(name, &a);
}
#endif


static void tellstatus(out, name, status)
FILE *out;
char *name;
int status;
{
  char cwd[PATH_MAX];

  fprintf(out, "%s in %s: ",
	name, getcwd(cwd, sizeof(cwd)) == NULL ? "?" : cwd);

  if (WIFEXITED(status)) {
	fprintf(out, "Exit code %d", WEXITSTATUS(status));
  } else {
	fprintf(out, "Signal %d%s",
		WTERMSIG(status), status & 0x80 ? " - core dumped" : "");
  }
}


/*
 *	Do commands to make a target
 */
void docmds1(np, lp)
struct name *np;
struct line *lp;
{
  register char       *q;
  register char       *p;
  register struct cmd *cp;
  bool                 ssilent;
  bool                 signore;
  int                  estat;
  char                *shell;


  if (*(shell = getmacro("SHELL")) == '\0')
#ifdef eon
	shell = ":bin/esh";
#endif
#ifdef unix
	shell = "/bin/sh";
#endif
#ifdef os9
	shell = "shell";
#endif
#ifdef tos
	shell = "DESKTOP";      /* TOS has no shell */
#endif

  for (cp = lp->l_cmd; cp; cp = cp->c_next) {
	execflag = TRUE;
	strcpy(str1, cp->c_cmd);
	expmake = FALSE;
	expand(&str1s);
	q = str1;
	ssilent = silent;
	signore = ignore;
	while ((*q == '@') || (*q == '-')) {
		if (*q == '@')	   /*  Specific silent  */
			ssilent = TRUE;
		else		   /*  Specific ignore  */
			signore = TRUE;
		if (!domake) putchar(*q);  /* Show all characters. */
		q++;		   /*  Not part of the command  */
	}

	for (p=q; *p; p++) {
		if (*p == '\n' && p[1] != '\0') {
			*p = ' ';
			if (!ssilent || !domake)
				fputs("\\\n", stdout);
		}
		else if (!ssilent || !domake)
			putchar(*p);
	}
	if (!ssilent || !domake)
		putchar('\n');

	if (domake || expmake) {	/*  Get the shell to execute it  */
		fflush(stdout);
		if ((estat = dosh(q, shell)) != 0) {
		    if (estat == -1)
			fatal("Couldn't execute %s", shell,0);
		    else if (signore) {
			tellstatus(stdout, myname, estat);
			printf(" (Ignored)\n");
		    } else {
			tellstatus(stderr, myname, estat);
			fprintf(stderr, "\n");
			if (!(np->n_flag & N_PREC))
#ifdef unix
			    if (makeold(np->n_name) == 0)
				fprintf(stderr,"%s: made '%s' look old.\n", myname, np->n_name);
#else
			    if (unlink(np->n_name) == 0)
				fprintf(stderr,"%s: '%s' removed.\n", myname, np->n_name);
#endif
			if (!conterr) exit(estat != 0);
			np->n_flag |= N_ERROR;
			return;
		    }
		}
	}
  }
}


void docmds(np)
struct name *np;
{
  register struct line *lp;

  for (lp = np->n_line; lp; lp = lp->l_next)
	docmds1(np, lp);
}

#ifdef tos
/*
 *      execute the command submitted by make,
 *      needed because TOS has no internal shell,
 *      so we use Pexec to do the job
 *        v 1.1 of 10/sep/89 by yeti
 */

#define DELM1 ';'
#define DELM2 ' '
#define DELM3 ','

int Tosexec(string)
char *string;
{
  register char *help, *help2, c;
  register unsigned char l=1;
  char progname[80], command[255], plain[15];
  static char **envp,*env;
  register int error,i;

  /* generate strange TOS environment (RAL) */
  for ( i = 0, envp = environ; *envp; envp++) i += strlen(*envp) +1;
  if ((env = malloc(i+1)) == (char *)0)
     fatal("No memory for TOS environment",(char *)0,0);
  for ( envp = environ, help = env; *envp; envp++) {
     strcpy ( help, *envp);
     while ( *(help++)) ;
  }
  *help = '\0';

  help = progname;
  while((*help++=*string++) != ' '); /* progname is command name */
  *--help = '\0';

  l = strlen(string);             /* build option list */
  command[0] = l;                 /* TOS likes it complicated */
  strcpy(&command[1],string);
  if ((error = (int) Pexec(0,progname,command,env)) != -33) {
    free(env);
    return(error);
  }

  /* could'nt find program, try to search the PATH */
  if((help=strrchr(progname,'\\')) != (char *) 0)  /* just the */
          strcpy(plain,++help);                     /* name     */
  else if((help=strrchr(progname,'/')) != (char *) 0)
          strcpy(plain,++help);
  else if((help=strrchr(progname,':')) != (char *) 0)
          strcpy(plain,++help);
  else
          strcpy(plain,progname);

  if(*(help=getmacro("PATH")) == '\0') {
    free(env);
    return(-33);
  }
  c = 1;
  while(c)
  {       help2 = &progname[-1];
          i = 0;
          while((c=*help++) != '\0' && i<80 && c != DELM1
                 && c != DELM2 && c != DELM3)
                  *++help2 = c, i++;
          *++help2 = '\\';
          strcpy(++help2,plain);
          if((error=(int) Pexec(0,progname,command,env))!=-33) {
                  free(env);
                  return(error);
          }
  }
  free(env);
  return(-33);
}


/* (stolen from ZOO -- thanks to Rahul Dehsi)
Function mstonix() accepts an MSDOS format date and time and returns
a **IX format time.  No adjustment is done for timezone.
*/

time_t mstonix (date, time)
unsigned int date, time;
{
   int year, month, day, hour, min, sec, daycount;
   time_t longtime;
   /* no. of days to beginning of month for each month */
   static int dsboy[12] = { 0, 31, 59, 90, 120, 151, 181, 212,
                              243, 273, 304, 334};

   if (date == 0 && time == 0)			/* special case! */
      return (0L);

   /* part of following code is common to zoolist.c */
   year  =  (((unsigned int) date >> 9) & 0x7f) + 1980;
   month =  ((unsigned int) date >> 5) & 0x0f;
   day   =  date        & 0x1f;

   hour =  ((unsigned int) time >> 11)& 0x1f;
   min   =  ((unsigned int) time >> 5) & 0x3f;
   sec   =  ((unsigned int) time & 0x1f) * 2;

/* DEBUG and leap year fixes thanks to Mark Alexander <uunet!amdahl!drivax!alexande>*/
#ifdef DEBUG
   printf ("mstonix:  year=%d  month=%d  day=%d  hour=%d  min=%d  sec=%d\n",
         year, month, day, hour, min, sec);
#endif
   /* Calculate days since 1970/01/01 */
   daycount = 365 * (year - 1970) +    /* days due to whole years */
               (year - 1969) / 4 +     /* days due to leap years */
               dsboy[month-1] +        /* days since beginning of this year */
               day-1;                  /* days since beginning of month */

   if (year % 4 == 0 &&
       year % 400 != 0 && month >= 3)  /* if this is a leap year and month */
      daycount++;                      /* is March or later, add a day */

   /* Knowing the days, we can find seconds */
   longtime = daycount * 24L * 60L * 60L    +
          hour * 60L * 60L   +   min * 60   +    sec;
	return (longtime);
}
#endif /* tos */

#ifdef os9
/*
 *	Some stuffing around to get the modified time of a file
 *	in an os9 file system
 */
void getmdate(fd, tbp)
int fd;
struct sgtbuf *tbp;
{
  struct registers     regs;
  static struct fildes fdbuf;


  regs.rg_a = fd;
  regs.rg_b = SS_FD;
  regs.rg_x = &fdbuf;
  regs.rg_y = sizeof (fdbuf);

  if (_os9(I_GETSTT, &regs) == -1) {
	errno = regs.rg_b & 0xff;
	return -1;
  }
  if (tbp)
  {
	_strass(tbp, fdbuf.fd_date, sizeof (fdbuf.fd_date));
	tbp->t_second = 0;	/* Files are only acurate to mins */
  }
  return 0;
}


/*
 *	Kludge routine to return an aproximation of how many
 *	seconds since 1980.  Dates will be in order, but will not
 *	be lineer
 */
time_t cnvtime(tbp)
struct sgtbuf *tbp;
{
  long acc;

  acc = tbp->t_year - 80;		/* Baseyear is 1980 */
  acc = acc * 12 + tbp->t_month;
  acc = acc * 31 + tbp->t_day;
  acc = acc * 24 + tbp->t_hour;
  acc = acc * 60 + tbp->t_minute;
  acc = acc * 60 + tbp->t_second;

  return acc;
}


/*
 *	Get the current time in the internal format
 */
void time(tp)
time_t *tp;
{
  struct sgtbuf tbuf;


  if (getime(&tbuf) < 0)
	return -1;

  if (tp)
	*tp = cnvtime(&tbuf);

  return 0;
}
#endif


/*
 *	Get the modification time of a file.  If the first
 *	doesn't exist, it's modtime is set to 0.
 */
void modtime(np)
struct name *np;
{
#ifdef unix
  struct stat info;
  int r;

  if (is_archive_ref(np->n_name)) {
	r = archive_stat(np->n_name, &info);
  } else {
	r = stat(np->n_name, &info);
  }
  if (r < 0) {
	if (errno != ENOENT)
		fatal("Can't open %s: %s", np->n_name, errno);

	np->n_time = 0L;
	np->n_flag &= ~N_EXISTS;
  } else {
	np->n_time = info.st_mtime;
	np->n_flag |= N_EXISTS;
  }
#endif
#ifdef tos
  struct DOSTIME fm;
  int fd;

  if((fd=Fopen(np->n_name,0)) < 0) {
        np->n_time = 0L;
	np->n_flag &= ~N_EXISTS;
  }
  else {
        Fdatime(&fm,fd,0);
        Fclose(fd);
        np->n_time = mstonix((unsigned int)fm.date,(unsigned int)fm.time);
        np->n_flag |= N_EXISTS;
  }
#endif
#ifdef eon
  struct stat  info;
  int          fd;

  if ((fd = open(np->n_name, 0)) < 0) {
	if (errno != ER_NOTF)
		fatal("Can't open %s: %s", np->n_name, errno);

	np->n_time = 0L;
	np->n_flag &= ~N_EXISTS;
  }
  else if (getstat(fd, &info) < 0)
	fatal("Can't getstat %s: %s", np->n_name, errno);
  else {
	np->n_time = info.st_mod;
	np->n_flag |= N_EXISTS;
  }

  close(fd);
#endif
#ifdef os9
  struct sgtbuf  info;
  int            fd;

  if ((fd = open(np->n_name, 0)) < 0) {
  if (errno != E_PNNF)
		fatal("Can't open %s: %s", np->n_name, errno);

	np->n_time = 0L;
	np->n_flag &= ~N_EXISTS;
  }
  else if (getmdate(fd, &info) < 0)
	fatal("Can't getstat %s: %s", np->n_name, errno);
  else {
	np->n_time = cnvtime(&info);
	np->n_flag |= N_EXISTS;
  }

  close(fd);
#endif
}


/*
 *	Update the mod time of a file to now.
 */
void touch(np)
struct name *np;
{
  char  c;
  int   fd;

  if (!domake || !silent) printf("touch(%s)\n", np->n_name);

  if (domake) {
#ifdef unix
	struct utimbuf   a;

	a.actime = a.modtime = time((time_t *)NULL);
	if (utime(np->n_name, &a) < 0)
		printf("%s: '%s' not touched - non-existant\n",
				myname, np->n_name);
#endif
#ifdef tos
        struct DOSTIME fm;
        int fd;

        if((fd=Fopen(np->n_name,0)) < 0) {
                printf("%s: '%s' not touched - non-existant\n",
                                myname, np->n_name);
        }
        else {
                fm.date = Tgetdate();
                fm.time = Tgettime();
                Fdatime(&fm,fd,1);
                Fclose(fd);
        }
#endif
#ifdef eon
	if ((fd = open(np->n_name, 0)) < 0)
		printf("%s: '%s' not touched - non-existant\n",
				myname, np->n_name);
	else
	{
		uread(fd, &c, 1, 0);
		uwrite(fd, &c, 1);
	}
	close(fd);
#endif
#ifdef os9
	/*
	 *	Strange that something almost as totally useless
	 *	as this is easy to do in os9!
	 */
	if ((fd = open(np->n_name, S_IWRITE)) < 0)
		printf("%s: '%s' not touched - non-existant\n",
				myname, np->n_name);
	close(fd);
#endif
  }
}


/*
 *	Recursive routine to make a target.
 */
int make(np, level)
struct name *np;
int          level;
{
  register struct depend  *dp;
  register struct line    *lp;
  register struct depend  *qdp;
  time_t  now, t, dtime = 0;
  bool    dbgfirst     = TRUE;
  char   *basename  = (char *) 0;
  char   *inputname = (char *) 0;

  if (np->n_flag & N_DONE) {
     if(dbginfo) dbgprint(level,np,"already done");
     return 0;
  }

  modtime(np);		/*  Gets modtime of this file  */

  while (time(&now) == np->n_time) {
     /* Time of target is equal to the current time.  This bothers us, because
      * we can't tell if it needs to be updated if we update a file it depends
      * on within a second.  So wait a second.  (A per-second timer is too
      * coarse for today's fast machines.)
      */
     sleep(1);
  }

  if (rules) {
     for (lp = np->n_line; lp; lp = lp->l_next)
        if (lp->l_cmd)
           break;
     if (!lp)
        dyndep(np,&basename,&inputname);
  }

  if (!(np->n_flag & (N_TARG | N_EXISTS))) {
     fprintf(stderr,"%s: Don't know how to make %s\n", myname, np->n_name);
     if (conterr) {
        np->n_flag |= N_ERROR;
        if (dbginfo) dbgprint(level,np,"don't know how to make");
        return 0;
     }
     else  exit(1);
  }

  for (qdp = (struct depend *)0, lp = np->n_line; lp; lp = lp->l_next) {
     for (dp = lp->l_dep; dp; dp = dp->d_next) {
        if(dbginfo && dbgfirst) {
           dbgprint(level,np," {");
           dbgfirst = FALSE;
        }
        make(dp->d_name, level+1);
        if (np->n_time < dp->d_name->n_time)
           qdp = newdep(dp->d_name, qdp);
        dtime = max(dtime, dp->d_name->n_time);
        if (dp->d_name->n_flag & N_ERROR) np->n_flag |= N_ERROR;
        if (dp->d_name->n_flag & N_EXEC ) np->n_flag |= N_EXEC;
     }
     if (!quest && (np->n_flag & N_DOUBLE) &&
           (np->n_time < dtime || !( np->n_flag & N_EXISTS))) {
        execflag = FALSE;
        make1(np, lp, qdp, basename, inputname); /* free()'s qdp */
        dtime = 0;
        qdp = (struct depend *)0;
        if(execflag) np->n_flag |= N_EXEC;
     }
  }

  np->n_flag |= N_DONE;

  if (quest) {
     t = np->n_time;
     np->n_time = now;
     return (t < dtime);
  }
  else if ((np->n_time < dtime || !( np->n_flag & N_EXISTS))
               && !(np->n_flag & N_DOUBLE)) {
     execflag = FALSE;
     make1(np, (struct line *)0, qdp, basename, inputname); /* free()'s qdp */
     np->n_time = now;
     if ( execflag) np->n_flag |= N_EXEC;
  }
  else if ( np->n_flag & N_EXEC ) {
     np->n_time = now;
  }

  if (dbginfo) {
     if(dbgfirst) {
        if(np->n_flag & N_ERROR)
              dbgprint(level,np,"skipped because of error");
        else if(np->n_flag & N_EXEC)
              dbgprint(level,np,"successfully made");
        else  dbgprint(level,np,"is up to date");
     }
     else {
        if(np->n_flag & N_ERROR)
              dbgprint(level,(struct name *)0,"} skipped because of error");
        else if(np->n_flag & N_EXEC)
              dbgprint(level,(struct name *)0,"} successfully made");
        else  dbgprint(level,(struct name *)0,"} is up to date");
     }
  }
  if (level == 0 && !(np->n_flag & N_EXEC))
     printf("%s: '%s' is up to date\n", myname, np->n_name);

  if(basename)
     free(basename);
  return 0;
}


void make1(np, lp, qdp, basename, inputname)
struct name *np;
struct line *lp;
register struct depend *qdp;
char        *basename;
char        *inputname;
{
  register struct depend *dp;


  if (dotouch)
    touch(np);
  else if (!(np->n_flag & N_ERROR)) {
    strcpy(str1, "");

    if(!inputname) {
       inputname = str1;  /* default */
       if (ambigmac) implmacros(np,lp,&basename,&inputname);
    }
    setDFmacro("<",inputname);

    if(!basename)
       basename = str1;
    setDFmacro("*",basename);

    for (dp = qdp; dp; dp = qdp) {
       if (strlen(str1))
          strcat(str1, " ");
       strcat(str1, dp->d_name->n_name);
       qdp = dp->d_next;
       free(dp);
    }
    setmacro("?", str1);
    setDFmacro("@", np->n_name);

    if (lp)		/* lp set if doing a :: rule */
       docmds1(np, lp);
    else
       docmds(np);
  }
}

void implmacros(np,lp, pbasename,pinputname)
struct name *np;
struct line *lp;
char        **pbasename;		/*  Name without suffix  */
char        **pinputname;
{
  struct line   *llp;
  register char *p;
  register char *q;
  register char *suff;				/*  Old suffix  */
  int            baselen;
  struct depend *dp;
  bool           dpflag = FALSE;

  /* get basename out of target name */
  p = str2;
  q = np->n_name;
  suff = suffix(q);
  while ( *q && (q < suff || !suff)) *p++ = *q++;
  *p = '\0';
  if ((*pbasename = (char *) malloc(strlen(str2)+1)) == (char *)0 )
     fatal("No memory for basename",(char *)0,0);
  strcpy(*pbasename,str2);
  baselen = strlen(str2);

  if ( lp)
     llp = lp;
  else
     llp = np->n_line;

  while (llp) {
     for (dp = llp->l_dep; dp; dp = dp->d_next) {
        if( strncmp(*pbasename,dp->d_name->n_name,baselen) == 0) {
           *pinputname = dp->d_name->n_name;
           return;
        }
        if( !dpflag) {
           *pinputname = dp->d_name->n_name;
           dpflag = TRUE;
        }
     }
     if (lp) break;
     llp = llp->l_next;
  }

#if NO_WE_DO_WANT_THIS_BASENAME
  free(*pbasename);  /* basename ambiguous or no dependency file */
  *pbasename = (char *)0;
#endif
  return;
}

void dbgprint(level,np,comment)
int          level;
struct name *np;
char        *comment;
{
  char *timep;

  if(np) {
     timep = ctime(&np->n_time);
     timep[24] = '\0';
     fputs(&timep[4],stdout);
  }
  else fputs("                    ",stdout);
  fputs("   ",stdout);
  while(level--) fputs("  ",stdout);
  if (np) {
     fputs(np->n_name,stdout);
     if (np->n_flag & N_DOUBLE) fputs("  :: ",stdout);
     else                       fputs("  : ",stdout);
  }
  fputs(comment,stdout);
  putchar((int)'\n');
  fflush(stdout);
  return;
}

