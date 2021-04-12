/*	$NetBSD: arch.c,v 1.198 2021/03/15 12:15:03 rillig Exp $	*/

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
 * Manipulate libraries, archives and their members.
 *
 * The first time an archive is referenced, all of its members' headers are
 * read and cached and the archive closed again.  All cached archives are kept
 * on a list which is searched each time an archive member is referenced.
 *
 * The interface to this module is:
 *
 *	Arch_Init	Initialize this module.
 *
 *	Arch_End	Clean up this module.
 *
 *	Arch_ParseArchive
 *			Parse an archive specification such as
 *			"archive.a(member1 member2)".
 *
 *	Arch_Touch	Alter the modification time of the archive
 *			member described by the given node to be
 *			the time when make was started.
 *
 *	Arch_TouchLib	Update the modification time of the library
 *			described by the given node. This is special
 *			because it also updates the modification time
 *			of the library's table of contents.
 *
 *	Arch_UpdateMTime
 *			Find the modification time of a member of
 *			an archive *in the archive* and place it in the
 *			member's GNode.
 *
 *	Arch_UpdateMemberMTime
 *			Find the modification time of a member of
 *			an archive. Called when the member doesn't
 *			already exist. Looks in the archive for the
 *			modification time. Returns the modification
 *			time.
 *
 *	Arch_FindLib	Search for a library along a path. The
 *			library name in the GNode should be in
 *			-l<name> format.
 *
 *	Arch_LibOODate	Decide if a library node is out-of-date.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>

#include <ar.h>
#include <utime.h>

#include "make.h"
#include "dir.h"
#include "config.h"

/*	"@(#)arch.c	8.2 (Berkeley) 1/2/94"	*/
MAKE_RCSID("$NetBSD: arch.c,v 1.198 2021/03/15 12:15:03 rillig Exp $");

typedef struct List ArchList;
typedef struct ListNode ArchListNode;

static ArchList archives;	/* The archives we've already examined */

typedef struct Arch {
	char *name;		/* Name of archive */
	HashTable members;	/* All the members of the archive described
				 * by <name, struct ar_hdr *> key/value pairs */
	char *fnametab;		/* Extended name table strings */
	size_t fnamesize;	/* Size of the string table */
} Arch;

static FILE *ArchFindMember(const char *, const char *,
			    struct ar_hdr *, const char *);
#if defined(__svr4__) || defined(__SVR4) || defined(__ELF__)
#define SVR4ARCHIVES
static int ArchSVR4Entry(Arch *, char *, size_t, FILE *);
#endif

#ifdef CLEANUP
static void
ArchFree(void *ap)
{
	Arch *a = ap;
	HashIter hi;

	/* Free memory from hash entries */
	HashIter_Init(&hi, &a->members);
	while (HashIter_Next(&hi) != NULL)
		free(hi.entry->value);

	free(a->name);
	free(a->fnametab);
	HashTable_Done(&a->members);
	free(a);
}
#endif


/*
 * Parse an archive specification such as "archive.a(member1 member2.${EXT})",
 * adding nodes for the expanded members to gns.  Nodes are created as
 * necessary.
 *
 * Input:
 *	pp		The start of the specification.
 *	gns		The list on which to place the nodes.
 *	scope		The scope in which to expand variables.
 *
 * Output:
 *	return		TRUE if it was a valid specification.
 *	*pp		Points to the first non-space after the archive spec.
 */
Boolean
Arch_ParseArchive(char **pp, GNodeList *gns, GNode *scope)
{
	char *cp;		/* Pointer into line */
	GNode *gn;		/* New node */
	MFStr libName;		/* Library-part of specification */
	char *memName;		/* Member-part of specification */
	char saveChar;		/* Ending delimiter of member-name */
	Boolean expandLibName;	/* Whether the parsed libName contains
				 * variable expressions that need to be
				 * expanded */

	libName = MFStr_InitRefer(*pp);
	expandLibName = FALSE;

	for (cp = libName.str; *cp != '(' && *cp != '\0';) {
		if (*cp == '$') {
			/* Expand nested variable expressions. */
			/* XXX: This code can probably be shortened. */
			const char *nested_p = cp;
			FStr result;
			Boolean isError;

			/* XXX: is expanded twice: once here and once below */
			(void)Var_Parse(&nested_p, scope,
			    VARE_UNDEFERR, &result);
			/* TODO: handle errors */
			isError = result.str == var_Error;
			FStr_Done(&result);
			if (isError)
				return FALSE;

			expandLibName = TRUE;
			cp += nested_p - cp;
		} else
			cp++;
	}

	*cp++ = '\0';
	if (expandLibName) {
		char *expanded;
		(void)Var_Subst(libName.str, scope, VARE_UNDEFERR, &expanded);
		/* TODO: handle errors */
		libName = MFStr_InitOwn(expanded);
	}


	for (;;) {
		/*
		 * First skip to the start of the member's name, mark that
		 * place and skip to the end of it (either white-space or
		 * a close paren).
		 */
		Boolean doSubst = FALSE;

		pp_skip_whitespace(&cp);

		memName = cp;
		while (*cp != '\0' && *cp != ')' && !ch_isspace(*cp)) {
			if (*cp == '$') {
				/* Expand nested variable expressions. */
				/* XXX: This code can probably be shortened. */
				FStr result;
				Boolean isError;
				const char *nested_p = cp;

				(void)Var_Parse(&nested_p, scope,
				    VARE_UNDEFERR, &result);
				/* TODO: handle errors */
				isError = result.str == var_Error;
				FStr_Done(&result);

				if (isError)
					return FALSE;

				doSubst = TRUE;
				cp += nested_p - cp;
			} else {
				cp++;
			}
		}

		/*
		 * If the specification ends without a closing parenthesis,
		 * chances are there's something wrong (like a missing
		 * backslash), so it's better to return failure than allow
		 * such things to happen
		 */
		if (*cp == '\0') {
			Parse_Error(PARSE_FATAL,
				    "No closing parenthesis "
				    "in archive specification");
			return FALSE;
		}

		/*
		 * If we didn't move anywhere, we must be done
		 */
		if (cp == memName)
			break;

		saveChar = *cp;
		*cp = '\0';

		/*
		 * XXX: This should be taken care of intelligently by
		 * SuffExpandChildren, both for the archive and the member
		 * portions.
		 */
		/*
		 * If member contains variables, try and substitute for them.
		 * This will slow down archive specs with dynamic sources, of
		 * course, since we'll be (non-)substituting them three
		 * times, but them's the breaks -- we need to do this since
		 * SuffExpandChildren calls us, otherwise we could assume the
		 * thing would be taken care of later.
		 */
		if (doSubst) {
			char *fullName;
			char *p;
			char *unexpandedMemName = memName;

			(void)Var_Subst(memName, scope, VARE_UNDEFERR,
			    &memName);
			/* TODO: handle errors */

			/*
			 * Now form an archive spec and recurse to deal with
			 * nested variables and multi-word variable values.
			 */
			fullName = str_concat4(libName.str, "(", memName, ")");
			p = fullName;

			if (strchr(memName, '$') != NULL &&
			    strcmp(memName, unexpandedMemName) == 0) {
				/*
				 * Must contain dynamic sources, so we can't
				 * deal with it now. Just create an ARCHV node
				 * for the thing and let SuffExpandChildren
				 * handle it.
				 */
				gn = Targ_GetNode(fullName);
				gn->type |= OP_ARCHV;
				Lst_Append(gns, gn);

			} else if (!Arch_ParseArchive(&p, gns, scope)) {
				/* Error in nested call. */
				free(fullName);
				/* XXX: does unexpandedMemName leak? */
				return FALSE;
			}
			free(fullName);
			/* XXX: does unexpandedMemName leak? */

		} else if (Dir_HasWildcards(memName)) {
			StringList members = LST_INIT;
			SearchPath_Expand(&dirSearchPath, memName, &members);

			while (!Lst_IsEmpty(&members)) {
				char *member = Lst_Dequeue(&members);
				char *fullname = str_concat4(libName.str, "(",
							     member, ")");
				free(member);

				gn = Targ_GetNode(fullname);
				free(fullname);

				gn->type |= OP_ARCHV;
				Lst_Append(gns, gn);
			}
			Lst_Done(&members);

		} else {
			char *fullname = str_concat4(libName.str, "(", memName,
						     ")");
			gn = Targ_GetNode(fullname);
			free(fullname);

			/*
			 * We've found the node, but have to make sure the
			 * rest of the world knows it's an archive member,
			 * without having to constantly check for parentheses,
			 * so we type the thing with the OP_ARCHV bit before
			 * we place it on the end of the provided list.
			 */
			gn->type |= OP_ARCHV;
			Lst_Append(gns, gn);
		}
		if (doSubst)
			free(memName);

		*cp = saveChar;
	}

	MFStr_Done(&libName);

	cp++;			/* skip the ')' */
	/* We promised that pp would be set up at the next non-space. */
	pp_skip_whitespace(&cp);
	*pp = cp;
	return TRUE;
}

/*
 * Locate a member of an archive, given the path of the archive and the path
 * of the desired member.
 *
 * Input:
 *	archive		Path to the archive
 *	member		Name of member; only its basename is used.
 *	addToCache	TRUE if archive should be cached if not already so.
 *
 * Results:
 *	The ar_hdr for the member, or NULL.
 *
 * See ArchFindMember for an almost identical copy of this code.
 */
static struct ar_hdr *
ArchStatMember(const char *archive, const char *member, Boolean addToCache)
{
#define AR_MAX_NAME_LEN (sizeof arh.ar_name - 1)
	FILE *arch;
	size_t size;		/* Size of archive member */
	char magic[SARMAG];
	ArchListNode *ln;
	Arch *ar;		/* Archive descriptor */
	struct ar_hdr arh;	/* archive-member header for reading archive */
	char memName[MAXPATHLEN + 1];
	/* Current member name while hashing. */

	/*
	 * Because of space constraints and similar things, files are archived
	 * using their basename, not the entire path.
	 */
	member = str_basename(member);

	for (ln = archives.first; ln != NULL; ln = ln->next) {
		const Arch *a = ln->datum;
		if (strcmp(a->name, archive) == 0)
			break;
	}

	if (ln != NULL) {
		struct ar_hdr *hdr;

		ar = ln->datum;
		hdr = HashTable_FindValue(&ar->members, member);
		if (hdr != NULL)
			return hdr;

		{
			/* Try truncated name */
			char copy[AR_MAX_NAME_LEN + 1];
			size_t len = strlen(member);

			if (len > AR_MAX_NAME_LEN) {
				snprintf(copy, sizeof copy, "%s", member);
				hdr = HashTable_FindValue(&ar->members, copy);
			}
			return hdr;
		}
	}

	if (!addToCache) {
		/*
		 * Caller doesn't want the thing cached, just use
		 * ArchFindMember to read the header for the member out and
		 * close down the stream again. Since the archive is not to be
		 * cached, we assume there's no need to allocate extra room
		 * for the header we're returning, so just declare it static.
		 */
		static struct ar_hdr sarh;

		arch = ArchFindMember(archive, member, &sarh, "r");
		if (arch == NULL)
			return NULL;

		fclose(arch);
		return &sarh;
	}

	/*
	 * We don't have this archive on the list yet, so we want to find out
	 * everything that's in it and cache it so we can get at it quickly.
	 */
	arch = fopen(archive, "r");
	if (arch == NULL)
		return NULL;

	/*
	 * We use the ARMAG string to make sure this is an archive we
	 * can handle...
	 */
	if (fread(magic, SARMAG, 1, arch) != 1 ||
	    strncmp(magic, ARMAG, SARMAG) != 0) {
		(void)fclose(arch);
		return NULL;
	}

	ar = bmake_malloc(sizeof *ar);
	ar->name = bmake_strdup(archive);
	ar->fnametab = NULL;
	ar->fnamesize = 0;
	HashTable_Init(&ar->members);
	memName[AR_MAX_NAME_LEN] = '\0';

	while (fread(&arh, sizeof arh, 1, arch) == 1) {
		char *nameend;

		/* If the header is bogus, there's no way we can recover. */
		if (strncmp(arh.ar_fmag, ARFMAG, sizeof arh.ar_fmag) != 0)
			goto badarch;

		/*
		 * We need to advance the stream's pointer to the start of the
		 * next header. Files are padded with newlines to an even-byte
		 * boundary, so we need to extract the size of the file from
		 * the 'size' field of the header and round it up during the
		 * seek.
		 */
		arh.ar_size[sizeof arh.ar_size - 1] = '\0';
		size = (size_t)strtol(arh.ar_size, NULL, 10);

		memcpy(memName, arh.ar_name, sizeof arh.ar_name);
		nameend = memName + AR_MAX_NAME_LEN;
		while (nameend > memName && *nameend == ' ')
			nameend--;
		nameend[1] = '\0';

#ifdef SVR4ARCHIVES
		/*
		 * svr4 names are slash-terminated.
		 * Also svr4 extended the AR format.
		 */
		if (memName[0] == '/') {
			/* svr4 magic mode; handle it */
			switch (ArchSVR4Entry(ar, memName, size, arch)) {
			case -1:	/* Invalid data */
				goto badarch;
			case 0:		/* List of files entry */
				continue;
			default:	/* Got the entry */
				break;
			}
		} else {
			if (nameend[0] == '/')
				nameend[0] = '\0';
		}
#endif

#ifdef AR_EFMT1
		/*
		 * BSD 4.4 extended AR format: #1/<namelen>, with name as the
		 * first <namelen> bytes of the file
		 */
		if (strncmp(memName, AR_EFMT1, sizeof AR_EFMT1 - 1) == 0 &&
		    ch_isdigit(memName[sizeof AR_EFMT1 - 1])) {

			int elen = atoi(memName + sizeof AR_EFMT1 - 1);

			if ((unsigned int)elen > MAXPATHLEN)
				goto badarch;
			if (fread(memName, (size_t)elen, 1, arch) != 1)
				goto badarch;
			memName[elen] = '\0';
			if (fseek(arch, -elen, SEEK_CUR) != 0)
				goto badarch;
			if (DEBUG(ARCH) || DEBUG(MAKE))
				debug_printf(
				    "ArchStatMember: "
				    "Extended format entry for %s\n",
				    memName);
		}
#endif

		{
			struct ar_hdr *cached_hdr = bmake_malloc(
			    sizeof *cached_hdr);
			memcpy(cached_hdr, &arh, sizeof arh);
			HashTable_Set(&ar->members, memName, cached_hdr);
		}

		if (fseek(arch, ((long)size + 1) & ~1, SEEK_CUR) != 0)
			goto badarch;
	}

	fclose(arch);

	Lst_Append(&archives, ar);

	/*
	 * Now that the archive has been read and cached, we can look into
	 * the addToCache table to find the desired member's header.
	 */
	return HashTable_FindValue(&ar->members, member);

badarch:
	fclose(arch);
	HashTable_Done(&ar->members);
	free(ar->fnametab);
	free(ar);
	return NULL;
}

#ifdef SVR4ARCHIVES
/*
 * Parse an SVR4 style entry that begins with a slash.
 * If it is "//", then load the table of filenames.
 * If it is "/<offset>", then try to substitute the long file name
 * from offset of a table previously read.
 * If a table is read, the file pointer is moved to the next archive member.
 *
 * Results:
 *	-1: Bad data in archive
 *	 0: A table was loaded from the file
 *	 1: Name was successfully substituted from table
 *	 2: Name was not successfully substituted from table
 */
static int
ArchSVR4Entry(Arch *ar, char *inout_name, size_t size, FILE *arch)
{
#define ARLONGNAMES1 "//"
#define ARLONGNAMES2 "/ARFILENAMES"
	size_t entry;
	char *ptr, *eptr;

	if (strncmp(inout_name, ARLONGNAMES1, sizeof ARLONGNAMES1 - 1) == 0 ||
	    strncmp(inout_name, ARLONGNAMES2, sizeof ARLONGNAMES2 - 1) == 0) {

		if (ar->fnametab != NULL) {
			DEBUG0(ARCH,
			       "Attempted to redefine an SVR4 name table\n");
			return -1;
		}

		/*
		 * This is a table of archive names, so we build one for
		 * ourselves
		 */
		ar->fnametab = bmake_malloc(size);
		ar->fnamesize = size;

		if (fread(ar->fnametab, size, 1, arch) != 1) {
			DEBUG0(ARCH, "Reading an SVR4 name table failed\n");
			return -1;
		}
		eptr = ar->fnametab + size;
		for (entry = 0, ptr = ar->fnametab; ptr < eptr; ptr++)
			if (*ptr == '/') {
				entry++;
				*ptr = '\0';
			}
		DEBUG1(ARCH, "Found svr4 archive name table with %lu entries\n",
		       (unsigned long)entry);
		return 0;
	}

	if (inout_name[1] == ' ' || inout_name[1] == '\0')
		return 2;

	entry = (size_t)strtol(&inout_name[1], &eptr, 0);
	if ((*eptr != ' ' && *eptr != '\0') || eptr == &inout_name[1]) {
		DEBUG1(ARCH, "Could not parse SVR4 name %s\n", inout_name);
		return 2;
	}
	if (entry >= ar->fnamesize) {
		DEBUG2(ARCH, "SVR4 entry offset %s is greater than %lu\n",
		       inout_name, (unsigned long)ar->fnamesize);
		return 2;
	}

	DEBUG2(ARCH, "Replaced %s with %s\n", inout_name, &ar->fnametab[entry]);

	snprintf(inout_name, MAXPATHLEN + 1, "%s", &ar->fnametab[entry]);
	return 1;
}
#endif


static Boolean
ArchiveMember_HasName(const struct ar_hdr *hdr,
		      const char *name, size_t namelen)
{
	const size_t ar_name_len = sizeof hdr->ar_name;
	const char *ar_name = hdr->ar_name;

	if (strncmp(ar_name, name, namelen) != 0)
		return FALSE;

	if (namelen >= ar_name_len)
		return namelen == ar_name_len;

	/* hdr->ar_name is space-padded to the right. */
	if (ar_name[namelen] == ' ')
		return TRUE;

	/* In archives created by GNU binutils 2.27, the member names end with
	 * a slash. */
	if (ar_name[namelen] == '/' &&
	    (namelen == ar_name_len || ar_name[namelen + 1] == ' '))
		return TRUE;

	return FALSE;
}

/*
 * Locate a member of an archive, given the path of the archive and the path
 * of the desired member.
 *
 * Input:
 *	archive		Path to the archive
 *	member		Name of member. If it is a path, only the last
 *			component is used.
 *	out_arh		Archive header to be filled in
 *	mode		"r" for read-only access, "r+" for read-write access
 *
 * Output:
 *	return		The archive file, positioned at the start of the
 *			member's struct ar_hdr, or NULL if the member doesn't
 *			exist.
 *	*out_arh	The current struct ar_hdr for member.
 *
 * See ArchStatMember for an almost identical copy of this code.
 */
static FILE *
ArchFindMember(const char *archive, const char *member, struct ar_hdr *out_arh,
	       const char *mode)
{
	FILE *arch;		/* Stream to archive */
	int size;		/* Size of archive member */
	char magic[SARMAG];
	size_t len;

	arch = fopen(archive, mode);
	if (arch == NULL)
		return NULL;

	/*
	 * We use the ARMAG string to make sure this is an archive we
	 * can handle...
	 */
	if (fread(magic, SARMAG, 1, arch) != 1 ||
	    strncmp(magic, ARMAG, SARMAG) != 0) {
		fclose(arch);
		return NULL;
	}

	/*
	 * Because of space constraints and similar things, files are archived
	 * using their basename, not the entire path.
	 */
	member = str_basename(member);

	len = strlen(member);

	while (fread(out_arh, sizeof *out_arh, 1, arch) == 1) {

		if (strncmp(out_arh->ar_fmag, ARFMAG,
			    sizeof out_arh->ar_fmag) != 0) {
			/*
			 * The header is bogus, so the archive is bad
			 * and there's no way we can recover...
			 */
			fclose(arch);
			return NULL;
		}

		DEBUG5(ARCH, "Reading archive %s member %.*s mtime %.*s\n",
		       archive,
		       (int)sizeof out_arh->ar_name, out_arh->ar_name,
		       (int)sizeof out_arh->ar_date, out_arh->ar_date);

		if (ArchiveMember_HasName(out_arh, member, len)) {
			/*
			 * To make life easier for callers that want to update
			 * the archive, we reposition the file at the start of
			 * the header we just read before we return the
			 * stream. In a more general situation, it might be
			 * better to leave the file at the actual member,
			 * rather than its header, but not here.
			 */
			if (fseek(arch, -(long)sizeof *out_arh, SEEK_CUR) !=
			    0) {
				fclose(arch);
				return NULL;
			}
			return arch;
		}

#ifdef AR_EFMT1
		/*
		 * BSD 4.4 extended AR format: #1/<namelen>, with name as the
		 * first <namelen> bytes of the file
		 */
		if (strncmp(out_arh->ar_name, AR_EFMT1, sizeof AR_EFMT1 - 1) ==
		    0 &&
		    (ch_isdigit(out_arh->ar_name[sizeof AR_EFMT1 - 1]))) {
			int elen = atoi(&out_arh->ar_name[sizeof AR_EFMT1 - 1]);
			char ename[MAXPATHLEN + 1];

			if ((unsigned int)elen > MAXPATHLEN) {
				fclose(arch);
				return NULL;
			}
			if (fread(ename, (size_t)elen, 1, arch) != 1) {
				fclose(arch);
				return NULL;
			}
			ename[elen] = '\0';
			if (DEBUG(ARCH) || DEBUG(MAKE))
				debug_printf(
				    "ArchFindMember: "
				    "Extended format entry for %s\n",
				    ename);
			if (strncmp(ename, member, len) == 0) {
				/* Found as extended name */
				if (fseek(arch,
					  -(long)sizeof(struct ar_hdr) - elen,
					  SEEK_CUR) != 0) {
					fclose(arch);
					return NULL;
				}
				return arch;
			}
			if (fseek(arch, -elen, SEEK_CUR) != 0) {
				fclose(arch);
				return NULL;
			}
		}
#endif

		/*
		 * This isn't the member we're after, so we need to advance the
		 * stream's pointer to the start of the next header. Files are
		 * padded with newlines to an even-byte boundary, so we need to
		 * extract the size of the file from the 'size' field of the
		 * header and round it up during the seek.
		 */
		out_arh->ar_size[sizeof out_arh->ar_size - 1] = '\0';
		size = (int)strtol(out_arh->ar_size, NULL, 10);
		if (fseek(arch, (size + 1) & ~1, SEEK_CUR) != 0) {
			fclose(arch);
			return NULL;
		}
	}

	fclose(arch);
	return NULL;
}

/*
 * Touch a member of an archive, on disk.
 * The GNode's modification time is left as-is.
 *
 * The st_mtime of the entire archive is also changed.
 * For a library, it may be required to run ranlib after this.
 *
 * Input:
 *	gn		Node of member to touch
 *
 * Results:
 *	The 'time' field of the member's header is updated.
 */
void
Arch_Touch(GNode *gn)
{
	FILE *f;
	struct ar_hdr arh;

	f = ArchFindMember(GNode_VarArchive(gn), GNode_VarMember(gn), &arh,
			   "r+");
	if (f == NULL)
		return;

	snprintf(arh.ar_date, sizeof arh.ar_date, "%-ld", (unsigned long)now);
	(void)fwrite(&arh, sizeof arh, 1, f);
	fclose(f);		/* TODO: handle errors */
}

/*
 * Given a node which represents a library, touch the thing, making sure that
 * the table of contents is also touched.
 *
 * Both the modification time of the library and of the RANLIBMAG member are
 * set to 'now'.
 */
/*ARGSUSED*/
void
Arch_TouchLib(GNode *gn MAKE_ATTR_UNUSED)
{
#ifdef RANLIBMAG
	FILE *f;
	struct ar_hdr arh;	/* Header describing table of contents */
	struct utimbuf times;

	f = ArchFindMember(gn->path, RANLIBMAG, &arh, "r+");
	if (f == NULL)
		return;

	snprintf(arh.ar_date, sizeof arh.ar_date, "%-ld", (unsigned long)now);
	(void)fwrite(&arh, sizeof arh, 1, f);
	fclose(f);		/* TODO: handle errors */

	times.actime = times.modtime = now;
	utime(gn->path, &times);	/* TODO: handle errors */
#endif
}

/*
 * Update the mtime of the GNode with the mtime from the archive member on
 * disk (or in the cache).
 */
void
Arch_UpdateMTime(GNode *gn)
{
	struct ar_hdr *arh;

	arh = ArchStatMember(GNode_VarArchive(gn), GNode_VarMember(gn), TRUE);
	if (arh != NULL)
		gn->mtime = (time_t)strtol(arh->ar_date, NULL, 10);
	else
		gn->mtime = 0;
}

/*
 * Given a nonexistent archive member's node, update gn->mtime from its
 * archived form, if it exists.
 */
void
Arch_UpdateMemberMTime(GNode *gn)
{
	GNodeListNode *ln;

	for (ln = gn->parents.first; ln != NULL; ln = ln->next) {
		GNode *pgn = ln->datum;

		if (pgn->type & OP_ARCHV) {
			/*
			 * If the parent is an archive specification and is
			 * being made and its member's name matches the name
			 * of the node we were given, record the modification
			 * time of the parent in the child. We keep searching
			 * its parents in case some other parent requires this
			 * child to exist.
			 */
			const char *nameStart = strchr(pgn->name, '(') + 1;
			const char *nameEnd = strchr(nameStart, ')');
			size_t nameLen = (size_t)(nameEnd - nameStart);

			if ((pgn->flags & REMAKE) &&
			    strncmp(nameStart, gn->name, nameLen) == 0) {
				Arch_UpdateMTime(pgn);
				gn->mtime = pgn->mtime;
			}
		} else if (pgn->flags & REMAKE) {
			/*
			 * Something which isn't a library depends on the
			 * existence of this target, so it needs to exist.
			 */
			gn->mtime = 0;
			break;
		}
	}
}

/*
 * Search for a library along the given search path.
 *
 * The node's 'path' field is set to the found path (including the
 * actual file name, not -l...). If the system can handle the -L
 * flag when linking (or we cannot find the library), we assume that
 * the user has placed the .LIBS variable in the final linking
 * command (or the linker will know where to find it) and set the
 * TARGET variable for this node to be the node's name. Otherwise,
 * we set the TARGET variable to be the full path of the library,
 * as returned by Dir_FindFile.
 *
 * Input:
 *	gn		Node of library to find
 */
void
Arch_FindLib(GNode *gn, SearchPath *path)
{
	char *libName = str_concat3("lib", gn->name + 2, ".a");
	gn->path = Dir_FindFile(libName, path);
	free(libName);

#ifdef LIBRARIES
	Var_Set(gn, TARGET, gn->name);
#else
	Var_Set(gn, TARGET, GNode_Path(gn));
#endif
}

/*
 * Decide if a node with the OP_LIB attribute is out-of-date. Called from
 * GNode_IsOODate to make its life easier.
 * The library is cached if it hasn't been already.
 *
 * There are several ways for a library to be out-of-date that are
 * not available to ordinary files. In addition, there are ways
 * that are open to regular files that are not available to
 * libraries.
 *
 * A library that is only used as a source is never
 * considered out-of-date by itself. This does not preclude the
 * library's modification time from making its parent be out-of-date.
 * A library will be considered out-of-date for any of these reasons,
 * given that it is a target on a dependency line somewhere:
 *
 *	Its modification time is less than that of one of its sources
 *	(gn->mtime < gn->youngestChild->mtime).
 *
 *	Its modification time is greater than the time at which the make
 *	began (i.e. it's been modified in the course of the make, probably
 *	by archiving).
 *
 *	The modification time of one of its sources is greater than the one
 *	of its RANLIBMAG member (i.e. its table of contents is out-of-date).
 *	We don't compare the archive time vs. TOC time because they can be
 *	too close. In my opinion we should not bother with the TOC at all
 *	since this is used by 'ar' rules that affect the data contents of the
 *	archive, not by ranlib rules, which affect the TOC.
 */
Boolean
Arch_LibOODate(GNode *gn)
{
	Boolean oodate;

	if (gn->type & OP_PHONY) {
		oodate = TRUE;
	} else if (!GNode_IsTarget(gn) && Lst_IsEmpty(&gn->children)) {
		oodate = FALSE;
	} else if ((!Lst_IsEmpty(&gn->children) && gn->youngestChild == NULL) ||
		   (gn->mtime > now) ||
		   (gn->youngestChild != NULL &&
		    gn->mtime < gn->youngestChild->mtime)) {
		oodate = TRUE;
	} else {
#ifdef RANLIBMAG
		struct ar_hdr *arh;	/* Header for __.SYMDEF */
		int modTimeTOC;		/* The table-of-contents' mod time */

		arh = ArchStatMember(gn->path, RANLIBMAG, FALSE);

		if (arh != NULL) {
			modTimeTOC = (int)strtol(arh->ar_date, NULL, 10);

			if (DEBUG(ARCH) || DEBUG(MAKE))
				debug_printf("%s modified %s...",
					     RANLIBMAG,
					     Targ_FmtTime(modTimeTOC));
			oodate = gn->youngestChild == NULL ||
				 gn->youngestChild->mtime > modTimeTOC;
		} else {
			/*
			 * A library without a table of contents is out-of-date.
			 */
			if (DEBUG(ARCH) || DEBUG(MAKE))
				debug_printf("no toc...");
			oodate = TRUE;
		}
#else
		oodate = FALSE;
#endif
	}
	return oodate;
}

/* Initialize the archives module. */
void
Arch_Init(void)
{
	Lst_Init(&archives);
}

/* Clean up the archives module. */
void
Arch_End(void)
{
#ifdef CLEANUP
	Lst_DoneCall(&archives, ArchFree);
#endif
}

Boolean
Arch_IsLib(GNode *gn)
{
	static const char armag[] = "!<arch>\n";
	char buf[sizeof armag - 1];
	int fd;

	if ((fd = open(gn->path, O_RDONLY)) == -1)
		return FALSE;

	if (read(fd, buf, sizeof buf) != sizeof buf) {
		(void)close(fd);
		return FALSE;
	}

	(void)close(fd);

	return memcmp(buf, armag, sizeof buf) == 0;
}
