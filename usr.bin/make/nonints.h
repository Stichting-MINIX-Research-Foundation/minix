/*	$NetBSD: nonints.h,v 1.206 2021/03/15 15:39:13 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *	from: @(#)nonints.h	8.3 (Berkeley) 3/19/94
 */

/*
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *	from: @(#)nonints.h	8.3 (Berkeley) 3/19/94
 */

/* arch.c */
void Arch_Init(void);
void Arch_End(void);

Boolean Arch_ParseArchive(char **, GNodeList *, GNode *);
void Arch_Touch(GNode *);
void Arch_TouchLib(GNode *);
void Arch_UpdateMTime(GNode *gn);
void Arch_UpdateMemberMTime(GNode *gn);
void Arch_FindLib(GNode *, SearchPath *);
Boolean Arch_LibOODate(GNode *);
Boolean Arch_IsLib(GNode *);

/* compat.c */
int Compat_RunCommand(const char *, GNode *, StringListNode *);
void Compat_Run(GNodeList *);
void Compat_Make(GNode *, GNode *);

/* cond.c */
CondEvalResult Cond_EvalCondition(const char *, Boolean *);
CondEvalResult Cond_EvalLine(const char *);
void Cond_restore_depth(unsigned int);
unsigned int Cond_save_depth(void);

/* dir.c; see also dir.h */

MAKE_INLINE const char *
str_basename(const char *pathname)
{
	const char *lastSlash = strrchr(pathname, '/');
	return lastSlash != NULL ? lastSlash + 1 : pathname;
}

MAKE_INLINE SearchPath *
SearchPath_New(void)
{
	SearchPath *path = bmake_malloc(sizeof *path);
	Lst_Init(&path->dirs);
	return path;
}

void SearchPath_Free(SearchPath *);

/* for.c */
int For_Eval(const char *);
Boolean For_Accum(const char *);
void For_Run(int);

/* job.c */
void JobReapChild(pid_t, int, Boolean);

/* main.c */
void Main_ParseArgLine(const char *);
char *Cmd_Exec(const char *, const char **);
void Error(const char *, ...) MAKE_ATTR_PRINTFLIKE(1, 2);
void Fatal(const char *, ...) MAKE_ATTR_PRINTFLIKE(1, 2) MAKE_ATTR_DEAD;
void Punt(const char *, ...) MAKE_ATTR_PRINTFLIKE(1, 2) MAKE_ATTR_DEAD;
void DieHorribly(void) MAKE_ATTR_DEAD;
void Finish(int) MAKE_ATTR_DEAD;
int eunlink(const char *);
void execDie(const char *, const char *);
char *getTmpdir(void);
Boolean ParseBoolean(const char *, Boolean);
char *cached_realpath(const char *, char *);

/* parse.c */
void Parse_Init(void);
void Parse_End(void);

typedef enum VarAssignOp {
	VAR_NORMAL,		/* = */
	VAR_SUBST,		/* := */
	VAR_SHELL,		/* != or :sh= */
	VAR_APPEND,		/* += */
	VAR_DEFAULT		/* ?= */
} VarAssignOp;

typedef struct VarAssign {
	char *varname;		/* unexpanded */
	VarAssignOp op;
	const char *value;	/* unexpanded */
} VarAssign;

typedef char *(*ReadMoreProc)(void *, size_t *);

void Parse_Error(ParseErrorLevel, const char *, ...) MAKE_ATTR_PRINTFLIKE(2, 3);
Boolean Parse_IsVar(const char *, VarAssign *out_var);
void Parse_DoVar(VarAssign *, GNode *);
void Parse_AddIncludeDir(const char *);
void Parse_File(const char *, int);
void Parse_SetInput(const char *, int, int, ReadMoreProc, void *);
void Parse_MainName(GNodeList *);
int Parse_GetFatals(void);

/* str.c */

/* A read-only string that may need to be freed after use. */
typedef struct FStr {
	const char *str;
	void *freeIt;
} FStr;

/* A modifiable string that may need to be freed after use. */
typedef struct MFStr {
	char *str;
	void *freeIt;
} MFStr;

typedef struct Words {
	char **words;
	size_t len;
	void *freeIt;
} Words;

/* Return a string that is the sole owner of str. */
MAKE_INLINE FStr
FStr_InitOwn(char *str)
{
	return (FStr){ str, str };
}

/* Return a string that refers to the shared str. */
MAKE_INLINE FStr
FStr_InitRefer(const char *str)
{
	return (FStr){ str, NULL };
}

MAKE_INLINE void
FStr_Done(FStr *fstr)
{
	free(fstr->freeIt);
#ifdef CLEANUP
	fstr->str = NULL;
	fstr->freeIt = NULL;
#endif
}

/* Return a string that is the sole owner of str. */
MAKE_INLINE MFStr
MFStr_InitOwn(char *str)
{
	return (MFStr){ str, str };
}

/* Return a string that refers to the shared str. */
MAKE_INLINE MFStr
MFStr_InitRefer(char *str)
{
	return (MFStr){ str, NULL };
}

MAKE_INLINE void
MFStr_Done(MFStr *mfstr)
{
	free(mfstr->freeIt);
#ifdef CLEANUP
	mfstr->str = NULL;
	mfstr->freeIt = NULL;
#endif
}

Words Str_Words(const char *, Boolean);
MAKE_INLINE void
Words_Free(Words w)
{
	free(w.words);
	free(w.freeIt);
}

char *str_concat2(const char *, const char *);
char *str_concat3(const char *, const char *, const char *);
char *str_concat4(const char *, const char *, const char *, const char *);
Boolean Str_Match(const char *, const char *);

/* suff.c */
void Suff_Init(void);
void Suff_End(void);

void Suff_ClearSuffixes(void);
Boolean Suff_IsTransform(const char *);
GNode *Suff_AddTransform(const char *);
void Suff_EndTransform(GNode *);
void Suff_AddSuffix(const char *, GNode **);
SearchPath *Suff_GetPath(const char *);
void Suff_DoPaths(void);
void Suff_AddInclude(const char *);
void Suff_AddLib(const char *);
void Suff_FindDeps(GNode *);
SearchPath *Suff_FindPath(GNode *);
void Suff_SetNull(const char *);
void Suff_PrintAll(void);

/* targ.c */
void Targ_Init(void);
void Targ_End(void);

void Targ_Stats(void);
GNodeList *Targ_List(void);
GNode *GNode_New(const char *);
GNode *Targ_FindNode(const char *);
GNode *Targ_GetNode(const char *);
GNode *Targ_NewInternalNode(const char *);
GNode *Targ_GetEndNode(void);
void Targ_FindList(GNodeList *, StringList *);
Boolean Targ_Precious(const GNode *);
void Targ_SetMain(GNode *);
void Targ_PrintCmds(GNode *);
void Targ_PrintNode(GNode *, int);
void Targ_PrintNodes(GNodeList *, int);
const char *Targ_FmtTime(time_t);
void Targ_PrintType(int);
void Targ_PrintGraph(int);
void Targ_Propagate(void);
const char *GNodeMade_Name(GNodeMade);

/* var.c */
void Var_Init(void);
void Var_End(void);

typedef struct VarEvalFlags {

	/*
	 * Expand and evaluate variables during parsing.
	 *
	 * Without this flag, the expression is only parsed, without
	 * evaluating any part of it.
	 *
	 * TODO: Document what Var_Parse and Var_Subst return when this flag
	 *  is not set.  As of 2021-03-15, they return unspecified,
	 *  inconsistent results.
	 */
	Boolean wantRes: 1;

	/*
	 * Treat undefined variables as errors.
	 * Must only be used in combination with wantRes.
	 */
	Boolean undefErr: 1;

	/*
	 * Keep '$$' as '$$' instead of reducing it to a single '$'.
	 *
	 * Used in variable assignments using the ':=' operator.  It allows
	 * multiple such assignments to be chained without accidentally
	 * expanding '$$file' to '$file' in the first assignment and
	 * interpreting it as '${f}' followed by 'ile' in the next assignment.
	 */
	Boolean keepDollar: 1;

	/*
	 * Keep undefined variables as-is instead of expanding them to an
	 * empty string.
	 *
	 * Example for a ':=' assignment:
	 *	CFLAGS = $(.INCLUDES)
	 *	CFLAGS := -I.. $(CFLAGS)
	 *	# If .INCLUDES (an undocumented special variable, by the
	 *	# way) is still undefined, the updated CFLAGS becomes
	 *	# "-I.. $(.INCLUDES)".
	 */
	Boolean keepUndef: 1;

	/*
	 * Without this padding, GCC 9.3.0 on NetBSD 9.99.80 generates larger
	 * code than necessary (1.2 kB), masking out the unused bits from the
	 * int (since that is the default representation of Boolean in make),
	 * even for initializers consisting entirely of constants.
	 */
	Boolean : 0;
} VarEvalFlags;

#define VARE_PARSE_ONLY	(VarEvalFlags) { FALSE, FALSE, FALSE, FALSE }
#define VARE_WANTRES	(VarEvalFlags) { TRUE, FALSE, FALSE, FALSE }
#define VARE_UNDEFERR	(VarEvalFlags) { TRUE, TRUE, FALSE, FALSE }
#define VARE_KEEP_DOLLAR_UNDEF (VarEvalFlags) { TRUE, FALSE, TRUE, TRUE }

typedef enum VarSetFlags {
	VAR_SET_NONE		= 0,

	/* do not export */
	VAR_SET_NO_EXPORT	= 1 << 0,

	/* Make the variable read-only. No further modification is possible,
	 * except for another call to Var_Set with the same flag. */
	VAR_SET_READONLY	= 1 << 1
} VarSetFlags;

/* The state of error handling returned by Var_Parse. */
typedef enum VarParseResult {

	/* Both parsing and evaluation succeeded. */
	VPR_OK,

	/* Parsing or evaluating failed, with an error message. */
	VPR_ERR,

	/*
	 * Parsing succeeded, undefined expressions are allowed and the
	 * expression was still undefined after applying all modifiers.
	 * No error message is printed in this case.
	 *
	 * Some callers handle this case differently, so return this
	 * information to them, for now.
	 *
	 * TODO: Instead of having this special return value, rather ensure
	 *  that VarEvalFlags.keepUndef is processed properly.
	 */
	VPR_UNDEF

} VarParseResult;

typedef enum VarExportMode {
	/* .export-env */
	VEM_ENV,
	/* .export: Initial export or update an already exported variable. */
	VEM_PLAIN,
	/* .export-literal: Do not expand the variable value. */
	VEM_LITERAL
} VarExportMode;

void Var_Delete(GNode *, const char *);
void Var_DeleteExpand(GNode *, const char *);
void Var_Undef(const char *);
void Var_Set(GNode *, const char *, const char *);
void Var_SetExpand(GNode *, const char *, const char *);
void Var_SetWithFlags(GNode *, const char *, const char *, VarSetFlags);
void Var_SetExpandWithFlags(GNode *, const char *, const char *, VarSetFlags);
void Var_Append(GNode *, const char *, const char *);
void Var_AppendExpand(GNode *, const char *, const char *);
Boolean Var_Exists(GNode *, const char *);
Boolean Var_ExistsExpand(GNode *, const char *);
FStr Var_Value(GNode *, const char *);
const char *GNode_ValueDirect(GNode *, const char *);
VarParseResult Var_Parse(const char **, GNode *, VarEvalFlags, FStr *);
VarParseResult Var_Subst(const char *, GNode *, VarEvalFlags, char **);
void Var_Stats(void);
void Var_Dump(GNode *);
void Var_ReexportVars(void);
void Var_Export(VarExportMode, const char *);
void Var_ExportVars(const char *);
void Var_UnExport(Boolean, const char *);

void Global_Set(const char *, const char *);
void Global_SetExpand(const char *, const char *);
void Global_Append(const char *, const char *);
void Global_Delete(const char *);

/* util.c */
typedef void (*SignalProc)(int);
SignalProc bmake_signal(int, SignalProc);
