/*************************************************************************
 *
 *  m a k e :   r u l e s . c
 *
 *  Control of the implicit suffix rules
 *========================================================================
 * Edition history
 *
 *  #    Date                         Comments                       By
 * --- -------- ---------------------------------------------------- ---
 *   1    ??                                                         ??
 *   2 01.07.89 $<,$* bugs fixed, impl. r. ending in expl. r. added  RAL
 *   3 23.08.89 suffix as macro, testname intr., algorithem to find
 *              source dep. made more intelligent (see Readme3)      RAL
 *   4 30.08.89 indention changed                                    PSH,RAL
 *   5 03.09.89 fixed LZ eliminated                                  RAL
 *   6 07.09.89 rules of type '.c', .DEFAULT added, dep. search impr.RAL
 * ------------ Version 2.0 released ------------------------------- RAL
 *
 *************************************************************************/

#include "h.h"


/*
 *	Dynamic dependency.  This routine applies the suffis rules
 *	to try and find a source and a set of rules for a missing
 *	target.  If found, np is made into a target with the implicit
 *	source name, and rules.  Returns TRUE if np was made into
 *	a target.
 */
bool dyndep(np,pbasename,pinputname)
struct name  *np;
char        **pbasename;		/*  Name without suffix  */
char        **pinputname;
{
  register char *p;
  register char *q;
  register char *suff;				/*  Old suffix  */
  struct name   *op = (struct name *)0,*optmp;	/*  New dependent  */
  struct name   *sp;				/*  Suffix  */
  struct line   *lp,*nlp;
  struct depend *dp,*ndp;
  struct cmd    *cmdp;
  char          *newsuff;
  bool           depexists = FALSE;


  p = str1;
  q = np->n_name;
  suff = suffix(q);
  while (*q && (q < suff || !suff)) *p++ = *q++;
  *p = '\0';
  if ((*pbasename = (char *) malloc(strlen(str1)+1)) == (char *)0 )
     fatal("No memory for basename",(char *)0,0);
  strcpy(*pbasename,str1);
  if ( !suff) suff = p - str1 + *pbasename;  /* set suffix to nullstring */

  if (!((sp = newname(".SUFFIXES"))->n_flag & N_TARG))  return FALSE;

  /* search all .SUFFIXES lines */
  for (lp = sp->n_line; lp; lp = lp->l_next)
     /* try all suffixes */
     for (dp = lp->l_dep; dp; dp = dp->d_next) {
        /* compose implicit rule name (.c.o)...*/
        newsuff = dp->d_name->n_name;
        while (strlen(suff)+strlen(newsuff)+1 >= str1s.len) strrealloc(&str1s);
        p = str1;
        q = newsuff;
        while (*p++ = *q++) ;
        p--;
        q = suff;
        while (*p++ = *q++) ;
        /* look if the rule exists */
        sp = newname(str1);
        if (sp->n_flag & N_TARG) {
           /* compose resulting dependency name */
           while (strlen(*pbasename) + strlen(newsuff)+1 >= str1s.len)
              strrealloc(&str1s);
           q = *pbasename;
           p = str1;
           while (*p++ = *q++) ;
           p--;
           q = newsuff;
           while (*p++ = *q++) ;
           /* test if dependency file or an explicit rule exists */
           if ((optmp= testname(str1)) != (struct name *)0) {
              /* store first possible dependency as default */
              if ( op == (struct name *)0) {
                 op = optmp;
                 cmdp = sp->n_line->l_cmd;
              }
              /* check if testname is an explicit dependency */
              for ( nlp=np->n_line; nlp; nlp=nlp->l_next) {
                 for( ndp=nlp->l_dep; ndp; ndp=ndp->d_next) {
                    if ( strcmp( ndp->d_name->n_name, str1) == 0) {
                       op = optmp;
                       cmdp = sp->n_line->l_cmd;
                       ndp = (struct depend *) 0;
                       goto found2;
                    }
                    depexists = TRUE;
                 }
              }
              /* if no explicit dependencies : accept testname */
              if (!depexists)  goto found;
           }
        }
     }

  if ( op == (struct name *)0) {
     if( np->n_flag & N_TARG) {     /* DEFAULT handling */
        if (!((sp = newname(".DEFAULT"))->n_flag & N_TARG))  return FALSE;
        if (!(sp->n_line)) return FALSE;
        cmdp = sp->n_line->l_cmd;
        for ( nlp=np->n_line; nlp; nlp=nlp->l_next) {
           if ( ndp=nlp->l_dep) {
              op = ndp->d_name;
              ndp = (struct depend *)0;
              goto found2;
           }
        }
        newline(np, (struct depend *)0, cmdp, 0);
        *pinputname = (char *)0;
        *pbasename  = (char *)0;
        return TRUE;
     }
  else return FALSE;
  }

found:
  ndp = newdep(op, (struct depend *)0);
found2:
  newline(np, ndp, cmdp, 0);
  *pinputname = op->n_name;
  return TRUE;
}


/*
 *	Make the default rules
 */
void makerules()
{
  struct cmd    *cp;
  struct name   *np;
  struct depend *dp;


#ifdef eon
  setmacro("BDSCC", "asm");
  /*	setmacro("BDSCFLAGS", "");	*/
  cp = newcmd("$(BDSCC) $(BDSCFLAGS) -n $<", (struct cmd *)0);
  np = newname(".c.o");
  newline(np, (struct depend *)0, cp, 0);

  setmacro("CC", "c");
  setmacro("CFLAGS", "-O");
  cp = newcmd("$(CC) $(CFLAGS) -c $<", (struct cmd *)0);
  np = newname(".c.obj");
  newline(np, (struct depend *)0, cp, 0);

  setmacro("M80", "asm -n");
  /*	setmacro("M80FLAGS", "");	*/
  cp = newcmd("$(M80) $(M80FLAGS) $<", (struct cmd *)0);
  np = newname(".mac.o");
  newline(np, (struct depend *)0, cp, 0);

  setmacro("AS", "zas");
  /*	setmacro("ASFLAGS", "");	*/
  cp = newcmd("$(ZAS) $(ASFLAGS) -o $@ $<", (struct cmd *)0);
  np = newname(".as.obj");
  newline(np, (struct depend *)0, cp, 0);

  np = newname(".as");
  dp = newdep(np, (struct depend *)0);
  np = newname(".obj");
  dp = newdep(np, dp);
  np = newname(".c");
  dp = newdep(np, dp);
  np = newname(".o");
  dp = newdep(np, dp);
  np = newname(".mac");
  dp = newdep(np, dp);
  np = newname(".SUFFIXES");
  newline(np, dp, (struct cmd *)0, 0);
#endif

#ifdef tos
#define unix
#endif

/*
 *	Some of the UNIX implicit rules
 */

#ifdef unix

  setmacro("CC", "cc");
  setmacro("CFLAGS", "");

  cp = newcmd("$(CC) -S $(CFLAGS) $<", (struct cmd *)0);
  np = newname(".c.s");
  newline(np, (struct depend *)0, cp, 0);

  cp = newcmd("$(CC) -c $(CFLAGS) $<", (struct cmd *)0);
  np = newname(".c.o");
  newline(np, (struct depend *)0, cp, 0);

#if this_rule_is_a_bit_too_much_of_a_good_thing
#ifdef MINIXPC
  cp = newcmd("$(CC) $(CFLAGS) -i -o $@ $<", (struct cmd *)0);
#else
  cp = newcmd("$(CC) $(CFLAGS) -o $@ $<", (struct cmd *)0);
#endif /* MINIXPC */
  np = newname(".c");
  newline(np, (struct depend *)0, cp, 0);
#endif

  cp = newcmd("$(CC) -c $(CFLAGS) $<", (struct cmd *)0);
  np = newname(".s.o");
  newline(np, (struct depend *)0, cp, 0);

  setmacro("YACC", "yacc");
  /*setmacro("YFLAGS", "");	*/
  cp = newcmd("$(YACC) $(YFLAGS) $<", (struct cmd *)0);
  cp = newcmd("mv y.tab.c $@", cp);
  np = newname(".y.c");
  newline(np, (struct depend *)0, cp, 0);

  cp = newcmd("$(YACC) $(YFLAGS) $<", (struct cmd *)0);
  cp = newcmd("$(CC) $(CFLAGS) -c y.tab.c", cp);
  cp = newcmd("mv y.tab.o $@", cp);
  np = newname(".y.o");
  cp = newcmd("rm y.tab.c", cp);
  newline(np, (struct depend *)0, cp, 0);

  setmacro("FLEX", "flex");
  cp = newcmd("$(FLEX) $(FLEX_FLAGS) $<", (struct cmd *)0);
  cp = newcmd("mv lex.yy.c $@", cp);
  np = newname(".l.c");
  newline(np, (struct depend *)0, cp, 0);

  cp = newcmd("$(FLEX) $(FLEX_FLAGS) $<", (struct cmd *)0);
  cp = newcmd("$(CC) $(CFLAGS) -c lex.yy.c", cp);
  cp = newcmd("mv lex.yy.o $@", cp);
  np = newname(".l.o");
  cp = newcmd("rm lex.yy.c", cp);
  newline(np, (struct depend *)0, cp, 0);

  np = newname(".o");
  dp = newdep(np, (struct depend *)0);
  np = newname(".s");
  dp = newdep(np, dp);
  np = newname(".c");
  dp = newdep(np, dp);
  np = newname(".y");
  dp = newdep(np, dp);
  np = newname(".l");
  dp = newdep(np, dp);
  np = newname(".SUFFIXES");
  newline(np, dp, (struct cmd *)0, 0);

#endif /* unix */


#ifdef os9
/*
 *	Fairlight use an enhanced version of the C sub-system.
 *	They have a specialised macro pre-processor.
 */
  setmacro("CC", "cc");
  setmacro("CFLAGS", "-z");
  cp = newcmd("$(CC) $(CFLAGS) -r $<", (struct cmd *)0);

  np = newname(".c.r");
  newline(np, (struct depend *)0, cp, 0);
  np = newname(".ca.r");
  newline(np, (struct depend *)0, cp, 0);
  np = newname(".a.r");
  newline(np, (struct depend *)0, cp, 0);
  np = newname(".o.r");
  newline(np, (struct depend *)0, cp, 0);
  np = newname(".mc.r");
  newline(np, (struct depend *)0, cp, 0);
  np = newname(".mca.r");
  newline(np, (struct depend *)0, cp, 0);
  np = newname(".ma.r");
  newline(np, (struct depend *)0, cp, 0);
  np = newname(".mo.r");
  newline(np, (struct depend *)0, cp, 0);

  np = newname(".r");
  dp = newdep(np, (struct depend *)0);
  np = newname(".mc");
  dp = newdep(np, dp);
  np = newname(".mca");
  dp = newdep(np, dp);
  np = newname(".c");
  dp = newdep(np, dp);
  np = newname(".ca");
  dp = newdep(np, dp);
  np = newname(".ma");
  dp = newdep(np, dp);
  np = newname(".mo");
  dp = newdep(np, dp);
  np = newname(".o");
  dp = newdep(np, dp);
  np = newname(".a");
  dp = newdep(np, dp);
  np = newname(".SUFFIXES");
  newline(np, dp, (struct cmd *)0, 0);
#endif
}
