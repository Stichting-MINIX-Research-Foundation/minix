/*	$NetBSD: indent_globs.h,v 1.10 2014/09/04 04:06:07 mrg Exp $	*/

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
 *
 *	from: @(#)indent_globs.h	8.1 (Berkeley) 6/6/93
 */

/*
 * Copyright (c) 1985 Sun Microsystems, Inc.
 * Copyright (c) 1976 Board of Trustees of the University of Illinois.
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
 *
 *	from: @(#)indent_globs.h	8.1 (Berkeley) 6/6/93
 */

#define BACKSLASH '\\'
#define bufsize 200		/* size of internal buffers */
#define sc_size 5000		/* size of save_com buffer */
#define label_offset 2		/* number of levels a label is placed to left
				 * of code */

#define tabsize 8		/* the size of a tab */
#define tabmask 0177770		/* mask used when figuring length of lines
				 * with tabs */


#define false 0
#define true  1


#ifndef EXTERN
#define EXTERN extern
#endif


EXTERN FILE   *input;			/* the fid for the input file */
EXTERN FILE   *output;			/* the output file */

#define CHECK_SIZE_CODE \
	if (e_code >= l_code) { \
	    int nsize = l_code-s_code+400; \
	    codebuf = (char *) realloc(codebuf, nsize); \
	    e_code = codebuf + (e_code-s_code) + 1; \
	    l_code = codebuf + nsize - 5; \
	    s_code = codebuf + 1; \
	}
#define CHECK_SIZE_COM \
	if (e_com >= l_com) { \
	    int nsize = l_com-s_com+400; \
	    combuf = (char *) realloc(combuf, nsize); \
	    e_com = combuf + (e_com-s_com) + 1; \
	    l_com = combuf + nsize - 5; \
	    s_com = combuf + 1; \
	}
#define CHECK_SIZE_LAB \
	if (e_lab >= l_lab) { \
	    int nsize = l_lab-s_lab+400; \
	    labbuf = (char *) realloc(labbuf, nsize); \
	    e_lab = labbuf + (e_lab-s_lab) + 1; \
	    l_lab = labbuf + nsize - 5; \
	    s_lab = labbuf + 1; \
	}
#define CHECK_SIZE_TOKEN \
	if (e_token >= l_token) { \
	    int nsize = l_token-s_token+400; \
	    tokenbuf = (char *) realloc(tokenbuf, nsize); \
	    e_token = tokenbuf + (e_token-s_token) + 1; \
	    l_token = tokenbuf + nsize - 5; \
	    s_token = tokenbuf + 1; \
	}

EXTERN char   *labbuf;			/* buffer for label */
EXTERN char   *s_lab;			/* start ... */
EXTERN char   *e_lab;			/* .. and end of stored label */
EXTERN char   *l_lab;			/* limit of label buffer */

EXTERN char   *codebuf;			/* buffer for code section */
EXTERN char   *s_code;			/* start ... */
EXTERN char   *e_code;			/* .. and end of stored code */
EXTERN char   *l_code;			/* limit of code section */

EXTERN char   *combuf;			/* buffer for comments */
EXTERN char   *s_com;			/* start ... */
EXTERN char   *e_com;			/* ... and end of stored comments */
EXTERN char   *l_com;			/* limit of comment buffer */

#define token s_token
EXTERN char   *tokenbuf;		/* the last token scanned */
EXTERN char   *s_token;
EXTERN char   *e_token;
EXTERN char   *l_token;

EXTERN char   *in_buffer;		/* input buffer */
EXTERN char   *in_buffer_limit;		/* the end of the input buffer */
EXTERN char   *buf_ptr;			/* ptr to next character to be taken from
					 * in_buffer */
EXTERN char   *buf_end;			/* ptr to first after last char in in_buffer */

EXTERN char    save_com[sc_size];	/* input text is saved here when looking for
				 	 * the brace after an if, while, etc */
EXTERN char   *sc_end;			/* pointer into save_com buffer */

EXTERN char   *bp_save;			/* saved value of buf_ptr when taking input
					 * from save_com */
EXTERN char   *be_save;			/* similarly saved value of buf_end */


EXTERN int     pointer_as_binop;
EXTERN int     blanklines_after_declarations;
EXTERN int     blanklines_before_blockcomments;
EXTERN int     blanklines_after_procs;
EXTERN int     blanklines_around_conditional_compilation;
EXTERN int     swallow_optional_blanklines;
EXTERN int     n_real_blanklines;
EXTERN int     prefix_blankline_requested;
EXTERN int     postfix_blankline_requested;
EXTERN int     break_comma;		/* when true and not in parens, break after a
					 * comma */
EXTERN int     btype_2;			/* when true, brace should be on same line as
					 * if, while, etc */
EXTERN float   case_ind;		/* indentation level to be used for a "case
					 * n:" */
EXTERN int     code_lines;		/* count of lines with code */
EXTERN int     had_eof;			/* set to true when input is exhausted */
EXTERN int     line_no;			/* the current line number. */
EXTERN int     max_col;			/* the maximum allowable line length */
EXTERN int     verbose;			/* when true, non-essential error messages are
					 * printed */
EXTERN int     cuddle_else;		/* true if else should cuddle up to '}' */
EXTERN int     star_comment_cont;	/* true iff comment continuation lines should
					 * have stars at the beginning of each line. */
EXTERN int     comment_delimiter_on_blankline;
EXTERN int     troff;			/* true iff were generating troff input */
EXTERN int     procnames_start_line;	/* if true, the names of procedures being
					 * defined get placed in column 1 (ie. a
					 * newline is placed between the type of the
					 * procedure and its name) */
EXTERN int     proc_calls_space;	/* If true, procedure calls look like:
					 * foo(bar) rather than foo (bar) */
EXTERN int     format_col1_comments;	/* If comments which start in column 1 are to
					 * be magically reformatted (just like
					 * comments that begin in later columns) */
EXTERN int     inhibit_formatting;	/* true if INDENT OFF is in effect */
EXTERN int     suppress_blanklines;	/* set iff following blanklines should be
					 * suppressed */
EXTERN int     continuation_indent;	/* set to the indentation between the edge of
					 * code and continuation lines */
EXTERN int     lineup_to_parens;	/* if true, continued code within parens will
					 * be lined up to the open paren */
EXTERN int     Bill_Shannon;		/* true iff a blank should always be inserted
					 * after sizeof */
EXTERN int     blanklines_after_declarations_at_proctop;	/* This is vaguely
								 * similar to
								 * blanklines_after_decla
								 * rations except that
								 * it only applies to
								 * the first set of
								 * declarations in a
								 * procedure (just after
								 * the first '{') and it
								 * causes a blank line
								 * to be generated even
								 * if there are no
								 * declarations */
EXTERN int     block_comment_max_col;
EXTERN int     extra_expression_indent; /* True if continuation lines from the
					 * expression part of "if(e)", "while(e)",
					 * "for(e;e;e)" should be indented an extra
					 * tab stop so that they don't conflict with
					 * the code that follows */
EXTERN int    use_tabs;			/* set true to use tabs for spacing,
					 * false uses all spaces */

/* -troff font state information */

struct fstate {
	char    font[4];
	char    size;
	int     allcaps:1;
};

EXTERN struct fstate
        keywordf,		/* keyword font */
        stringf,		/* string font */
        boxcomf,		/* Box comment font */
        blkcomf,		/* Block comment font */
        scomf,			/* Same line comment font */
        bodyf;			/* major body font */

#define STACK_SIZE 150

EXTERN struct parser_state {
	int     last_token;
	struct fstate cfont;	/* Current font */
        int     p_stack[STACK_SIZE];	/* this is the parsers stack */
        int     il[STACK_SIZE];	/* this stack stores indentation levels */
        float   cstk[STACK_SIZE];/* used to store case stmt indentation levels */
	int     box_com;	/* set to true when we are in a "boxed"
				 * comment. In that case, the first non-blank
				 * char should be lined up with the comment / */
	int     comment_delta, n_comment_delta;
	int     cast_mask;	/* indicates which close parens close off
				 * casts */
	int     sizeof_mask;	/* indicates which close parens close off
				 * sizeof''s */
	int     block_init;	/* true iff inside a block initialization */
	int     block_init_level;	/* The level of brace nesting in an
					 * initialization */
	int     last_nl;	/* this is true if the last thing scanned was
				 * a newline */
	int     in_or_st;	/* Will be true iff there has been a
				 * declarator (e.g. int or char) and no left
				 * paren since the last semicolon. When true,
				 * a '{' is starting a structure definition or
				 * an initialization list */
	int     bl_line;	/* set to 1 by dump_line if the line is blank */
	int     col_1;		/* set to true if the last token started in
				 * column 1 */
	int     com_col;	/* this is the column in which the current
				 * coment should start */
	int     com_ind;	/* the column in which comments to the right
				 * of code should start */
	int     com_lines;	/* the number of lines with comments, set by
				 * dump_line */
	int     dec_nest;	/* current nesting level for structure or init */
	int     decl_com_ind;	/* the column in which comments after
				 * declarations should be put */
	int     decl_on_line;	/* set to true if this line of code has part
				 * of a declaration on it */
	int     i_l_follow;	/* the level to which ind_level should be set
				 * after the current line is printed */
	int     in_decl;	/* set to true when we are in a declaration
				 * stmt.  The processing of braces is then
				 * slightly different */
	int     in_stmt;	/* set to 1 while in a stmt */
	int     ind_level;	/* the current indentation level */
	int     ind_size;	/* the size of one indentation level */
	int     ind_stmt;	/* set to 1 if next line should have an extra
				 * indentation level because we are in the
				 * middle of a stmt */
	int     last_u_d;	/* set to true after scanning a token which
				 * forces a following operator to be unary */
	int     leave_comma;	/* if true, never break declarations after
				 * commas */
	int     ljust_decl;	/* true if declarations should be left
				 * justified */
	int     out_coms;	/* the number of comments processed, set by
				 * pr_comment */
	int     out_lines;	/* the number of lines written, set by
				 * dump_line */
	int     p_l_follow;	/* used to remember how to indent following
				 * statement */
	int     paren_level;	/* parenthesization level. used to indent
				 * within stmts */
	short   paren_indents[20];	/* column positions of each paren */
	int     pcase;		/* set to 1 if the current line label is a
				 * case.  It is printed differently from a
				 * regular label */
	int     search_brace;	/* set to true by parse when it is necessary
				 * to buffer up all info up to the start of a
				 * stmt after an if, while, etc */
	int     unindent_displace;	/* comments not to the right of code
					 * will be placed this many
					 * indentation levels to the left of
					 * code */
	int     use_ff;		/* set to one if the current line should be
				 * terminated with a form feed */
	int     want_blank;	/* set to true when the following token should
				 * be prefixed by a blank. (Said prefixing is
				 * ignored in some cases.) */
	int     else_if;	/* True iff else if pairs should be handled
				 * specially */
	int     decl_indent;	/* column to indent declared identifiers to */
	int     its_a_keyword;
	int     sizeof_keyword;
	int     dumped_decl_indent;
	float   case_indent;	/* The distance to indent case labels from the
				 * switch statement */
	int     in_parameter_declaration;
	int     indent_parameters;
	int     tos;		/* pointer to top of stack */
	char    procname[100];	/* The name of the current procedure */
	int     just_saw_decl;
}       ps;

EXTERN int     ifdef_level;
EXTERN int     rparen_count;
EXTERN struct parser_state state_stack[5];
EXTERN struct parser_state match_state[5];

int compute_code_target(void);
int compute_label_target(void);
int count_spaces(int, char *);
void diag(int, const char *,...) __attribute__((__format__(__printf__, 2, 3)));
void dump_line(void);
int eqin(const char *, const char *);
void fill_buffer(void);
int pad_output(int, int);
void scan_profile(FILE *);
void set_defaults(void);
void set_option(char *);
void addkey(char *, int);
void set_profile(void);
char *chfont(struct fstate *, struct fstate *, char *);
void parsefont(struct fstate *, const char *);
void writefdef(struct fstate *, int);
int lexi(void);
void reduce(void);
void parse(int);
void pr_comment(void);
void bakcopy(void);
