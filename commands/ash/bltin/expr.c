/*
 * The expr and test commands.
 *
 * Copyright (C) 1989 by Kenneth Almquist.  All rights reserved.
 * This file is part of ash, which is distributed under the terms specified
 * by the Ash General Public License.  See the file named LICENSE.
 */


#define main exprcmd

#include "bltin.h"
#include "operators.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISLNK
#define lstat		stat
#define S_ISLNK(mode)	(0)
#endif

#define STACKSIZE 12
#define NESTINCR 16

/* data types */
#define STRING 0
#define INTEGER 1
#define BOOLEAN 2


/*
 * This structure hold a value.  The type keyword specifies the type of
 * the value, and the union u holds the value.  The value of a boolean
 * is stored in u.num (1 = TRUE, 0 = FALSE).
 */

struct value {
      int type;
      union {
	    char *string;
	    long num;
      } u;
};


struct operator {
      short op;			/* which operator */
      short pri;		/* priority of operator */
};


struct filestat {
      int op;			/* OP_FILE or OP_LFILE */
      char *name;		/* name of file */
      int rcode;		/* return code from stat */
      struct stat stat;		/* status info on file */
};


extern char *match_begin[10];	/* matched string */
extern short match_length[10];	/* defined in regexp.c */
extern short number_parens;	/* number of \( \) pairs */


#ifdef __STDC__
int expr_is_false(struct value *);
void expr_operator(int, struct value *, struct filestat *);
int lookup_op(char *, char *const*);
char *re_compile(char *);	/* defined in regexp.c */
int re_match(char *, char *);	/* defined in regexp.c */
long atol(const char *);
#else
int expr_is_false();
void expr_operator();
int lookup_op();
char *re_compile();	/* defined in regexp.c */
int re_match();	/* defined in regexp.c */
long atol();
#endif



main(argc, argv)  char **argv; {
      char **ap;
      char *opname;
      char c;
      char *p;
      int print;
      int nest;		/* parenthises nesting */
      int op;
      int pri;
      int skipping;
      int binary;
      struct operator opstack[STACKSIZE];
      struct operator *opsp;
      struct value valstack[STACKSIZE + 1];
      struct value *valsp;
      struct filestat fs;

      INITARGS(argv);
      c = **argv;
      print = 1;
      if (c == 't')
	    print = 0;
      else if (c == '[') {
	    if (! equal(argv[argc - 1], "]"))
		  error("missing ]");
	    argv[argc - 1] = NULL;
	    print = 0;
      }
      ap = argv + 1;
      fs.name = NULL;

      /*
       * We use operator precedence parsing, evaluating the expression
       * as we parse it.  Parentheses are handled by bumping up the
       * priority of operators using the variable "nest."  We use the
       * variable "skipping" to turn off evaluation temporarily for the
       * short circuit boolean operators.  (It is important do the short
       * circuit evaluation because under NFS a stat operation can take
       * infinitely long.)
       */

      nest = 0;
      skipping = 0;
      opsp = opstack + STACKSIZE;
      valsp = valstack;
      if (*ap == NULL) {
	    valstack[0].type = BOOLEAN;
	    valstack[0].u.num = 0;
	    goto done;
      }
      for (;;) {
	    opname = *ap++;
	    if (opname == NULL)
syntax:		  error("syntax error");
	    if (opname[0] == '(' && opname[1] == '\0') {
		  nest += NESTINCR;
		  continue;
	    } else if (*ap && (op = lookup_op(opname, unary_op)) >= 0) {
		  if (opsp == &opstack[0])
overflow:		error("Expression too complex");
		  --opsp;
		  opsp->op = op;
		  opsp->pri = op_priority[op] + nest;
		  continue;

	    } else {
		  valsp->type = STRING;
		  valsp->u.string = opname;
		  valsp++;
	    }
	    for (;;) {
		  opname = *ap++;
		  if (opname == NULL) {
			if (nest != 0)
			      goto syntax;
			pri = 0;
			break;
		  }
		  if (opname[0] != ')' || opname[1] != '\0') {
			if ((op = lookup_op(opname, binary_op)) < 0)
			      goto syntax;
			op += FIRST_BINARY_OP;
			pri = op_priority[op] + nest;
			break;
		  }
		  if ((nest -= NESTINCR) < 0)
			goto syntax;
	    }
	    while (opsp < &opstack[STACKSIZE] && opsp->pri >= pri) {
		  binary = opsp->op;
		  for (;;) {
			valsp--;
			c = op_argflag[opsp->op];
			if (c == OP_INT) {
			      if (valsp->type == STRING)
				    valsp->u.num = atol(valsp->u.string);
			      valsp->type = INTEGER;
			} else if (c >= OP_STRING) { /* OP_STRING or OP_FILE */
			      if (valsp->type == INTEGER) {
				    p = stalloc(32);
#ifdef SHELL
				    fmtstr(p, 32, "%d", valsp->u.num);
#else
				    sprintf(p, "%d", valsp->u.num);
#endif
				    valsp->u.string = p;
			      } else if (valsp->type == BOOLEAN) {
				    if (valsp->u.num)
					  valsp->u.string = "true";
				    else
					  valsp->u.string = "";
			      }
			      valsp->type = STRING;
			      if (c >= OP_FILE
			       && (fs.op != c
			           || fs.name == NULL
			           || ! equal(fs.name, valsp->u.string))) {
				    fs.op = c;
				    fs.name = valsp->u.string;
				    if (c == OP_FILE) {
					fs.rcode = stat(valsp->u.string,
								&fs.stat);
				    } else {
					fs.rcode = lstat(valsp->u.string,
								&fs.stat);
				    }
			      }
			}
			if (binary < FIRST_BINARY_OP)
			      break;
			binary = 0;
		  }
		  if (! skipping)
			expr_operator(opsp->op, valsp, &fs);
		  else if (opsp->op == AND1 || opsp->op == OR1)
			skipping--;
		  valsp++;		/* push value */
		  opsp++;		/* pop operator */
	    }
	    if (opname == NULL)
		  break;
	    if (opsp == &opstack[0])
		  goto overflow;
	    if (op == AND1 || op == AND2) {
		  op = AND1;
		  if (skipping || expr_is_false(valsp - 1))
			skipping++;
	    }
	    if (op == OR1 || op == OR2) {
		  op = OR1;
		  if (skipping || ! expr_is_false(valsp - 1))
			skipping++;
	    }
	    opsp--;
	    opsp->op = op;
	    opsp->pri = pri;
      }
done:
      if (print) {
	    if (valstack[0].type == STRING)
		  printf("%s\n", valstack[0].u.string);
	    else if (valstack[0].type == INTEGER)
		  printf("%ld\n", valstack[0].u.num);
	    else if (valstack[0].u.num != 0)
		  printf("true\n");
      }
      return expr_is_false(&valstack[0]);
}


int
expr_is_false(val)
      struct value *val;
      {
      if (val->type == STRING) {
	    if (val->u.string[0] == '\0')
		  return 1;
      } else {	/* INTEGER or BOOLEAN */
	    if (val->u.num == 0)
		  return 1;
      }
      return 0;
}


/*
 * Execute an operator.  Op is the operator.  Sp is the stack pointer;
 * sp[0] refers to the first operand, sp[1] refers to the second operand
 * (if any), and the result is placed in sp[0].  The operands are converted
 * to the type expected by the operator before expr_operator is called.
 * Fs is a pointer to a structure which holds the value of the last call
 * to stat, to avoid repeated stat calls on the same file.
 */

void
expr_operator(op, sp, fs)
      int op;
      struct value *sp;
      struct filestat *fs;
      {
      int i;
      struct stat st1, st2;

      switch (op) {
      case NOT:
	    sp->u.num = expr_is_false(sp);
	    sp->type = BOOLEAN;
	    break;
      case ISREAD:
	    i = 04;
	    goto permission;
      case ISWRITE:
	    i = 02;
	    goto permission;
      case ISEXEC:
	    i = 01;
permission:
	    if (fs->stat.st_uid == geteuid())
		  i <<= 6;
	    else if (fs->stat.st_gid == getegid())
		  i <<= 3;
	    goto filebit;	/* true if (stat.st_mode & i) != 0 */
      case ISFILE:
	    i = S_IFREG;
	    goto filetype;
      case ISDIR:
	    i = S_IFDIR;
	    goto filetype;
      case ISCHAR:
	    i = S_IFCHR;
	    goto filetype;
      case ISBLOCK:
	    i = S_IFBLK;
	    goto filetype;
      case ISFIFO:
#ifdef S_IFIFO
	    i = S_IFIFO;
	    goto filetype;
#else
	    goto false;
#endif
filetype:
	    if ((fs->stat.st_mode & S_IFMT) == i && fs->rcode >= 0) {
true:
		  sp->u.num = 1;
	    } else {
false:
		  sp->u.num = 0;
	    }
	    sp->type = BOOLEAN;
	    break;
      case ISSETUID:
	    i = S_ISUID;
	    goto filebit;
      case ISSETGID:
	    i = S_ISGID;
	    goto filebit;
      case ISSTICKY:
	    i = S_ISVTX;
filebit:
	    if (fs->stat.st_mode & i && fs->rcode >= 0)
		  goto true;
	    goto false;
      case ISSIZE:
	    sp->u.num = fs->rcode >= 0? fs->stat.st_size : 0L;
	    sp->type = INTEGER;
	    break;
      case ISLINK1:
      case ISLINK2:
	    if (S_ISLNK(fs->stat.st_mode) && fs->rcode >= 0)
		  goto true;
	    fs->op = OP_FILE;	/* not a symlink, so expect a -d or so next */
	    goto false;
      case NEWER:
	    if (stat(sp->u.string, &st1) != 0) {
		  sp->u.num = 0;
	    } else if (stat((sp + 1)->u.string, &st2) != 0) {
		  sp->u.num = 1;
	    } else {
		  sp->u.num = st1.st_mtime >= st2.st_mtime;
	    }
	    sp->type = INTEGER;
	    break;
      case ISTTY:
	    sp->u.num = isatty(sp->u.num);
	    sp->type = BOOLEAN;
	    break;
      case NULSTR:
	    if (sp->u.string[0] == '\0')
		  goto true;
	    goto false;
      case STRLEN:
	    sp->u.num = strlen(sp->u.string);
	    sp->type = INTEGER;
	    break;
      case OR1:
      case AND1:
	    /*
	     * These operators are mostly handled by the parser.  If we
	     * get here it means that both operands were evaluated, so
	     * the value is the value of the second operand.
	     */
	    *sp = *(sp + 1);
	    break;
      case STREQ:
      case STRNE:
	    i = 0;
	    if (equal(sp->u.string, (sp + 1)->u.string))
		  i++;
	    if (op == STRNE)
		  i = 1 - i;
	    sp->u.num = i;
	    sp->type = BOOLEAN;
	    break;
      case EQ:
	    if (sp->u.num == (sp + 1)->u.num)
		  goto true;
	    goto false;
      case NE:
	    if (sp->u.num != (sp + 1)->u.num)
		  goto true;
	    goto false;
      case GT:
	    if (sp->u.num > (sp + 1)->u.num)
		  goto true;
	    goto false;
      case LT:
	    if (sp->u.num < (sp + 1)->u.num)
		  goto true;
	    goto false;
      case LE:
	    if (sp->u.num <= (sp + 1)->u.num)
		  goto true;
	    goto false;
      case GE:
	    if (sp->u.num >= (sp + 1)->u.num)
		  goto true;
	    goto false;
      case PLUS:
	    sp->u.num += (sp + 1)->u.num;
	    break;
      case MINUS:
	    sp->u.num -= (sp + 1)->u.num;
	    break;
      case TIMES:
	    sp->u.num *= (sp + 1)->u.num;
	    break;
      case DIVIDE:
	    if ((sp + 1)->u.num == 0)
		  error("Division by zero");
	    sp->u.num /= (sp + 1)->u.num;
	    break;
      case REM:
	    if ((sp + 1)->u.num == 0)
		  error("Division by zero");
	    sp->u.num %= (sp + 1)->u.num;
	    break;
      case MATCHPAT:
	    {
		  char *pat;

		  pat = re_compile((sp + 1)->u.string);
		  if (re_match(pat, sp->u.string)) {
			if (number_parens > 0) {
			      sp->u.string = match_begin[1];
			      sp->u.string[match_length[1]] = '\0';
			} else {
			      sp->u.num = match_length[0];
			      sp->type = INTEGER;
			}
		  } else {
			if (number_parens > 0) {
			      sp->u.string[0] = '\0';
			} else {
			      sp->u.num = 0;
			      sp->type = INTEGER;
			}
		  }
	    }
	    break;
      }
}


int
lookup_op(name, table)
      char *name;
      char *const*table;
      {
      register char *const*tp;
      register char const *p;
      char c = name[1];

      for (tp = table ; (p = *tp) != NULL ; tp++) {
	    if (p[1] == c && equal(p, name))
		  return tp - table;
      }
      return -1;
}
