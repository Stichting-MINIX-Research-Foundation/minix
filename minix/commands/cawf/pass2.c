/*
 *	pass2.c - cawf(1) pass 2 function
 */

/*
 *	Copyright (c) 1991 Purdue University Research Foundation,
 *	West Lafayette, Indiana 47907.  All rights reserved.
 *
 *	Written by Victor A. Abell <abe@mace.cc.purdue.edu>,  Purdue
 *	University Computing Center.  Not derived from licensed software;
 *	derived from awf(1) by Henry Spencer of the University of Toronto.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to alter it and redistribute
 *	it freely, subject to the following restrictions:
 *
 *	1. The author is not responsible for any consequences of use of
 *	   this software, even if they arise from flaws in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *	   by explicit claim or by omission.  Credits must appear in the
 *	   documentation.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *	   be misrepresented as being the original software.  Credits must
 *	   appear in the documentation.
 *
 *	4. This notice may not be removed or altered.
 */

#include "cawf.h"
#include <ctype.h>

/*
 * Pass2(line) - process the nroff requests in a line and break
 *		 text into words for pass 3
 */

void Pass2(unsigned char *line) {
	int brk;			/* request break status */
	unsigned char buf[MAXLINE];	/* working buffer */
	unsigned char c;		/* character buffer */
	double d;			/* temporary double */
	double exscale;			/* expression scaling factor */
	double expr[MAXEXP];            /* expressions */
	unsigned char exsign[MAXEXP];	/* expression signs */
	int i, j;			/* temporary indexes */
	int inword;			/* word processing status */
	int nexpr;			/* number of expressions */
	unsigned char nm[4], nm1[4];	/* names */
	int nsp;			/* number of spaces */
	unsigned char op;		/* expression term operator */
	unsigned char opstack[MAXSP];	/* expression operation stack */
	unsigned char period;		/* end of word status */
	unsigned char *s1, *s2, *s3;	/* temporary string pointers */
	double sexpr[MAXEXP];           /* signed expressions */
	int sp;				/* expression stack pointer */
	unsigned char ssign;		/* expression's starting sign */
	int tabpos;			/* tab position */
	double tscale;			/* term scaling factor */
	double tval;			/* term value */
	double val;			/* term value */
	double valstack[MAXSP];		/* expression value stack */
	unsigned char xbuf[MAXLINE];	/* expansion buffer */

	if (line == NULL) {
    /*
     * End of macro expansion.
     */
		Pass3(DOBREAK, (unsigned char *)"need", NULL, 999);
		return;
	}
    /*
     * Adjust line number.
     */
	if (Lockil == 0)
		P2il++;
    /*
     * Empty line - "^[ \t]*$" or "^\\\"".
     */
	if (regexec(Pat[6].pat, line)
	||  strncmp((char *)line, "\\\"", 2) == 0) {
		Pass3(DOBREAK, (unsigned char *)"space", NULL, 0);
		return;
	}
    /*
     * Line begins with white space.
     */
	if (*line == ' ' || *line == '\t') {
		Pass3(DOBREAK, (unsigned char *)"flush", NULL, 0);
		Pass3(0, (unsigned char *)"", NULL, 0);
	}
	if (*line != '.' && *line != '\'') {
    /*
     * Line contains text (not an nroff request).
     */
		if (Font[0] == 'R' && Backc == 0 && Aftnxt == NULL
		&&  regexec(Pat[7].pat, line) == 0) {
		    /*
		     * The font is Roman, there is no "\\c" or "after next"
		     * trap pending and and the line has no '\\', '\t', '-',
		     * or "  "  (regular expression "\\|\t|-|  ").
		     *
		     * Output each word of the line as "<length> <word>".
		     */
			for (s1 = line;;) {
				while (*s1 == ' ')
					s1++;
				if (*s1 == '\0')
					break;
				for (s2 = s1, s3 = buf; *s2 && *s2 != ' ';)
				    *s3++ = Trtbl[(int)*s2++];
				*s3 = '\0';
				Pass3((s2 - s1), buf, NULL, 0);
				s1 = *s2 ? ++s2 : s2;
			}
		    /*
		     * Line terminates with punctuation and optional
		     * bracketing (regular expression "[.!?:][\])'\"*]*$").
		     */
			if (regexec(Pat[8].pat, line))
				Pass3(NOBREAK, (unsigned char *)"gap", NULL, 2);
			if (Centering > 0) {
				Pass3(DOBREAK,(unsigned char *)"center", NULL,
					0);
				Centering--;
			} else if (Fill == 0)
				Pass3(DOBREAK, (unsigned char *)"flush", NULL,
					0);
			return;
		}
	    /*
	     * Line must be scanned a character at a time.
	     */
		inword = nsp = tabpos = 0;
		period = '\0';
		for (s1 = line;; s1++) {
		    /*
		     * Space or TAB causes state transition.
		     */
			if (*s1 == '\0' || *s1 == ' ' || *s1 == '\t') {
				if (inword) {
					if (!Backc) {
						Endword();
						Pass3(Wordl, Word, NULL, 0);
						if (Uhyph) {
						  Pass3(NOBREAK,
						    (unsigned char *)"nohyphen",
						    NULL, 0);
						}
					}
					inword = 0;
					nsp = 0;
				}
				if (*s1 == '\0')
					break;
			} else {
				if (inword == 0) {
					if (Backc == 0) {
						Wordl = Wordx = 0;
						Uhyph = 0;
					}
					Backc = 0;
					inword = 1;
					if (nsp > 1) {
						Pass3(NOBREAK,
						    (unsigned char *)"gap",
						    NULL, nsp);
					}
				}
			}
		    /*
		     * Process a character.
		     */
			switch (*s1) {
		    /*
		     * Space
		     */
	     		case ' ':
				nsp++;
				period = '\0';
				break;
		    /*
		     * TAB
		     */
	     		case '\t':
				tabpos++;
				if (tabpos <= Ntabs) {
					Pass3(NOBREAK,
					    (unsigned char *)"tabto", NULL,
					    Tabs[tabpos-1]);
				}
				nsp = 0;
				period = '\0';
				break;
		    /*
		     * Hyphen if word is being assembled
		     */
			case '-':
				if (Wordl <= 0)
				    goto ordinary_char;
				if ((i = Findhy(NULL, 0, 0)) < 0) {
				    Error(WARN, LINE, " no hyphen for font ",
					(char *)Font);
				    return;
				}
				Endword();
				Pass3(Wordl, Word, NULL, Hychar[i].len);
				Pass3(NOBREAK, (unsigned char *)"userhyphen",
				    Hychar[i].str, Hychar[i].len);
				Wordl = Wordx = 0;
				period = '\0';
				Uhyph = 1;
				break;
		    /*
		     * Backslash
		     */
			case '\\':
				s1++;
				switch(*s1) {
			    /*
			     * Comment - "\\\""
			     */
				case '"':
					while (*(s1+1))
						s1++;
					break;
			    /*
			     * Change font - "\\fN"
			     */
				case 'f':
					s1 = Asmcode(&s1, nm);
					if (nm[0] == 'P') {
					    Font[0] = Prevfont;
					    break;
					}
					for (i = 0; Fcode[i].nm; i++) {
					    if (Fcode[i].nm == nm[0])
						break;
					}
					if (Fcode[i].nm == '\0'
					||  nm[1] != '\0') {
					    Error(WARN, LINE, " unknown font ",
					    	(char *)nm);
					    break;
					}
					if (Fcode[i].status != '1') {
					    Error(WARN, LINE,
						" font undefined ", (char *)nm);
					    break;
					} else {
					    Prevfont = Font[0];
					    Font[0] = nm[0];
					}
					break;
			    /*
			     * Positive horizontal motion - "\\h\\n(NN" or
			     * "\\h\\nN"
			     */
				case 'h':
					if (s1[1] != '\\' || s1[2] != 'n') {
					    Error(WARN, LINE,
						" no \\n after \\h", NULL);
					    break;
					}
					s1 +=2;
					s1 = Asmcode(&s1, nm);
					if ((i = Findnum(nm, 0, 0)) < 0)
						goto unknown_num;
					if ((j = Numb[i].val) < 0) {
					    Error(WARN, LINE, " \\h < 0 ",
					    NULL);
					    break;
					}
					if (j == 0)
						break;
					if ((strlen((char *)s1+1) + j + 1)
					>=  MAXLINE)
						goto line_too_long;
					for (s2 = &xbuf[1]; j; j--)
						*s2++ = ' ';
					(void) strcpy((char *)s2, (char *)s1+1);
					s1 = xbuf;
					break;
			    /*
			     * Save current position in register if "\\k<reg>"
			     */
			        case 'k':
					s1 = Asmcode(&s1, nm);
					if ((i = Findnum(nm, 0, 0)) < 0)
					    i = Findnum(nm, 0, 1);
					Numb[i].val =
						(int)((double)Outll * Scalen);
					break;
			    /*
			     * Interpolate number - "\\n(NN" or "\\nN"
			     */
				case 'n':
					s1 = Asmcode(&s1, nm);
					if ((i = Findnum(nm, 0, 0)) < 0) {
unknown_num:
					    Error(WARN, LINE,
					        " unknown number register ",
						(char *)nm);
					    break;
					}
					(void) sprintf((char *)buf, "%d",
					    Numb[i].val);
					if ((strlen((char *)buf)
					   + strlen((char *)s1+1) + 1)
					>=  MAXLINE) {
line_too_long:
					    Error(WARN, LINE, " line too long",
					        NULL);
					    break;
					}
					(void) sprintf((char *)buf, "%d%s",
					    Numb[i].val, (char *)s1+1);
					(void) strcpy((char *)&xbuf[1],
						(char *)buf);
				        s1 = xbuf;
					break;
			    /*
			     * Change size - "\\s[+-][0-9]" - NOP
			     */
				case 's':
					s1++;
					if (*s1 == '+' || *s1 == '-')
						s1++;
					while (*s1 && isdigit(*s1))
						s1++;
					s1--;
					break;
			    /*
			     * Continue - "\\c"
			     */
				case 'c':
					Backc = 1;
					break;
			    /*
			     * Interpolate string - "\\*(NN" or "\\*N"
			     */
				case '*':
					s1 = Asmcode(&s1, nm);
					s2 = Findstr(nm, NULL, 0);
					if (*s2 != '\0') {
					    if ((strlen((char *)s2)
					       + strlen((char *)s1+1) + 1)
					    >=  MAXLINE)
						goto line_too_long;
					    (void) sprintf((char *)buf, "%s%s",
						(char *)s2, (char *)s1+1);
					    (void) strcpy((char *)&xbuf[1],
						(char *)buf);
					    s1 = xbuf;
					}
					break;
			    /*
			     * Discretionary hyphen - "\\%"
			     */
				case '%':
					if (Wordl <= 0)
					    break;
					if ((i = Findhy(NULL, 0, 0)) < 0) {
					    Error(WARN, LINE,
					        " no hyphen for font ",
						(char *)Font);
					    break;
					}
					Endword();
					Pass3(Wordl, Word, NULL, Hychar[i].len);
					Pass3(NOBREAK,
					    (unsigned char *) "hyphen",
					    Hychar[i].str, Hychar[i].len);
					Wordl = Wordx = 0;
					Uhyph = 1;
					break;
			    /*
			     * None of the above - may be special character
			     * name.
			     */
				default:
					s2 = s1--;
					s1 = Asmcode(&s1, nm);
					if ((i = Findchar(nm, 0, NULL, 0)) < 0){
					    s1 = s2;
					    goto ordinary_char;
					}
					if (strcmp((char *)nm, "em") == 0
					&& Wordx > 0) {
				    /*
				     * "\\(em" is a special case when a word
				     * has been assembled, because of
				     * hyphenation.
				     */
					    Endword();
					    Pass3(Wordl, Word, NULL,
					        Schar[i].len);
					    Pass3(NOBREAK,
						(unsigned char *)"userhyphen",
					        Schar[i].str, Schar[i].len);
				            Wordl = Wordx = 0;
					    period = '\0';
					    Uhyph = 1;
			 		}
				    /*
				     * Interpolate a special character
				     */
					if (Str2word(Schar[i].str,
					    strlen((char *)Schar[i].str)) != 0)
						return;
				        Wordl += Schar[i].len;
					period = '\0';
				}
				break;
		    /*
		     * Ordinary character
		     */
			default:
ordinary_char:
				if (Str2word(s1, 1) != 0)
					return;
				Wordl++;
				if (*s1 == '.' || *s1 == '!'
				||  *s1 == '?' || *s1 == ':')
				    period = '.';
				else if (period == '.') {
				    nm[0] = *s1;
				    nm[1] = '\0';
				    if (regexec(Pat[13].pat, nm) == 0)
					 period = '\0';
				}
			}
		}
	    /*
	     * End of line processing
	     */
     		if (!Backc) {
			if (period == '.')
				Pass3(NOBREAK, (unsigned char *)"gap", NULL, 2);
			if (Centering > 0) {
				Pass3(DOBREAK, (unsigned char *)"center", NULL,
				0);
				Centering--;
			} else if (!Fill)
				Pass3(DOBREAK, (unsigned char *)"flush", NULL,
				0);
		}
		if (Aftnxt == NULL)
			return;
		/* else fall through to process an "after next trap */
	}
    /*
     * Special -man macro handling.
     */
	if (Marg == MANMACROS) {
	    /*
	     * A text line - "^[^.]" - is only processed when there is an
	     * "after next" directive.
	     */
		if (*line != '.' && *line != '\'') {
			if (Aftnxt != NULL) {
				if (regexec(Pat[9].pat, Aftnxt))  /* ",fP" */
					Font[0] = Prevfont;
				if (regexec(Pat[16].pat, Aftnxt))  /* ",fR" */
					Font[0] = 'R';
				if (regexec(Pat[10].pat, Aftnxt))  /* ",tP" */
					Pass3(DOBREAK,
						(unsigned char *)"toindent",
						NULL, 0);
				Free(&Aftnxt);
			}
			return;
		}
	    /*
	     * Special footer handling - "^.lF"
	     */
		if (line[1] == 'l' && line[2] == 'F') {
			s1 = Findstr((unsigned char *)"by", NULL, 0);
			s2 = Findstr((unsigned char *)"nb", NULL, 0);
			if (*s1 == '\0' || *s2 == '\0')
				(void) sprintf((char *)buf, "%s%s",
					(char *)s1, (char *)s2);
			else
				(void) sprintf((char *)buf, "%s; %s",
					(char *)s1, (char *)s2);
			Pass3(NOBREAK, (unsigned char *)"LF", buf, 0);
			return;
		}
	}
    /*
     * Special -ms macro handling.
     */
	if (Marg == MSMACROS) {
	    /*
	     * A text line - "^[^.]" - is only processed when there is an
	     * "after next" directive.
	     */
		if (*line != '.' && *line != '\'') {
			if (Aftnxt != NULL) {
				if (regexec(Pat[10].pat, Aftnxt))  /* ",tP" */
					Pass3(DOBREAK,
						(unsigned char *)"toindent",
						NULL, 0);
				Free(&Aftnxt);
			}
			return;
		}
	    /*
	     * Numbered headings - "^[.']nH"
	     */
		if (line[1] == 'n' && line[2] == 'H') {
			s1 = Field(2, line, 0);
			if (s1 != NULL) {
				i = atoi((char *)s1) - 1;	
				if (i < 0) {
					for (j = 0; j < MAXNHNR; j++) {
						Nhnr[j] = 0;
					}
					i = 0;
				} else if (i >= MAXNHNR) {
				    (void) sprintf((char *)buf,
					" over NH limit (%d)", MAXNHNR);
				    Error(WARN, LINE, (char *)buf, NULL);
				}
			} else
				i = 0;
			Nhnr[i]++;
			for (j = i + 1; j < MAXNHNR; j++) {
				Nhnr[j] = 0;
			}
			s1 = buf;
			for (j = 0; j <= i; j++) {
				(void) sprintf((char *)s1, "%d.", Nhnr[j]);
				s1 = buf + strlen((char *)buf);
			}
			(void) Findstr((unsigned char *)"Nh", buf, 1);
			return;
		}
	}
    /*
     * Remaining lines should begin with a '.' or '\'' unless an "after next"
     * trap has failed.
     */
	if (*line != '.' && *line != '\'') {
		if (Aftnxt != NULL)
			Error(WARN, LINE, " failed .it: ", (char *)Aftnxt);
		else
			Error(WARN, LINE, " unrecognized line ", NULL);
		return;
	}
	brk = (*line == '.') ? DOBREAK : NOBREAK;
    /*
     * Evaluate expressions for "^[.'](ta|ll|ls|in|ti|po|ne|sp|pl|nr)"
     * Then process the requests.
     */
	if (regexec(Pat[11].pat, &line[1])) {
	    /*
	     * Establish default scale factor.
	     */
		if ((line[1] == 'n' && line[2] == 'e')
		||  (line[1] == 's' && line[2] == 'p')
		||  (line[1] == 'p' && line[2] == 'l'))
			exscale = Scalev;
		else if (line[1] == 'n' && line[2] == 'r')
			exscale = Scaleu;
		else
			exscale = Scalen;
	    /*
	     * Determine starting argument.
	     */
		if (line[1] == 'n' && line[2] == 'r')
			s1 = Field(2, &line[3], 0);
		else
			s1 = Field(1, &line[3], 0);
	    /*
	     * Evaluate expressions.
	     */
		for (nexpr = 0; s1 != NULL &&*s1 != '\0'; ) {
			while (*s1 == ' ' || *s1 == '\t')
				s1++;
			if (*s1 == '+' || *s1 == '-')
				ssign = *s1++;
			else
				ssign = '\0';
		    /*
		     * Process terms.
		     */
			val = 0.0;
			sp = -1;
			c = '+';
			s1--;
			while (c == '+' || c == '*' || c == '%'
			||  c == ')' || c == '-' || c == '/') {
			    op = c;
			    s1++;
			    tscale = exscale;
			    tval = 0.0;
			/*
			 * Pop stack on right parenthesis.
			 */
			    if (op == ')') {
				tval = val;
				if (sp >= 0) {
				    val = valstack[sp];
				    op = opstack[sp];
				    sp--;
				} else {
				    Error(WARN, LINE,
					" expression stack underflow", NULL);
				    return;
				}
				tscale = Scaleu;
			/*
			 * Push stack on left parenthesis.
			 */
			    } else if (*s1 == '(') {
				sp++;
				if (sp >= MAXSP) {
				    Error(WARN, LINE,
				       " expression stack overflow", NULL);
				    return;
				}
				valstack[sp] = val;
				opstack[sp] = op;
				val = 0.0;
				c = '+';
				continue;
			    } else if (*s1 == '\\') {
			      s1++;
			      switch(*s1) {
			/*
			 * "\\"" begins a comment.
			 */
			      case '"':
				while (*s1)
					s1++;
				break;
			/*
			 * Crude width calculation for "\\w"
			 */
			      case 'w':
				s2 = ++s1;
				if (*s1) {
				    s1++;
				    while (*s1 && *s1 != *s2)
					s1++;
				    tval = (double) (s1 - s2 - 1) * Scalen;
				    if (*s1)
					s1++;
				}
				break;
			/*
			 * Interpolate number register if "\\n".
			 */
			      case 'n':
				s1 = Asmcode(&s1, nm);
				if ((i = Findnum(nm, 0, 0)) >= 0)
				    tval = Numb[i].val;
			        s1++;
			     }
			/*
			 * Assemble numeric value.
			 */
			    } else if (*s1 == '.' || isdigit(*s1)) {
				for (i = 0; isdigit(*s1) || *s1 == '.'; s1++) {
				    if (*s1 == '.') {
					i = 10;
					continue;
				    }
				    d = (double) (*s1 - '0');
				    if (i) {
					tval = tval + (d / (double) i);
					i = i * 10;
				    } else
					tval = (tval * 10.0) + d;
				}
			    } else {
			/*
			 * It's not an expression.  Ignore extra scale.
			 */
				if ((i = Findscale((int)*s1, 0.0, 0)) < 0) {
				    (void) sprintf((char *)buf,
					" \"%s\" isn't an expression",
					(char *)s1);
				    Error(WARN, LINE, (char *)buf, NULL);
				}
				s1++;
			    }
			/*
			 * Add term to expression value.
			 */
			    if ((i = Findscale((int)*s1, 0.0, 0)) >= 0) {
				tval *= Scale[i].val;
				s1++;
			    } else
				tval *= tscale;
			    switch (op) {
			    case '+':
				val += tval;
				break;
			    case '-':
				val -= tval;
				break;
			    case '*':
				val *= tval;
				break;
			    case '/':
			    case '%':
				i = (int) val;
				j = (int) tval;
				if (j == 0) {
				    Error(WARN, LINE,
					(*s1 == '/') ? "div" : "mod",
				        " by 0");
				    return;
				}
				if (op == '/')
					val = (double) (i / j);
				else
					val = (double) (i % j);
				break;
			    }
			    c = *s1;
			}
		    /*
		     * Save expression value and sign.
		     */
			if (nexpr >= MAXEXP) {
				(void) sprintf((char *)buf,
				    " at expression limit of %d", MAXEXP);
				Error(WARN, LINE, (char *)buf, NULL);
				return;
			}
			exsign[nexpr] = ssign;
			expr[nexpr] = val;
			if (ssign == '-')
				sexpr[nexpr] = -1.0 * val;
			else
				sexpr[nexpr] = val;
			nexpr++;
			while (*s1 == ' ' || *s1 == '\t')
				s1++;
		}
	    /*
	     * Set parameters "(ll|ls|in|ti|po|pl)"
	     */
		if (regexec(Pat[12].pat, &line[1])) {
			nm[0] = line[1];
			nm[1] = line[2];
			if ((i = Findparms(nm)) < 0) {
				Error(WARN, LINE,
				    " can't find parameter register ",
				    (char *)nm);
				return;
			}
			if (nexpr == 0 || exscale == 0.0)
				j = Parms[i].prev;
			else if (exsign[0] == '\0'
			     ||  (nm[0] == 't' && nm[1] == 'i'))
				 j = (int)(sexpr[0] / exscale);
			else
				j = Parms[i].val + (int)(sexpr[0] / exscale);
			Parms[i].prev = Parms[i].val;
			Parms[i].val = j;
			nm[0] = (nexpr) ? exsign[0] : '\0';     /* for .ti */
			nm[1] = '\0';
			Pass3(brk, (unsigned char *)Parms[i].cmd, nm, j);
			return;
		}
		if (line[1] == 'n') {
			switch(line[2]) {
	    /*
	     * Need - "^[.']ne <expression>"
	     */
			case 'e':
				if (nexpr && Scalev > 0.0)
					i = (int) ((expr[0]/Scalev) + 0.99);
				else
					i = 0;
				Pass3(DOBREAK, (unsigned char *)"need", NULL,
					i);
				return;
	    /*
	     * Number - "^[.']nr <name> <expression>"
	     */
			case 'r':
				if ((s1 = Field(2, line, 0)) == NULL) {
				    Error(WARN, LINE, " bad number register",
				        NULL);
				    return;
				}
				if ((i = Findnum(s1, 0, 0)) < 0)
				    i = Findnum(s1, 0, 1);
				if (nexpr < 1) {
				    Numb[i].val = 0;
				    return;
				}
				if (exsign[0] == '\0')
				    Numb[i].val = (int) expr[0];
				else
				    Numb[i].val += (int) sexpr[0];
				return;
			}
		}
	    /*
	     * Space - "^[.']sp <expression>"
	     */
		if (line[1] == 's' && line[2] == 'p') {
			if (nexpr == 0)
				i = 1;
			else
				i = (int)((expr[0] / Scalev) + 0.99);
			while (i--)
				Pass3(brk, (unsigned char *)"space", NULL, 0);
			return;
		}
	    /*
	     * Tab positions - "^[.']ta <pos1> <pos2> . . ."
	     */
     		if (line[1] == 't' && line[2] == 'a') {
			tval = 0.0;
			for (j = 0; j < nexpr; j++) {
				if (exsign[j] == '\0')
					tval = expr[j];
				else
					tval += sexpr[j];
				Tabs[j] = (int) (tval / Scalen);
			}
			Ntabs = nexpr;
			return;
		}
	}
    /*
     * Process all other nroff requests via Nreq().
     */
	(void) Nreq(line, brk);
	return;
}
