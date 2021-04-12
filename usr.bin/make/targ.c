/*	$NetBSD: targ.c,v 1.166 2021/02/22 20:38:55 rillig Exp $	*/

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
 */

/*
 * Maintaining the targets and sources, which are both implemented as GNode.
 *
 * Interface:
 *	Targ_Init	Initialize the module.
 *
 *	Targ_End	Clean up the module.
 *
 *	Targ_List	Return the list of all targets so far.
 *
 *	GNode_New	Create a new GNode for the passed target
 *			(string). The node is *not* placed in the
 *			hash table, though all its fields are
 *			initialized.
 *
 *	Targ_FindNode	Find the node, or return NULL.
 *
 *	Targ_GetNode	Find the node, or create it.
 *
 *	Targ_NewInternalNode
 *			Create an internal node.
 *
 *	Targ_FindList	Given a list of names, find nodes for all
 *			of them, creating them as necessary.
 *
 *	Targ_Precious	Return TRUE if the target is precious and
 *			should not be removed if we are interrupted.
 *
 *	Targ_Propagate	Propagate information between related nodes.
 *			Should be called after the makefiles are parsed
 *			but before any action is taken.
 *
 * Debugging:
 *	Targ_PrintGraph
 *			Print out the entire graph, all variables and
 *			statistics for the directory cache. Should print
 *			something for suffixes, too, but...
 */

#include <time.h>

#include "make.h"
#include "dir.h"

/*	"@(#)targ.c	8.2 (Berkeley) 3/19/94"	*/
MAKE_RCSID("$NetBSD: targ.c,v 1.166 2021/02/22 20:38:55 rillig Exp $");

/*
 * All target nodes that appeared on the left-hand side of one of the
 * dependency operators ':', '::', '!'.
 */
static GNodeList allTargets = LST_INIT;
static HashTable allTargetsByName;

#ifdef CLEANUP
static GNodeList allNodes = LST_INIT;

static void GNode_Free(void *);
#endif

void
Targ_Init(void)
{
	HashTable_Init(&allTargetsByName);
}

void
Targ_End(void)
{
	Targ_Stats();
#ifdef CLEANUP
	Lst_Done(&allTargets);
	HashTable_Done(&allTargetsByName);
	Lst_DoneCall(&allNodes, GNode_Free);
#endif
}

void
Targ_Stats(void)
{
	HashTable_DebugStats(&allTargetsByName, "targets");
}

/*
 * Return the list of all targets, which are all nodes that appear on the
 * left-hand side of a dependency declaration such as "target: source".
 * The returned list does not contain pure sources.
 */
GNodeList *
Targ_List(void)
{
	return &allTargets;
}

/*
 * Create a new graph node, but don't register it anywhere.
 *
 * Graph nodes that appear on the left-hand side of a dependency line such
 * as "target: source" are called targets.  XXX: In some cases (like the
 * .ALLTARGETS variable), all nodes are called targets as well, even if they
 * never appear on the left-hand side.  This is a mistake.
 *
 * Typical names for graph nodes are:
 *	"src.c" (an ordinary file)
 *	"clean" (a .PHONY target)
 *	".END" (a special hook target)
 *	"-lm" (a library)
 *	"libc.a(isspace.o)" (an archive member)
 */
GNode *
GNode_New(const char *name)
{
	GNode *gn;

	gn = bmake_malloc(sizeof *gn);
	gn->name = bmake_strdup(name);
	gn->uname = NULL;
	gn->path = NULL;
	gn->type = name[0] == '-' && name[1] == 'l' ? OP_LIB : OP_NONE;
	gn->flags = GNF_NONE;
	gn->made = UNMADE;
	gn->unmade = 0;
	gn->mtime = 0;
	gn->youngestChild = NULL;
	Lst_Init(&gn->implicitParents);
	Lst_Init(&gn->parents);
	Lst_Init(&gn->children);
	Lst_Init(&gn->order_pred);
	Lst_Init(&gn->order_succ);
	Lst_Init(&gn->cohorts);
	gn->cohort_num[0] = '\0';
	gn->unmade_cohorts = 0;
	gn->centurion = NULL;
	gn->checked_seqno = 0;
	HashTable_Init(&gn->vars);
	Lst_Init(&gn->commands);
	gn->suffix = NULL;
	gn->fname = NULL;
	gn->lineno = 0;

#ifdef CLEANUP
	Lst_Append(&allNodes, gn);
#endif

	return gn;
}

#ifdef CLEANUP
static void
GNode_Free(void *gnp)
{
	GNode *gn = gnp;

	free(gn->name);
	free(gn->uname);
	free(gn->path);

	/* Don't free gn->youngestChild since it is not owned by this node. */

	/*
	 * In the following lists, only free the list nodes, but not the
	 * GNodes in them since these are not owned by this node.
	 */
	Lst_Done(&gn->implicitParents);
	Lst_Done(&gn->parents);
	Lst_Done(&gn->children);
	Lst_Done(&gn->order_pred);
	Lst_Done(&gn->order_succ);
	Lst_Done(&gn->cohorts);

	/*
	 * Do not free the variables themselves, even though they are owned
	 * by this node.
	 *
	 * XXX: For the nodes that represent targets or sources (and not
	 * SCOPE_GLOBAL), it should be safe to free the variables as well,
	 * since each node manages the memory for all its variables itself.
	 *
	 * XXX: The GNodes that are only used as variable scopes (SCOPE_CMD,
	 * SCOPE_GLOBAL, SCOPE_INTERNAL) are not freed at all (see Var_End,
	 * where they are not mentioned).  These might be freed at all, if
	 * their variable values are indeed not used anywhere else (see
	 * Trace_Init for the only suspicious use).
	 */
	HashTable_Done(&gn->vars);

	/*
	 * Do not free the commands themselves, as they may be shared with
	 * other nodes.
	 */
	Lst_Done(&gn->commands);

	/*
	 * gn->suffix is not owned by this node.
	 *
	 * XXX: gn->suffix should be unreferenced here.  This requires a
	 * thorough check that the reference counting is done correctly in
	 * all places, otherwise a suffix might be freed too early.
	 */

	free(gn);
}
#endif

/* Get the existing global node, or return NULL. */
GNode *
Targ_FindNode(const char *name)
{
	return HashTable_FindValue(&allTargetsByName, name);
}

/* Get the existing global node, or create it. */
GNode *
Targ_GetNode(const char *name)
{
	Boolean isNew;
	HashEntry *he = HashTable_CreateEntry(&allTargetsByName, name, &isNew);
	if (!isNew)
		return HashEntry_Get(he);

	{
		GNode *gn = Targ_NewInternalNode(name);
		HashEntry_Set(he, gn);
		return gn;
	}
}

/*
 * Create a node, register it in .ALLTARGETS but don't store it in the
 * table of global nodes.  This means it cannot be found by name.
 *
 * This is used for internal nodes, such as cohorts or .WAIT nodes.
 */
GNode *
Targ_NewInternalNode(const char *name)
{
	GNode *gn = GNode_New(name);
	Global_Append(".ALLTARGETS", name);
	Lst_Append(&allTargets, gn);
	DEBUG1(TARG, "Adding \"%s\" to all targets.\n", gn->name);
	if (doing_depend)
		gn->flags |= FROM_DEPEND;
	return gn;
}

/*
 * Return the .END node, which contains the commands to be run when
 * everything else has been made.
 */
GNode *
Targ_GetEndNode(void)
{
	/*
	 * Save the node locally to avoid having to search for it all
	 * the time.
	 */
	static GNode *endNode = NULL;

	if (endNode == NULL) {
		endNode = Targ_GetNode(".END");
		endNode->type = OP_SPECIAL;
	}
	return endNode;
}

/* Add the named nodes to the list, creating them as necessary. */
void
Targ_FindList(GNodeList *gns, StringList *names)
{
	StringListNode *ln;

	for (ln = names->first; ln != NULL; ln = ln->next) {
		const char *name = ln->datum;
		GNode *gn = Targ_GetNode(name);
		Lst_Append(gns, gn);
	}
}

/* See if the given target is precious. */
Boolean
Targ_Precious(const GNode *gn)
{
	/* XXX: Why are '::' targets precious? */
	return allPrecious || gn->type & (OP_PRECIOUS | OP_DOUBLEDEP);
}

/*
 * The main target to be made; only for debugging output.
 * See mainNode in parse.c for the definitive source.
 */
static GNode *mainTarg;

/* Remember the main target to make; only used for debugging. */
void
Targ_SetMain(GNode *gn)
{
	mainTarg = gn;
}

static void
PrintNodeNames(GNodeList *gnodes)
{
	GNodeListNode *ln;

	for (ln = gnodes->first; ln != NULL; ln = ln->next) {
		GNode *gn = ln->datum;
		debug_printf(" %s%s", gn->name, gn->cohort_num);
	}
}

static void
PrintNodeNamesLine(const char *label, GNodeList *gnodes)
{
	if (Lst_IsEmpty(gnodes))
		return;
	debug_printf("# %s:", label);
	PrintNodeNames(gnodes);
	debug_printf("\n");
}

void
Targ_PrintCmds(GNode *gn)
{
	StringListNode *ln;

	for (ln = gn->commands.first; ln != NULL; ln = ln->next) {
		const char *cmd = ln->datum;
		debug_printf("\t%s\n", cmd);
	}
}

/*
 * Format a modification time in some reasonable way and return it.
 * The formatted time is placed in a static area, so it is overwritten
 * with each call.
 */
const char *
Targ_FmtTime(time_t tm)
{
	static char buf[128];

	struct tm *parts = localtime(&tm);
	(void)strftime(buf, sizeof buf, "%k:%M:%S %b %d, %Y", parts);
	return buf;
}

/* Print out a type field giving only those attributes the user can set. */
void
Targ_PrintType(int type)
{
	int tbit;

	type &= ~OP_OPMASK;

	while (type != 0) {
		tbit = 1 << (ffs(type) - 1);
		type &= ~tbit;

		switch (tbit) {
#define PRINTBIT(bit, attr) case bit: debug_printf(" " attr); break
#define PRINTDBIT(bit, attr) case bit: DEBUG0(TARG, " " attr); break
		PRINTBIT(OP_OPTIONAL, ".OPTIONAL");
		PRINTBIT(OP_USE, ".USE");
		PRINTBIT(OP_EXEC, ".EXEC");
		PRINTBIT(OP_IGNORE, ".IGNORE");
		PRINTBIT(OP_PRECIOUS, ".PRECIOUS");
		PRINTBIT(OP_SILENT, ".SILENT");
		PRINTBIT(OP_MAKE, ".MAKE");
		PRINTBIT(OP_JOIN, ".JOIN");
		PRINTBIT(OP_INVISIBLE, ".INVISIBLE");
		PRINTBIT(OP_NOTMAIN, ".NOTMAIN");
		PRINTDBIT(OP_LIB, ".LIB");
		PRINTDBIT(OP_MEMBER, ".MEMBER");
		PRINTDBIT(OP_ARCHV, ".ARCHV");
		PRINTDBIT(OP_MADE, ".MADE");
		PRINTDBIT(OP_PHONY, ".PHONY");
#undef PRINTBIT
#undef PRINTDBIT
		}
	}
}

const char *
GNodeMade_Name(GNodeMade made)
{
	switch (made) {
	case UNMADE:    return "unmade";
	case DEFERRED:  return "deferred";
	case REQUESTED: return "requested";
	case BEINGMADE: return "being made";
	case MADE:      return "made";
	case UPTODATE:  return "up-to-date";
	case ERROR:     return "error when made";
	case ABORTED:   return "aborted";
	default:        return "unknown enum_made value";
	}
}

static const char *
GNode_OpName(const GNode *gn)
{
	switch (gn->type & OP_OPMASK) {
	case OP_DEPENDS:
		return ":";
	case OP_FORCE:
		return "!";
	case OP_DOUBLEDEP:
		return "::";
	}
	return "";
}

/* Print the contents of a node. */
void
Targ_PrintNode(GNode *gn, int pass)
{
	debug_printf("# %s%s", gn->name, gn->cohort_num);
	GNode_FprintDetails(opts.debug_file, ", ", gn, "\n");
	if (gn->flags == 0)
		return;

	if (!GNode_IsTarget(gn))
		return;

	debug_printf("#\n");
	if (gn == mainTarg)
		debug_printf("# *** MAIN TARGET ***\n");

	if (pass >= 2) {
		if (gn->unmade > 0)
			debug_printf("# %d unmade children\n", gn->unmade);
		else
			debug_printf("# No unmade children\n");
		if (!(gn->type & (OP_JOIN | OP_USE | OP_USEBEFORE | OP_EXEC))) {
			if (gn->mtime != 0) {
				debug_printf("# last modified %s: %s\n",
				    Targ_FmtTime(gn->mtime),
				    GNodeMade_Name(gn->made));
			} else if (gn->made != UNMADE) {
				debug_printf("# nonexistent (maybe): %s\n",
				    GNodeMade_Name(gn->made));
			} else
				debug_printf("# unmade\n");
		}
		PrintNodeNamesLine("implicit parents", &gn->implicitParents);
	} else {
		if (gn->unmade != 0)
			debug_printf("# %d unmade children\n", gn->unmade);
	}

	PrintNodeNamesLine("parents", &gn->parents);
	PrintNodeNamesLine("order_pred", &gn->order_pred);
	PrintNodeNamesLine("order_succ", &gn->order_succ);

	debug_printf("%-16s%s", gn->name, GNode_OpName(gn));
	Targ_PrintType(gn->type);
	PrintNodeNames(&gn->children);
	debug_printf("\n");
	Targ_PrintCmds(gn);
	debug_printf("\n\n");
	if (gn->type & OP_DOUBLEDEP)
		Targ_PrintNodes(&gn->cohorts, pass);
}

void
Targ_PrintNodes(GNodeList *gnodes, int pass)
{
	GNodeListNode *ln;

	for (ln = gnodes->first; ln != NULL; ln = ln->next)
		Targ_PrintNode(ln->datum, pass);
}

/* Print only those targets that are just a source. */
static void
PrintOnlySources(void)
{
	GNodeListNode *ln;

	for (ln = allTargets.first; ln != NULL; ln = ln->next) {
		GNode *gn = ln->datum;
		if (GNode_IsTarget(gn))
			continue;

		debug_printf("#\t%s [%s]", gn->name, GNode_Path(gn));
		Targ_PrintType(gn->type);
		debug_printf("\n");
	}
}

/*
 * Input:
 *	pass		1 => before processing
 *			2 => after processing
 *			3 => after processing, an error occurred
 */
void
Targ_PrintGraph(int pass)
{
	debug_printf("#*** Input graph:\n");
	Targ_PrintNodes(&allTargets, pass);
	debug_printf("\n");
	debug_printf("\n");

	debug_printf("#\n");
	debug_printf("#   Files that are only sources:\n");
	PrintOnlySources();

	debug_printf("#*** Global Variables:\n");
	Var_Dump(SCOPE_GLOBAL);

	debug_printf("#*** Command-line Variables:\n");
	Var_Dump(SCOPE_CMDLINE);

	debug_printf("\n");
	Dir_PrintDirectories();
	debug_printf("\n");

	Suff_PrintAll();
}

/*
 * Propagate some type information to cohort nodes (those from the '::'
 * dependency operator).
 *
 * Should be called after the makefiles are parsed but before any action is
 * taken.
 */
void
Targ_Propagate(void)
{
	GNodeListNode *ln, *cln;

	for (ln = allTargets.first; ln != NULL; ln = ln->next) {
		GNode *gn = ln->datum;
		GNodeType type = gn->type;

		if (!(type & OP_DOUBLEDEP))
			continue;

		for (cln = gn->cohorts.first; cln != NULL; cln = cln->next) {
			GNode *cohort = cln->datum;

			cohort->type |= type & ~OP_OPMASK;
		}
	}
}
