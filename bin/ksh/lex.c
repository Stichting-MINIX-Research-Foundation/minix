/*	$NetBSD: lex.c,v 1.15 2011/10/16 17:12:11 joerg Exp $	*/

/*
 * lexical analysis and source input
 */
#include <sys/cdefs.h>

#ifndef lint
__RCSID("$NetBSD: lex.c,v 1.15 2011/10/16 17:12:11 joerg Exp $");
#endif


#include "sh.h"
#include <ctype.h>


/* Structure to keep track of the lexing state and the various pieces of info
 * needed for each particular state.
 */
typedef struct lex_state Lex_state;
struct lex_state {
	int ls_state;
	union {
	    /* $(...) */
	    struct scsparen_info {
		    int nparen;		/* count open parenthesis */
		    int csstate; /* XXX remove */
#define ls_scsparen ls_info.u_scsparen
	    } u_scsparen;

	    /* $((...)) */
	    struct sasparen_info {
		    int nparen;		/* count open parenthesis */
		    int start;		/* marks start of $(( in output str */
#define ls_sasparen ls_info.u_sasparen
	    } u_sasparen;

	    /* ((...)) */
	    struct sletparen_info {
		    int nparen;		/* count open parenthesis */
#define ls_sletparen ls_info.u_sletparen
	    } u_sletparen;

	    /* `...` */
	    struct sbquote_info {
		    int indquotes;	/* true if in double quotes: "`...`" */
#define ls_sbquote ls_info.u_sbquote
	    } u_sbquote;

	    Lex_state *base;		/* used to point to next state block */
	} ls_info;
};

typedef struct State_info State_info;
struct State_info {
	Lex_state	*base;
	Lex_state	*end;
};


static void	readhere ARGS((struct ioword *iop));
static int	getsc__ ARGS((void));
static void	getsc_line ARGS((Source *s));
static int	getsc_bn ARGS((void));
static char	*get_brace_var ARGS((XString *wsp, char *wp));
static int	arraysub ARGS((char **strp));
static const char *ungetsc ARGS((int c));
static void	gethere ARGS((void));
static Lex_state *push_state_ ARGS((State_info *si, Lex_state *old_end));
static Lex_state *pop_state_ ARGS((State_info *si, Lex_state *old_end));

static int backslash_skip;
static int ignore_backslash_newline;

/* optimized getsc_bn() */
#define getsc()		(*source->str != '\0' && *source->str != '\\' \
			 && !backslash_skip ? *source->str++ : getsc_bn())
/* optimized getsc__() */
#define	getsc_()	((*source->str != '\0') ? *source->str++ : getsc__())

#define STATE_BSIZE	32

#define PUSH_STATE(s)	do { \
			    if (++statep == state_info.end) \
				statep = push_state_(&state_info, statep); \
			    state = statep->ls_state = (s); \
			} while (0)

#define POP_STATE()	do { \
			    if (--statep == state_info.base) \
				statep = pop_state_(&state_info, statep); \
			    state = statep->ls_state; \
			} while (0)



/*
 * Lexical analyzer
 *
 * tokens are not regular expressions, they are LL(1).
 * for example, "${var:-${PWD}}", and "$(size $(whence ksh))".
 * hence the state stack.
 */

int
yylex(cf)
	int cf;
{
	Lex_state states[STATE_BSIZE], *statep;
	State_info state_info;
	register int c, state;
	XString ws;		/* expandable output word */
	register char *wp;	/* output word pointer */
	char *sp, *dp;
	int c2;


  Again:
	states[0].ls_state = -1;
	states[0].ls_info.base = (Lex_state *) 0;
	statep = &states[1];
	state_info.base = states;
	state_info.end = &states[STATE_BSIZE];

	Xinit(ws, wp, 64, ATEMP);

	backslash_skip = 0;
	ignore_backslash_newline = 0;

	if (cf&ONEWORD)
		state = SWORD;
#ifdef KSH
	else if (cf&LETEXPR) {
		*wp++ = OQUOTE;	 /* enclose arguments in (double) quotes */
		state = SLETPAREN;	
		statep->ls_sletparen.nparen = 0;
	}
#endif /* KSH */
	else {		/* normal lexing */
		state = (cf & HEREDELIM) ? SHEREDELIM : SBASE;
		while ((c = getsc()) == ' ' || c == '\t')
			;
		if (c == '#') {
			ignore_backslash_newline++;
			while ((c = getsc()) != '\0' && c != '\n')
				;
			ignore_backslash_newline--;
		}
		ungetsc(c);
	}
	if (source->flags & SF_ALIAS) {	/* trailing ' ' in alias definition */
		source->flags &= ~SF_ALIAS;
		/* In POSIX mode, a trailing space only counts if we are
		 * parsing a simple command
		 */
		if (!Flag(FPOSIX) || (cf & CMDWORD))
			cf |= ALIAS;
	}

	/* Initial state: one of SBASE SHEREDELIM SWORD SASPAREN */
	statep->ls_state = state;

	/* collect non-special or quoted characters to form word */
	while (!((c = getsc()) == 0
		 || ((state == SBASE || state == SHEREDELIM)
		     && ctype(c, C_LEX1))))
	{
		Xcheck(ws, wp);
		switch (state) {
		  case SBASE:
			if (c == '[' && (cf & (VARASN|ARRAYVAR))) {
				*wp = EOS; /* temporary */
				if (is_wdvarname(Xstring(ws, wp), FALSE))
				{
					char *p, *tmp;

					if (arraysub(&tmp)) {
						*wp++ = CHAR;
						*wp++ = c;
						for (p = tmp; *p; ) {
							Xcheck(ws, wp);
							*wp++ = CHAR;
							*wp++ = *p++;
						}
						afree(tmp, ATEMP);
						break;
					} else {
						Source *s;

						s = pushs(SREREAD,
							  source->areap);
						s->start = s->str
							= s->u.freeme = tmp;
						s->next = source;
						source = s;
					}
				}
				*wp++ = CHAR;
				*wp++ = c;
				break;
			}
			/* fall through.. */
		  Sbase1:	/* includes *(...|...) pattern (*+?@!) */
#ifdef KSH
			if (c == '*' || c == '@' || c == '+' || c == '?'
			    || c == '!')
			{
				c2 = getsc();
				if (c2 == '(' /*)*/ ) {
					*wp++ = OPAT;
					*wp++ = c;
					PUSH_STATE(SPATTERN);
					break;
				}
				ungetsc(c2);
			}
#endif /* KSH */
			/* fall through.. */
		  Sbase2:	/* doesn't include *(...|...) pattern (*+?@!) */
			switch (c) {
			  case '\\':
				c = getsc();
#ifdef OS2
				if (isalnum((unsigned char)c)) {
					*wp++ = CHAR, *wp++ = '\\';
					*wp++ = CHAR, *wp++ = c;
				} else
#endif
				if (c) /* trailing \ is lost */
					*wp++ = QCHAR, *wp++ = c;
				break;
			  case '\'':
				*wp++ = OQUOTE;
				ignore_backslash_newline++;
				PUSH_STATE(SSQUOTE);
				break;
			  case '"':
				*wp++ = OQUOTE;
				PUSH_STATE(SDQUOTE);
				break;
			  default:
				goto Subst;
			}
			break;

		  Subst:
			switch (c) {
			  Lex_state *s;
			  Lex_state *base;

			  case '\\':
				c = getsc();
				switch (c) {
				  case '\\':
				  case '$': case '`':
					*wp++ = QCHAR, *wp++ = c;
					break;
				  case '"':
					if ((cf & HEREDOC) == 0) {
						*wp++ = QCHAR, *wp++ = c;
						break;
					}
					/* FALLTROUGH */
				  default:
					Xcheck(ws, wp);
					if (c) { /* trailing \ is lost */
						*wp++ = CHAR, *wp++ = '\\';
						*wp++ = CHAR, *wp++ = c;
					}
					break;
				}
				break;
			  case '$':
				c = getsc();
				if (c == '(') /*)*/ {
					c = getsc();
					if (c == '(') /*)*/ {
						PUSH_STATE(SASPAREN);
						statep->ls_sasparen.nparen = 2;
						statep->ls_sasparen.start =
							Xsavepos(ws, wp);
						*wp++ = EXPRSUB;
					} else {
						ungetsc(c);
						PUSH_STATE(SCSPAREN);
						statep->ls_scsparen.nparen = 1;
						statep->ls_scsparen.csstate = 0;
						*wp++ = COMSUB;
					}
				} else if (c == '{') /*}*/ {
					*wp++ = OSUBST;
					*wp++ = '{'; /*}*/
					wp = get_brace_var(&ws, wp);
					c = getsc();
					/* allow :# and :% (ksh88 compat) */
					if (c == ':') {
						*wp++ = CHAR, *wp++ = c;
						c = getsc();
					}
					/* If this is a trim operation,
					 * treat (,|,) specially in STBRACE.
					 */
					if (c == '#' || c == '%') {
						ungetsc(c);
						PUSH_STATE(STBRACE);
					} else {
						ungetsc(c);
						PUSH_STATE(SBRACE);
					}
				} else if (ctype(c, C_ALPHA)) {
					*wp++ = OSUBST;
					*wp++ = 'X';
					do {
						Xcheck(ws, wp);
						*wp++ = c;
						c = getsc();
					} while (ctype(c, C_ALPHA|C_DIGIT));
					*wp++ = '\0';
					*wp++ = CSUBST;
					*wp++ = 'X';
					ungetsc(c);
				} else if (ctype(c, C_DIGIT|C_VAR1)) {
					Xcheck(ws, wp);
					*wp++ = OSUBST;
					*wp++ = 'X';
					*wp++ = c;
					*wp++ = '\0';
					*wp++ = CSUBST;
					*wp++ = 'X';
				} else {
					*wp++ = CHAR, *wp++ = '$';
					ungetsc(c);
				}
				break;
			  case '`':
				PUSH_STATE(SBQUOTE);
				*wp++ = COMSUB;
				/* Need to know if we are inside double quotes
				 * since sh/at&t-ksh translate the \" to " in
				 * "`..\"..`".  POSIX also requires this.
				 * An earlier version of ksh misinterpreted
				 * the POSIX specification and performed
				 * removal of backslash escapes only if
				 * posix mode was not in effect.
				 */
				statep->ls_sbquote.indquotes = 0;
				s = statep;
				base = state_info.base;
				while (1) {
					for (; s != base; s--) {
						if (s->ls_state == SDQUOTE) {
							statep->ls_sbquote.indquotes = 1;
							break;
						}
					}
					if (s != base)
						break;
					if (!(s = s->ls_info.base))
						break;
					base = s-- - STATE_BSIZE;
				}
				break;
			  default:
				*wp++ = CHAR, *wp++ = c;
			}
			break;

		  case SSQUOTE:
			if (c == '\'') {
				POP_STATE();
				*wp++ = CQUOTE;
				ignore_backslash_newline--;
			} else
				*wp++ = QCHAR, *wp++ = c;
			break;

		  case SDQUOTE:
			if (c == '"') {
				POP_STATE();
				*wp++ = CQUOTE;
			} else
				goto Subst;
			break;

		  case SCSPAREN: /* $( .. ) */
			/* todo: deal with $(...) quoting properly
			 * kludge to partly fake quoting inside $(..): doesn't
			 * really work because nested $(..) or ${..} inside
			 * double quotes aren't dealt with.
			 */
			switch (statep->ls_scsparen.csstate) {
			  case 0: /* normal */
				switch (c) {
				  case '(':
					statep->ls_scsparen.nparen++;
					break;
				  case ')':
					statep->ls_scsparen.nparen--;
					break;
				  case '\\':
					statep->ls_scsparen.csstate = 1;
					break;
				  case '"':
					statep->ls_scsparen.csstate = 2;
					break;
				  case '\'':
					statep->ls_scsparen.csstate = 4;
					ignore_backslash_newline++;
					break;
				}
				break;

			  case 1: /* backslash in normal mode */
			  case 3: /* backslash in double quotes */
				--statep->ls_scsparen.csstate;
				break;

			  case 2: /* double quotes */
				if (c == '"')
					statep->ls_scsparen.csstate = 0;
				else if (c == '\\')
					statep->ls_scsparen.csstate = 3;
				break;

			  case 4: /* single quotes */
				if (c == '\'') {
					statep->ls_scsparen.csstate = 0;
					ignore_backslash_newline--;
				}
				break;
			}
			if (statep->ls_scsparen.nparen == 0) {
				POP_STATE();
				*wp++ = 0; /* end of COMSUB */
			} else
				*wp++ = c;
			break;

		  case SASPAREN: /* $(( .. )) */
			/* todo: deal with $((...); (...)) properly */
			/* XXX should nest using existing state machine
			 *     (embed "..", $(...), etc.) */
			if (c == '(')
				statep->ls_sasparen.nparen++;
			else if (c == ')') {
				statep->ls_sasparen.nparen--;
				if (statep->ls_sasparen.nparen == 1) {
					/*(*/
					if ((c2 = getsc()) == ')') {
						POP_STATE();
						*wp++ = 0; /* end of EXPRSUB */
						break;
					} else {
						char *s;

						ungetsc(c2);
						/* mismatched parenthesis -
						 * assume we were really
						 * parsing a $(..) expression
						 */
						s = Xrestpos(ws, wp,
						     statep->ls_sasparen.start);
						memmove(s + 1, s, wp - s);
						*s++ = COMSUB;
						*s = '('; /*)*/
						wp++;
						statep->ls_scsparen.nparen = 1;
						statep->ls_scsparen.csstate = 0;
						state = statep->ls_state
							= SCSPAREN;
						
					}
				}
			}
			*wp++ = c;
			break;

		  case SBRACE:
			/*{*/
			if (c == '}') {
				POP_STATE();
				*wp++ = CSUBST;
				*wp++ = /*{*/ '}';
			} else
				goto Sbase1;
			break;

		  case STBRACE:
			/* Same as SBRACE, except (,|,) treated specially */
			/*{*/
			if (c == '}') {
				POP_STATE();
				*wp++ = CSUBST;
				*wp++ = /*{*/ '}';
			} else if (c == '|') {
				*wp++ = SPAT;
			} else if (c == '(') {
				*wp++ = OPAT;
				*wp++ = ' ';	/* simile for @ */
				PUSH_STATE(SPATTERN);
			} else
				goto Sbase1;
			break;

		  case SBQUOTE:
			if (c == '`') {
				*wp++ = 0;
				POP_STATE();
			} else if (c == '\\') {
				switch (c = getsc()) {
				  case '\\':
				  case '$': case '`':
					*wp++ = c;
					break;
				  case '"':
					if (statep->ls_sbquote.indquotes) {
						*wp++ = c;
						break;
					}
					/* fall through.. */
				  default:
					if (c) { /* trailing \ is lost */
						*wp++ = '\\';
						*wp++ = c;
					}
					break;
				}
			} else
				*wp++ = c;
			break;

		  case SWORD:	/* ONEWORD */
			goto Subst;

#ifdef KSH
		  case SLETPAREN:	/* LETEXPR: (( ... )) */
			/*(*/
			if (c == ')') {
				if (statep->ls_sletparen.nparen > 0)
				    --statep->ls_sletparen.nparen;
				/*(*/
				else if ((c2 = getsc()) == ')') {
					c = 0;
					*wp++ = CQUOTE;
					goto Done;
				} else
					ungetsc(c2);
			} else if (c == '(')
				/* parenthesis inside quotes and backslashes
				 * are lost, but at&t ksh doesn't count them
				 * either
				 */
				++statep->ls_sletparen.nparen;
			goto Sbase2;
#endif /* KSH */

		  case SHEREDELIM:	/* <<,<<- delimiter */
			/* XXX chuck this state (and the next) - use
			 * the existing states ($ and \`..` should be
			 * stripped of their specialness after the
			 * fact).
			 */
			/* here delimiters need a special case since
			 * $ and `..` are not to be treated specially
			 */
			if (c == '\\') {
				c = getsc();
				if (c) { /* trailing \ is lost */
					*wp++ = QCHAR;
					*wp++ = c;
				}
			} else if (c == '\'') {
				PUSH_STATE(SSQUOTE);
				*wp++ = OQUOTE;
				ignore_backslash_newline++;
			} else if (c == '"') {
				state = statep->ls_state = SHEREDQUOTE;
				*wp++ = OQUOTE;
			} else {
				*wp++ = CHAR;
				*wp++ = c;
			}
			break;

		  case SHEREDQUOTE:	/* " in <<,<<- delimiter */
			if (c == '"') {
				*wp++ = CQUOTE;
				state = statep->ls_state = SHEREDELIM;
			} else {
				if (c == '\\') {
					switch (c = getsc()) {
					  case '\\': case '"':
					  case '$': case '`':
						break;
					  default:
						if (c) { /* trailing \ lost */
							*wp++ = CHAR;
							*wp++ = '\\';
						}
						break;
					}
				}
				*wp++ = CHAR;
				*wp++ = c;
			}
			break;

		  case SPATTERN:	/* in *(...|...) pattern (*+?@!) */
			if ( /*(*/ c == ')') {
				*wp++ = CPAT;
				POP_STATE();
			} else if (c == '|') {
				*wp++ = SPAT;
			} else if (c == '(') {
				*wp++ = OPAT;
				*wp++ = ' ';	/* simile for @ */
				PUSH_STATE(SPATTERN);
			} else
				goto Sbase1;
			break;
		}
	}
Done:
	Xcheck(ws, wp);
	if (statep != &states[1])
		/* XXX figure out what is missing */
		yyerror("no closing quote\n");

	/* This done to avoid tests for SHEREDELIM wherever SBASE tested */
	if (state == SHEREDELIM)
		state = SBASE;

	dp = Xstring(ws, wp);
	if ((c == '<' || c == '>') && state == SBASE
	    && ((c2 = Xlength(ws, wp)) == 0
	        || (c2 == 2 && dp[0] == CHAR && digit(dp[1]))))
	{
		struct ioword *iop =
				(struct ioword *) alloc(sizeof(*iop), ATEMP);

		if (c2 == 2)
			iop->unit = dp[1] - '0';
		else
			iop->unit = c == '>'; /* 0 for <, 1 for > */

		c2 = getsc();
		/* <<, >>, <> are ok, >< is not */
		if (c == c2 || (c == '<' && c2 == '>')) {
			iop->flag = c == c2 ?
				  (c == '>' ? IOCAT : IOHERE) : IORDWR;
			if (iop->flag == IOHERE) {
				if ((c2 = getsc()) == '-') {
					iop->flag |= IOSKIP;
				} else {
					ungetsc(c2);
				}
			}
		} else if (c2 == '&')
			iop->flag = IODUP | (c == '<' ? IORDUP : 0);
		else {
			iop->flag = c == '>' ? IOWRITE : IOREAD;
			if (c == '>' && c2 == '|')
				iop->flag |= IOCLOB;
			else
				ungetsc(c2);
		}

		iop->name = (char *) 0;
		iop->delim = (char *) 0;
		iop->heredoc = (char *) 0;
		Xfree(ws, wp);	/* free word */
		yylval.iop = iop;
		return REDIR;
	}

	if (wp == dp && state == SBASE) {
		Xfree(ws, wp);	/* free word */
		/* no word, process LEX1 character */
		switch (c) {
		  default:
			return c;

		  case '|':
		  case '&':
		  case ';':
			if ((c2 = getsc()) == c)
				c = (c == ';') ? BREAK :
				    (c == '|') ? LOGOR :
				    (c == '&') ? LOGAND :
				    YYERRCODE;
#ifdef KSH
			else if (c == '|' && c2 == '&')
				c = COPROC;
#endif /* KSH */
			else
				ungetsc(c2);
			return c;

		  case '\n':
			gethere();
			if (cf & CONTIN)
				goto Again;
			return c;

		  case '(':  /*)*/
#ifdef KSH
			if ((c2 = getsc()) == '(') /*)*/
				/* XXX need to handle ((...); (...)) */
				c = MDPAREN;
			else
				ungetsc(c2);
#endif /* KSH */
			return c;
		  /*(*/
		  case ')':
			return c;
		}
	}

	*wp++ = EOS;		/* terminate word */
	yylval.cp = Xclose(ws, wp);
	if (state == SWORD
#ifdef KSH
		|| state == SLETPAREN
#endif /* KSH */
		)	/* ONEWORD? */
		return LWORD;
	ungetsc(c);		/* unget terminator */

	/* copy word to unprefixed string ident */
	for (sp = yylval.cp, dp = ident; dp < ident+IDENT && (c = *sp++) == CHAR; )
		*dp++ = *sp++;
	/* Make sure the ident array stays '\0' padded */
	memset(dp, 0, (ident+IDENT) - dp + 1);
	if (c != EOS)
		*ident = '\0';	/* word is not unquoted */

	if (*ident != '\0' && (cf&(KEYWORD|ALIAS))) {
		struct tbl *p;
		int h = hash(ident);

		/* { */
		if ((cf & KEYWORD) && (p = tsearch(&keywords, ident, h))
		    && (!(cf & ESACONLY) || p->val.i == ESAC || p->val.i == '}'))
		{
			afree(yylval.cp, ATEMP);
			return p->val.i;
		}
		if ((cf & ALIAS) && (p = tsearch(&aliases, ident, h))
		    && (p->flag & ISSET))
		{
			register Source *s;

			for (s = source; s->type == SALIAS; s = s->next)
				if (s->u.tblp == p)
					return LWORD;
			/* push alias expansion */
			s = pushs(SALIAS, source->areap);
			s->start = s->str = p->val.s;
			s->u.tblp = p;
			s->next = source;
			source = s;
			afree(yylval.cp, ATEMP);
			goto Again;
		}
	}

	return LWORD;
}

static void
gethere()
{
	register struct ioword **p;

	for (p = heres; p < herep; p++)
		readhere(*p);
	herep = heres;
}

/*
 * read "<<word" text into temp file
 */

static void
readhere(iop)
	struct ioword *iop;
{
	register int c;
	char *volatile eof;
	char *eofp;
	int skiptabs;
	XString xs;
	char *xp;
	int xpos;

	eof = evalstr(iop->delim, 0);

	if (!(iop->flag & IOEVAL))
		ignore_backslash_newline++;

	Xinit(xs, xp, 256, ATEMP);

	for (;;) {
		eofp = eof;
		skiptabs = iop->flag & IOSKIP;
		xpos = Xsavepos(xs, xp);
		while ((c = getsc()) != 0) {
			if (skiptabs) {
				if (c == '\t')
					continue;
				skiptabs = 0;
			}
			if (c != *eofp)
				break;
			Xcheck(xs, xp);
			Xput(xs, xp, c);
			eofp++;
		}
		/* Allow EOF here so commands with out trailing newlines
		 * will work (eg, ksh -c '...', $(...), etc).
		 */
		if (*eofp == '\0' && (c == 0 || c == '\n')) {
			xp = Xrestpos(xs, xp, xpos);
			break;
		}
		ungetsc(c);
		while ((c = getsc()) != '\n') {
			if (c == 0)
				yyerror("here document `%s' unclosed\n", eof);
			Xcheck(xs, xp);
			Xput(xs, xp, c);
		}
		Xcheck(xs, xp);
		Xput(xs, xp, c);
	}
	Xput(xs, xp, '\0');
	iop->heredoc = Xclose(xs, xp);

	if (!(iop->flag & IOEVAL))
		ignore_backslash_newline--;
}

void
#ifdef HAVE_PROTOTYPES
yyerror(const char *fmt, ...)
#else
yyerror(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	/* pop aliases and re-reads */
	while (source->type == SALIAS || source->type == SREREAD)
		source = source->next;
	source->str = null;	/* zap pending input */

	error_prefix(TRUE);
	SH_VA_START(va, fmt);
	shf_vfprintf(shl_out, fmt, va);
	va_end(va);
	errorf("%s", null);
}

/*
 * input for yylex with alias expansion
 */

Source *
pushs(type, areap)
	int type;
	Area *areap;
{
	register Source *s;

	s = (Source *) alloc(sizeof(Source), areap);
	s->type = type;
	s->str = null;
	s->start = NULL;
	s->line = 0;
	s->errline = 0;
	s->file = NULL;
	s->flags = 0;
	s->next = NULL;
	s->areap = areap;
	if (type == SFILE || type == SSTDIN) {
		char *dummy;
		Xinit(s->xs, dummy, 256, s->areap);
	} else
		memset(&s->xs, 0, sizeof(s->xs));
	return s;
}

static int
getsc__()
{
	register Source *s = source;
	register int c;

	while ((c = *s->str++) == 0) {
		s->str = NULL;		/* return 0 for EOF by default */
		switch (s->type) {
		  case SEOF:
			s->str = null;
			return 0;

		  case SSTDIN:
		  case SFILE:
			getsc_line(s);
			break;

		  case SWSTR:
			break;

		  case SSTRING:
			break;

		  case SWORDS:
			s->start = s->str = *s->u.strv++;
			s->type = SWORDSEP;
			break;

		  case SWORDSEP:
			if (*s->u.strv == NULL) {
				s->start = s->str = newline;
				s->type = SEOF;
			} else {
				s->start = s->str = space;
				s->type = SWORDS;
			}
			break;

		  case SALIAS:
			if (s->flags & SF_ALIASEND) {
				/* pass on an unused SF_ALIAS flag */
				source = s->next;
				source->flags |= s->flags & SF_ALIAS;
				s = source;
			} else if (*s->u.tblp->val.s
				 && isspace((unsigned char)strchr(s->u.tblp->val.s, 0)[-1]))
			{
				source = s = s->next;	/* pop source stack */
				/* Note that this alias ended with a space,
				 * enabling alias expansion on the following
				 * word.
				 */
				s->flags |= SF_ALIAS;
			} else {
				/* At this point, we need to keep the current
				 * alias in the source list so recursive
				 * aliases can be detected and we also need
				 * to return the next character.  Do this
				 * by temporarily popping the alias to get
				 * the next character and then put it back
				 * in the source list with the SF_ALIASEND
				 * flag set.
				 */
				source = s->next;	/* pop source stack */
				source->flags |= s->flags & SF_ALIAS;
				c = getsc__();
				if (c) {
					s->flags |= SF_ALIASEND;
					s->ugbuf[0] = c; s->ugbuf[1] = '\0';
					s->start = s->str = s->ugbuf;
					s->next = source;
					source = s;
				} else {
					s = source;
					/* avoid reading eof twice */
					s->str = NULL;
					break;
				}
			}
			continue;

		  case SREREAD:
			if (s->start != s->ugbuf) /* yuck */
				afree(s->u.freeme, ATEMP);
			source = s = s->next;
			continue;
		}
		if (s->str == NULL) {
			s->type = SEOF;
			s->start = s->str = null;
			return '\0';
		}
		if (s->flags & SF_ECHO) {
			shf_puts(s->str, shl_out);
			shf_flush(shl_out);
		}
	}
	return c;
}

static void
getsc_line(s)
	Source *s;
{
	char *xp = Xstring(s->xs, xp);
	int interactive = Flag(FTALKING) && s->type == SSTDIN;
	int have_tty = interactive && (s->flags & SF_TTY);

	/* Done here to ensure nothing odd happens when a timeout occurs */
	XcheckN(s->xs, xp, LINE);
	*xp = '\0';
	s->start = s->str = xp;

#ifdef KSH
	if (have_tty && ksh_tmout) {
		ksh_tmout_state = TMOUT_READING;
		alarm(ksh_tmout);
	}
#endif /* KSH */
#ifdef EDIT
	if (have_tty && (0
# ifdef VI
			 || Flag(FVI)
# endif /* VI */
# ifdef EMACS
			 || Flag(FEMACS) || Flag(FGMACS)
# endif /* EMACS */
		))
	{
		int nread;

		nread = x_read(xp, LINE);
		if (nread < 0)	/* read error */
			nread = 0;
		xp[nread] = '\0';
		xp += nread;
	}
	else
#endif /* EDIT */
	{
		if (interactive) {
			pprompt(prompt, 0);
		} else
			s->line++;

		while (1) {
			char *p = shf_getse(xp, Xnleft(s->xs, xp), s->u.shf);

			if (!p && shf_error(s->u.shf)
			    && shf_errno(s->u.shf) == EINTR)
			{
				shf_clearerr(s->u.shf);
				if (trap)
					runtraps(0);
				continue;
			}
			if (!p || (xp = p, xp[-1] == '\n'))
				break;
			/* double buffer size */
			xp++; /* move past null so doubling works... */
			XcheckN(s->xs, xp, Xlength(s->xs, xp));
			xp--; /* ...and move back again */
		}
		/* flush any unwanted input so other programs/builtins
		 * can read it.  Not very optimal, but less error prone
		 * than flushing else where, dealing with redirections,
		 * etc..
		 * todo: reduce size of shf buffer (~128?) if SSTDIN
		 */
		if (s->type == SSTDIN)
			shf_flush(s->u.shf);
	}
	/* XXX: temporary kludge to restore source after a
	 * trap may have been executed.
	 */
	source = s;
#ifdef KSH
	if (have_tty && ksh_tmout)
	{
		ksh_tmout_state = TMOUT_EXECUTING;
		alarm(0);
	}
#endif /* KSH */
	s->start = s->str = Xstring(s->xs, xp);
	strip_nuls(Xstring(s->xs, xp), Xlength(s->xs, xp));
	/* Note: if input is all nulls, this is not eof */
	if (Xlength(s->xs, xp) == 0) { /* EOF */
		if (s->type == SFILE)
			shf_fdclose(s->u.shf);
		s->str = NULL;
	} else if (interactive) {
#ifdef HISTORY
		char *p = Xstring(s->xs, xp);
		if (cur_prompt == PS1)
			while (*p && ctype(*p, C_IFS) && ctype(*p, C_IFSWS))
				p++;
		if (*p) {
# ifdef EASY_HISTORY
			if (cur_prompt == PS2)
				histappend(Xstring(s->xs, xp), 1);
			else
# endif /* EASY_HISTORY */
			{
				s->line++;
				histsave(s->line, s->str, 1);
			}
		}
#endif /* HISTORY */
	}
	if (interactive)
		set_prompt(PS2, (Source *) 0);
}

void
set_prompt(to, s)
	int to;
	Source *s;
{
	cur_prompt = to;

	switch (to) {
	case PS1: /* command */
#ifdef KSH
		/* Substitute ! and !! here, before substitutions are done
		 * so ! in expanded variables are not expanded.
		 * NOTE: this is not what at&t ksh does (it does it after
		 * substitutions, POSIX doesn't say which is to be done.
		 */
		{
			struct shf *shf;
			char * volatile ps1;
			Area *saved_atemp;

			ps1 = str_val(global("PS1"));
			shf = shf_sopen((char *) 0, strlen(ps1) * 2,
				SHF_WR | SHF_DYNAMIC, (struct shf *) 0);
			while (*ps1) {
				if (*ps1 != '!' || *++ps1 == '!')
					shf_putchar(*ps1++, shf);
				else
					shf_fprintf(shf, "%d",
						s ? s->line + 1 : 0);
			}
			ps1 = shf_sclose(shf);
			saved_atemp = ATEMP;
			newenv(E_ERRH);
			if (ksh_sigsetjmp(e->jbuf, 0)) {
				prompt = safe_prompt;
				/* Don't print an error - assume it has already
				 * been printed.  Reason is we may have forked
				 * to run a command and the child may be
				 * unwinding its stack through this code as it
				 * exits.
				 */
			} else
				prompt = str_save(substitute(ps1, 0),
						 saved_atemp);
			quitenv();
		}
#else /* KSH */
		prompt = str_val(global("PS1"));
#endif /* KSH */
		break;

	case PS2: /* command continuation */
		prompt = str_val(global("PS2"));
		break;
	}
}

/* See also related routine, promptlen() in edit.c */
void
pprompt(cp, ntruncate)
	const char *cp;
	int ntruncate;
{
#if 0
	char nbuf[32];
	int c;

	while (*cp != 0) {
		if (*cp != '!')
			c = *cp++;
		else if (*++cp == '!')
			c = *cp++;
		else {
			int len;
			char *p;

			shf_snprintf(p = nbuf, sizeof(nbuf), "%d",
				source->line + 1);
			len = strlen(nbuf);
			if (ntruncate) {
				if (ntruncate >= len) {
					ntruncate -= len;
					continue;
				}
				p += ntruncate;
				len -= ntruncate;
				ntruncate = 0;
			}
			shf_write(p, len, shl_out);
			continue;
		}
		if (ntruncate)
			--ntruncate;
		else
			shf_putc(c, shl_out);
	}
#endif /* 0 */
	shf_puts(cp + ntruncate, shl_out);
	shf_flush(shl_out);
}

/* Read the variable part of a ${...} expression (ie, up to but not including
 * the :[-+?=#%] or close-brace.
 */
static char *
get_brace_var(wsp, wp)
	XString *wsp;
	char *wp;
{
	enum parse_state {
			   PS_INITIAL, PS_SAW_HASH, PS_IDENT,
			   PS_NUMBER, PS_VAR1, PS_END
			 }
		state;
	char c;

	state = PS_INITIAL;
	while (1) {
		c = getsc();
		/* State machine to figure out where the variable part ends. */
		switch (state) {
		  case PS_INITIAL:
			if (c == '#') {
				state = PS_SAW_HASH;
				break;
			}
			/* fall through.. */
		  case PS_SAW_HASH:
			if (letter(c))
				state = PS_IDENT;
			else if (digit(c))
				state = PS_NUMBER;
			else if (ctype(c, C_VAR1))
				state = PS_VAR1;
			else
				state = PS_END;
			break;
		  case PS_IDENT:
			if (!letnum(c)) {
				state = PS_END;
				if (c == '[') {
					char *tmp, *p;

					if (!arraysub(&tmp))
						yyerror("missing ]\n");
					*wp++ = c;
					for (p = tmp; *p; ) {
						Xcheck(*wsp, wp);
						*wp++ = *p++;
					}
					afree(tmp, ATEMP);
					c = getsc(); /* the ] */
				}
			}
			break;
		  case PS_NUMBER:
			if (!digit(c))
				state = PS_END;
			break;
		  case PS_VAR1:
			state = PS_END;
			break;
		  case PS_END: /* keep gcc happy */
			break;
		}
		if (state == PS_END) {
			*wp++ = '\0';	/* end of variable part */
			ungetsc(c);
			break;
		}
		Xcheck(*wsp, wp);
		*wp++ = c;
	}
	return wp;
}

/*
 * Save an array subscript - returns true if matching bracket found, false
 * if eof or newline was found.
 * (Returned string double null terminated)
 */
static int
arraysub(strp)
	char **strp;
{
	XString ws;
	char	*wp;
	char	c;
	int 	depth = 1;	/* we are just past the initial [ */

	Xinit(ws, wp, 32, ATEMP);

	do {
		c = getsc();
		Xcheck(ws, wp);
		*wp++ = c;
		if (c == '[')
			depth++;
		else if (c == ']')
			depth--;
	} while (depth > 0 && c && c != '\n');

	*wp++ = '\0';
	*strp = Xclose(ws, wp);

	return depth == 0 ? 1 : 0;
}

/* Unget a char: handles case when we are already at the start of the buffer */
static const char *
ungetsc(c)
	int c;
{
	if (backslash_skip)
		backslash_skip--;
	/* Don't unget eof... */
	if (source->str == null && c == '\0')
		return source->str;
	if (source->str > source->start)
		source->str--;
	else {
		Source *s;

		s = pushs(SREREAD, source->areap);
		s->ugbuf[0] = c; s->ugbuf[1] = '\0';
		s->start = s->str = s->ugbuf;
		s->next = source;
		source = s;
	}
	return source->str;
}


/* Called to get a char that isn't a \newline sequence. */
static int
getsc_bn ARGS((void))
{
	int c, c2;

	if (ignore_backslash_newline)
		return getsc_();

	if (backslash_skip == 1) {
		backslash_skip = 2;
		return getsc_();
	}

	backslash_skip = 0;

	while (1) {
		c = getsc_();
		if (c == '\\') {
			if ((c2 = getsc_()) == '\n')
				/* ignore the \newline; get the next char... */
				continue;
			ungetsc(c2);
			backslash_skip = 1;
		}
		return c;
	}
}

static Lex_state *
push_state_(si, old_end)
	State_info *si;
	Lex_state *old_end;
{
	Lex_state	*new = alloc(sizeof(Lex_state) * STATE_BSIZE, ATEMP);

	new[0].ls_info.base = old_end;
	si->base = &new[0];
	si->end = &new[STATE_BSIZE];
	return &new[1];
}

static Lex_state *
pop_state_(si, old_end)
	State_info *si;
	Lex_state *old_end;
{
	Lex_state *old_base = si->base;

	si->base = old_end->ls_info.base - STATE_BSIZE;
	si->end = old_end->ls_info.base;

	afree(old_base, ATEMP);

	return si->base + STATE_BSIZE - 1;
}
