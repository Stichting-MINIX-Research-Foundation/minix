#ifndef lint
static char *rcsid = "$Id: aliaslist.c,v 1.1.1.1 2003-06-04 00:25:47 marka Exp $";
#endif

/*
 * Copyright (c) 2002 Japan Network Information Center.  All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <config.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <idn/aliaslist.h>
#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/result.h>

struct aliasitem {
	char *pattern;			/* name pattern */
	char *encoding;			/* MIME-preferred charset name */
	struct aliasitem *next;
};
typedef struct aliasitem *aliasitem_t;

struct idn__aliaslist {
	aliasitem_t first_item;		/* first item of the list */
};

static idn_result_t
additem_to_top(idn__aliaslist_t list,
	       const char *pattern, const char *encoding);

static idn_result_t
additem_to_bottom(idn__aliaslist_t list,
		  const char *pattern, const char *encoding);

static int		match(const char *pattern, const char *str);

static idn_result_t	create_item(const char *pattern, const char *encoding,
				    aliasitem_t *itemp);

#ifdef DEBUG
static void		dump_list(idn__aliaslist_t list);
#endif

idn_result_t
idn__aliaslist_create(idn__aliaslist_t *listp) {
	static int size = sizeof(struct idn__aliaslist);

	TRACE(("idn__aliaslist_create()\n"));

	assert(listp != NULL);

	if ((*listp = malloc(size)) == NULL) {
		return (idn_nomemory);
	}
	(*listp)->first_item = NULL;

	return (idn_success);
}

void
idn__aliaslist_destroy(idn__aliaslist_t list) {
	aliasitem_t current;
	aliasitem_t next;

	TRACE(("idn__aliaslist_destroy()\n"));

	assert(list != NULL);

	current = list->first_item;
	while (current != NULL) {
		if (current->pattern != NULL) {
			free(current->pattern);
		}
		if (current->encoding != NULL) {
			free(current->encoding);
		}
		next = current->next;
		free(current);
		current = next;
	}
	free(list);
}

idn_result_t
idn__aliaslist_aliasfile(idn__aliaslist_t list, const char *path) {
	FILE *fp;
	int line_no;
	idn_result_t r = idn_success;
	char line[200], alias[200], real[200];

	assert(path != NULL);

	TRACE(("idn__aliaslist_aliasfile(path=%s)\n", path));

	if ((fp = fopen(path, "r")) == NULL) {
		return (idn_nofile);
	}
	for (line_no = 1; fgets(line, sizeof(line), fp) != NULL; line_no++) {
		unsigned char *p = (unsigned char *)line;

		while (isascii(*p) && isspace(*p))
			p++;
		if (*p == '#' || *p == '\n')
			continue;
		if (sscanf((char *)p, "%s %s", alias, real) == 2) {
			r = additem_to_bottom(list, alias, real);
			if (r != idn_success)
				break;
		} else {
			INFO(("idn__aliaslist_aliasfile: file %s has "
			      "invalid contents at line %d\n",
			      path, line_no));
			r = idn_invalid_syntax;
			break;
		}
	}
	fclose(fp);

#ifdef DEBUG
	dump_list(list);
#endif

	return (r);
}

idn_result_t
idn__aliaslist_additem(idn__aliaslist_t list,
		       const char *pattern, const char *encoding,
		       int first_item) {
	if (first_item) {
		return additem_to_top(list, pattern, encoding);
	} else {
		return additem_to_bottom(list, pattern, encoding);
	}
}

static idn_result_t
additem_to_top(idn__aliaslist_t list,
	       const char *pattern, const char *encoding) {
	aliasitem_t new_item;
	idn_result_t r;

	TRACE(("additem_to_top()\n"));

	assert(list != NULL);
	assert(pattern != NULL);
	assert(encoding != NULL);

	if ((r = create_item(pattern, encoding, &new_item))
	    != idn_success) {
		WARNING(("additem_to_top: malloc failed\n"));
		return (r);
	}

	new_item->next = list->first_item;
	list->first_item = new_item;

#ifdef DEBUG
	dump_list(list);
#endif

	return (idn_success);
}

static idn_result_t
additem_to_bottom(idn__aliaslist_t list,
		  const char *pattern, const char *encoding) {
	aliasitem_t new_item;
	idn_result_t r;

	TRACE(("additem_to_bottom()\n"));

	assert(list != NULL);
	assert(pattern != NULL);
	assert(encoding != NULL);

	r = create_item(pattern, encoding, &new_item);
	if (r != idn_success) {
		WARNING(("additem_to_bottom: malloc failed\n"));
		return r;
	}

	if (list->first_item == NULL) {
		list->first_item = new_item;
	} else {
		aliasitem_t cur_item = list->first_item;
		for (;;) {
			if (cur_item->next == NULL) {
				break;
			}
			cur_item = cur_item->next;
		}
		cur_item->next = new_item;
	}

	return (idn_success);
}

idn_result_t
idn__aliaslist_find(idn__aliaslist_t list,
		   const char *pattern, char **encodingp) {
	aliasitem_t current;

	TRACE(("idn__aliaslist_find()\n"));

	assert(list != NULL);
	assert(pattern != NULL);

#ifdef DEBUG
	DUMP(("target pattern: %s\n", pattern));
#endif
	current = list->first_item;
	while (current != NULL) {
#ifdef DEBUG
		DUMP(("current pattern: %s, encoding: %s\n",
		      current->pattern, current->encoding));
#endif
		if (match(current->pattern, pattern)) {
			*encodingp = current->encoding;
			return (idn_success);
		}
		current = current->next;
	}

	TRACE(("idn__aliaslist_find(): not found\n"));
	*encodingp = (char *)pattern;
	return (idn_notfound);
}

/*
 * Wild card matching function that supports only '*'.
 */
static int
match(const char *pattern, const char *str) {
	for (;;) {
		int c;

		switch (c = *pattern++) {
		case '\0':
			return (*str == '\0');
		case '*':
			while (!match(pattern, str)) {
				if (*str == '\0')
					return (0);
				str++;
			}
			return (1);
			break;
		default:
			if (*str++ != c)
				return (0);
			break;
		}
	}
}

/*
 * List item creation.
 * pattern and encoding must not be NULL.
 */
static idn_result_t
create_item(const char *pattern, const char *encoding,
	    aliasitem_t *itemp) {
	static size_t size = sizeof(struct aliasitem);

	assert(pattern != NULL);
	assert(encoding != NULL);

	if ((*itemp = malloc(size)) == NULL)
		return (idn_nomemory);

	if (((*itemp)->pattern = malloc(strlen(pattern) + 1)) == NULL) {
		free(*itemp);
		*itemp = NULL;
		return (idn_nomemory);
	}

	if (((*itemp)->encoding = malloc(strlen(encoding) + 1)) == NULL) {
		free((*itemp)->pattern);
		free(*itemp);
		*itemp = NULL;
		return (idn_nomemory);
	}

	(void)strcpy((*itemp)->pattern, pattern);
	(void)strcpy((*itemp)->encoding, encoding);
	(*itemp)->next = NULL;

	return (idn_success);
}

#ifdef DEBUG
static void
dump_list(idn__aliaslist_t list) {
	aliasitem_t item;
	int i;

	TRACE(("dump_list()\n"));
	if (list == NULL) {
		TRACE(("list is NULL\n"));
		return;
	}
	item = list->first_item;
	i = 0;
	while (item != NULL) {
		DUMP(("%d: %s\t%s\n", i, item->pattern, item->encoding));
		item = item->next;
		i++;
	}
}
#endif
