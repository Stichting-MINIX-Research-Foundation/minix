/*	$NetBSD: io.c,v 1.15 2014/09/04 04:06:07 mrg Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1976 Board of Trustees of the University of Illinois.
 * Copyright (c) 1985 Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)io.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: io.c,v 1.15 2014/09/04 04:06:07 mrg Exp $");
#endif
#endif				/* not lint */

#include <ctype.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "indent_globs.h"

int     comment_open;
static  int paren_target;

void
dump_line(void)
{				/* dump_line is the routine that actually
				 * effects the printing of the new source. It
				 * prints the label section, followed by the
				 * code section with the appropriate nesting
				 * level, followed by any comments */
	int     cur_col, target_col;
	static int not_first_line;

	target_col = 0;
	if (ps.procname[0]) {
		if (troff) {
			if (comment_open) {
				comment_open = 0;
				fprintf(output, ".*/\n");
			}
			fprintf(output, ".Pr \"%s\"\n", ps.procname);
		}
		ps.ind_level = 0;
		ps.procname[0] = 0;
	}
	if (s_code == e_code && s_lab == e_lab && s_com == e_com) {
		if (suppress_blanklines > 0)
			suppress_blanklines--;
		else {
			ps.bl_line = true;
			n_real_blanklines++;
		}
	} else
		if (!inhibit_formatting) {
			suppress_blanklines = 0;
			ps.bl_line = false;
			if (prefix_blankline_requested && not_first_line) {
				if (swallow_optional_blanklines) {
					if (n_real_blanklines == 1)
						n_real_blanklines = 0;
				} else {
					if (n_real_blanklines == 0)
						n_real_blanklines = 1;
				}
			}
			while (--n_real_blanklines >= 0)
				putc('\n', output);
			n_real_blanklines = 0;
			if (ps.ind_level == 0)
				ps.ind_stmt = 0;	/* this is a class A
							 * kludge. dont do
							 * additional statement
							 * indentation if we are
							 * at bracket level 0 */

			if (e_lab != s_lab || e_code != s_code)
				++code_lines;	/* keep count of lines with
						 * code */


			if (e_lab != s_lab) {	/* print lab, if any */
				if (comment_open) {
					comment_open = 0;
					fprintf(output, ".*/\n");
				}
				while (e_lab > s_lab && (e_lab[-1] == ' ' || e_lab[-1] == '\t'))
					e_lab--;
				cur_col = pad_output(1, compute_label_target());
				if (s_lab[0] == '#' && (strncmp(s_lab, "#else", 5) == 0
					|| strncmp(s_lab, "#endif", 6) == 0)) {
					char   *s = s_lab;
					if (e_lab[-1] == '\n')
						e_lab--;
					do
						putc(*s++, output);
					while (s < e_lab && 'a' <= *s && *s <= 'z');
					while ((*s == ' ' || *s == '\t') && s < e_lab)
						s++;
					if (s < e_lab)
						fprintf(output, s[0] == '/' && s[1] == '*' ? "\t%.*s" : "\t/* %.*s */",
						    (int)(e_lab - s), s);
				} else
					fprintf(output, "%.*s", (int)(e_lab - s_lab), s_lab);
				cur_col = count_spaces(cur_col, s_lab);
			} else
				cur_col = 1;	/* there is no label section */

			ps.pcase = false;

			if (s_code != e_code) {	/* print code section, if any */
				char   *p;

				if (comment_open) {
					comment_open = 0;
					fprintf(output, ".*/\n");
				}
				target_col = compute_code_target();
				{
					int     i;

					for (i = 0; i < ps.p_l_follow; i++)
						if (ps.paren_indents[i] >= 0)
							ps.paren_indents[i] = -(ps.paren_indents[i] + target_col);
				}
				cur_col = pad_output(cur_col, target_col);
				for (p = s_code; p < e_code; p++)
					if (*p == (char) 0200)
						fprintf(output, "%d", target_col * 7);
					else
						putc(*p, output);
				cur_col = count_spaces(cur_col, s_code);
			}
			if (s_com != e_com) {
				if (troff) {
					int     all_here = 0;
					char   *p;

					if (e_com[-1] == '/' && e_com[-2] == '*')
						e_com -= 2, all_here++;
					while (e_com > s_com && e_com[-1] == ' ')
						e_com--;
					*e_com = 0;
					p = s_com;
					while (*p == ' ')
						p++;
					if (p[0] == '/' && p[1] == '*')
						p += 2, all_here++;
					else
						if (p[0] == '*')
							p += p[1] == '/' ? 2 : 1;
					while (*p == ' ')
						p++;
					if (*p == 0)
						goto inhibit_newline;
					if (comment_open < 2 && ps.box_com) {
						comment_open = 0;
						fprintf(output, ".*/\n");
					}
					if (comment_open == 0) {
						if ('a' <= *p && *p <= 'z')
							*p = *p + 'A' - 'a';
						if (e_com - p < 50 && all_here == 2) {
							char   *follow = p;
							fprintf(output, "\n.nr C! \\w\1");
							while (follow < e_com) {
								switch (*follow) {
								case '\n':
									putc(' ', output);
								case 1:
									break;
								case '\\':
									putc('\\', output);
								default:
									putc(*follow, output);
								}
								follow++;
							}
							putc(1, output);
						}
						fprintf(output, "\n./* %dp %d %dp\n",
						    ps.com_col * 7,
						    (s_code != e_code || s_lab != e_lab) - ps.box_com,
						    target_col * 7);
					}
					comment_open = 1 + ps.box_com;
					while (*p) {
						if (*p == BACKSLASH)
							putc(BACKSLASH, output);
						putc(*p++, output);
					}
				} else {	/* print comment, if any */
					int     target = ps.com_col;
					char   *com_st = s_com;

					target += ps.comment_delta;
					while (*com_st == '\t')
						com_st++, target += 8;	/* ? */
					while (target <= 0)
						if (*com_st == ' ')
							target++, com_st++;
						else
							if (*com_st == '\t')
								target = ((target - 1) & ~7) + 9, com_st++;
							else
								target = 1;
					if (cur_col > target) {	/* if comment cant fit
								 * on this line, put it
								 * on next line */
						putc('\n', output);
						cur_col = 1;
						++ps.out_lines;
					}
					while (e_com > com_st
					&& isspace((unsigned char)e_com[-1]))
						e_com--;
					cur_col = pad_output(cur_col, target);
					if (!ps.box_com) {
						if (star_comment_cont
						&& (com_st[1] != '*'
						    || e_com <= com_st + 1)) {
							if (com_st[1] == ' '
							&&  com_st[0] == ' '
							&&  e_com > com_st + 1)
								com_st[1] = '*';
							else
								fwrite(" * ",
								com_st[0] == '\t'
								? 2
								: com_st[0]=='*'
								? 1
								: 3, 1, output);
						}
					}
					fwrite(com_st,
					    e_com - com_st, 1, output);
					ps.comment_delta = ps.n_comment_delta;
					cur_col = count_spaces(cur_col, com_st);
					++ps.com_lines;	/* count lines with
							 * comments */
				}
			}
			if (ps.use_ff)
				putc('\014', output);
			else
				putc('\n', output);
	inhibit_newline:
			++ps.out_lines;
			if (ps.just_saw_decl == 1 && blanklines_after_declarations) {
				prefix_blankline_requested = 1;
				ps.just_saw_decl = 0;
			} else
				prefix_blankline_requested = postfix_blankline_requested;
			postfix_blankline_requested = 0;
		}
	ps.decl_on_line = ps.in_decl;	/* if we are in the middle of a
					 * declaration, remember that fact for
					 * proper comment indentation */
	ps.ind_stmt = ps.in_stmt & ~ps.in_decl;	/* next line should be
						 * indented if we have not
						 * completed this stmt and if
						 * we are not in the middle of
						 * a declaration */
	ps.use_ff = false;
	ps.dumped_decl_indent = 0;
	*(e_lab = s_lab) = '\0';/* reset buffers */
	*(e_code = s_code) = '\0';
	*(e_com = s_com) = '\0';
	ps.ind_level = ps.i_l_follow;
	ps.paren_level = ps.p_l_follow;
	paren_target = -ps.paren_indents[ps.paren_level - 1];
	not_first_line = 1;
}

int
compute_code_target(void)
{
	int     target_col = ps.ind_size * ps.ind_level + 1;

	if (ps.paren_level) {
		if (!lineup_to_parens)
			target_col += continuation_indent * ps.paren_level;
		else {
			int     w;
			int     t = paren_target;

			if ((w = count_spaces(t, s_code) - max_col) > 0
			    && count_spaces(target_col, s_code) <= max_col) {
				t -= w + 1;
				if (t > target_col)
					target_col = t;
			} else
				target_col = t;
		}
	} else
		if (ps.ind_stmt)
			target_col += continuation_indent;
	return target_col;
}

int
compute_label_target(void)
{
	return
	ps.pcase ? (int) (case_ind * ps.ind_size) + 1
	: *s_lab == '#' ? 1
	: ps.ind_size * (ps.ind_level - label_offset) + 1;
}


/*
 * Copyright (C) 1976 by the Board of Trustees of the University of Illinois
 *
 * All rights reserved
 *
 *
 * NAME: fill_buffer
 *
 * FUNCTION: Reads one block of input into input_buffer
 *
 * HISTORY: initial coding 	November 1976	D A Willcox of CAC 1/7/77 A
 * Willcox of CAC	Added check for switch back to partly full input
 * buffer from temporary buffer
 *
 */
void
fill_buffer(void)
{				/* this routine reads stuff from the input */
	char   *p;
	int     i;
	FILE   *f = input;
	char   *n;

	if (bp_save != 0) {	/* there is a partly filled input buffer left */
		buf_ptr = bp_save;	/* dont read anything, just switch
					 * buffers */
		buf_end = be_save;
		bp_save = be_save = 0;
		if (buf_ptr < buf_end)
			return;	/* only return if there is really something in
				 * this buffer */
	}
	for (p = in_buffer;;) {
		if (p >= in_buffer_limit) {
			int     size = (in_buffer_limit - in_buffer) * 2 + 10;
			int     offset = p - in_buffer;
			n = (char *) realloc(in_buffer, size);
			if (n == 0)
				errx(1, "input line too long");
			in_buffer = n;
			p = in_buffer + offset;
			in_buffer_limit = in_buffer + size - 2;
		}
		if ((i = getc(f)) == EOF) {
			*p++ = ' ';
			*p++ = '\n';
			had_eof = true;
			break;
		}
		*p++ = i;
		if (i == '\n')
			break;
	}
	buf_ptr = in_buffer;
	buf_end = p;
	if (p[-2] == '/' && p[-3] == '*') {
		if (in_buffer[3] == 'I' && strncmp(in_buffer, "/**INDENT**", 11) == 0)
			fill_buffer();	/* flush indent error message */
		else {
			int     com = 0;

			p = in_buffer;
			while (*p == ' ' || *p == '\t')
				p++;
			if (*p == '/' && p[1] == '*') {
				p += 2;
				while (*p == ' ' || *p == '\t')
					p++;
				if (p[0] == 'I' && p[1] == 'N' && p[2] == 'D' && p[3] == 'E'
				    && p[4] == 'N' && p[5] == 'T') {
					p += 6;
					while (*p == ' ' || *p == '\t')
						p++;
					if (*p == '*')
						com = 1;
					else {
						if (*p == 'O') {
							if (*++p == 'N')
								p++, com = 1;
							else
								if (*p == 'F' && *++p == 'F')
									p++, com = 2;
						}
					}
					while (*p == ' ' || *p == '\t')
						p++;
					if (p[0] == '*' && p[1] == '/' && p[2] == '\n' && com) {
						if (s_com != e_com || s_lab != e_lab || s_code != e_code)
							dump_line();
						if (!(inhibit_formatting = com - 1)) {
							n_real_blanklines = 0;
							postfix_blankline_requested = 0;
							prefix_blankline_requested = 0;
							suppress_blanklines = 1;
						}
					}
				}
			}
		}
	}
	if (inhibit_formatting) {
		p = in_buffer;
		do
			putc(*p, output);
		while (*p++ != '\n');
	}
}
/*
 * Copyright (C) 1976 by the Board of Trustees of the University of Illinois
 *
 * All rights reserved
 *
 *
 * NAME: pad_output
 *
 * FUNCTION: Writes tabs and spaces to move the current column up to the desired
 * position.
 *
 * ALGORITHM: Put tabs and/or blanks into pobuf, then write pobuf.
 *
 * PARAMETERS: current		integer		The current column target
 * 	       target		integer		The desired column
 *
 * RETURNS: Integer value of the new column.  (If current >= target, no action is
 * taken, and current is returned.
 *
 * GLOBALS: None
 *
 * CALLS: write (sys)
 *
 * CALLED BY: dump_line
 *
 * HISTORY: initial coding 	November 1976	D A Willcox of CAC
 *
 */
int
pad_output(int current, int target)
{
	int     curr;		/* internal column pointer */
	int     tcur;

	if (troff)
		fprintf(output, "\\h'|%dp'", (target - 1) * 7);
	else {
		if (current >= target)
			return (current);	/* line is already long enough */
		curr = current;
		if (use_tabs) {
			while ((tcur = ((curr - 1) & tabmask) + tabsize + 1) <= target) {
				putc('\t', output);
				curr = tcur;
			}
		}
		while (curr++ < target)
			putc(' ', output);	/* pad with final blanks */
	}
	return (target);
}
/*
 * Copyright (C) 1976 by the Board of Trustees of the University of Illinois
 *
 * All rights reserved
 *
 *
 * NAME: count_spaces
 *
 * FUNCTION: Find out where printing of a given string will leave the current
 * character position on output.
 *
 * ALGORITHM: Run thru input string and add appropriate values to current
 * position.
 *
 * RETURNS: Integer value of position after printing "buffer" starting in column
 * "current".
 *
 * HISTORY: initial coding 	November 1976	D A Willcox of CAC
 *
 */
int
count_spaces(int current, char *buffer)
/*
 * this routine figures out where the character position will be after
 * printing the text in buffer starting at column "current"
 */
{
	char   *buf;		/* used to look thru buffer */
	int     cur;		/* current character counter */

	cur = current;

	for (buf = buffer; *buf != '\0'; ++buf) {
		switch (*buf) {

		case '\n':
		case 014:	/* form feed */
			cur = 1;
			break;

		case '\t':
			cur = ((cur - 1) & tabmask) + tabsize + 1;
			break;

		case 010:	/* backspace */
			--cur;
			break;

		default:
			++cur;
			break;
		}		/* end of switch */
	}			/* end of for loop */
	return (cur);
}


int     found_err;

void
diag(int level, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);

	if (level)
		found_err = 1;
	if (output == stdout) {
		fprintf(stdout, "/**INDENT** %s@%d: ", level == 0 ? "Warning" : "Error", line_no);
		vfprintf(stdout, msg, ap);
		fprintf(stdout, " */\n");
	} else {
		fprintf(stderr, "%s@%d: ", level == 0 ? "Warning" : "Error", line_no);
		vfprintf(stdout, msg, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
}

void
writefdef(struct fstate *f, int nm)
{
	fprintf(output, ".ds f%c %s\n.nr s%c %d\n",
	    nm, f->font, nm, f->size);
}

char   *
chfont(struct fstate *of, struct fstate *nf, char *s)
{
	if (of->font[0] != nf->font[0]
	    || of->font[1] != nf->font[1]) {
		*s++ = '\\';
		*s++ = 'f';
		if (nf->font[1]) {
			*s++ = '(';
			*s++ = nf->font[0];
			*s++ = nf->font[1];
		} else
			*s++ = nf->font[0];
	}
	if (nf->size != of->size) {
		*s++ = '\\';
		*s++ = 's';
		if (nf->size < of->size) {
			*s++ = '-';
			*s++ = '0' + of->size - nf->size;
		} else {
			*s++ = '+';
			*s++ = '0' + nf->size - of->size;
		}
	}
	return s;
}


void
parsefont(struct fstate *f, const char *s0)
{
	const char *s = s0;
	int     sizedelta = 0;
	memset(f, 0, sizeof *f);
	while (*s) {
		if (isdigit((unsigned char)*s))
			f->size = f->size * 10 + *s - '0';
		else
			if (isupper((unsigned char)*s)) {
				if (f->font[0])
					f->font[1] = *s;
				else
					f->font[0] = *s;
			} else
				if (*s == 'c')
					f->allcaps = 1;
				else
					if (*s == '+')
						sizedelta++;
					else
						if (*s == '-')
							sizedelta--;
						else {
							fprintf(stderr, "indent: bad font specification: %s\n", s0);
							exit(1);
						}
		s++;
	}
	if (f->font[0] == 0)
		f->font[0] = 'R';
	if (bodyf.size == 0)
		bodyf.size = 11;
	if (f->size == 0)
		f->size = bodyf.size + sizedelta;
	else
		if (sizedelta > 0)
			f->size += bodyf.size;
		else
			f->size = bodyf.size - f->size;
}
