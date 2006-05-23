/*
complete.c

Created:	July 1995 by Philip Homburg <philip@cs.vu.nl>
*/

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "myhistedit.h"
#include "shell.h"

#include "complete.h"
#include "error.h"
#include "expand.h"
#include "nodes.h"
#include "memalloc.h"

static char **getlist(EditLine *el, int *baselen, int *isdir);
static char **getlist_tilde(char *prefix);
static int vstrcmp(const void *v1, const void *v2);
static void print_list(char **list);
static int install_extra(EditLine *el, char **list, int baselen, int isdir);

unsigned char complete(EditLine *el, int ch)
{
	struct stackmark mark;
	const LineInfo *lf;
	char **list;
	int baselen, prefix, isdir;

	/* Direct the cursor the the end of the word. */
	for(;;)
	{
		lf = el_line(el);
		if (lf->cursor < lf->lastchar &&
			!isspace((unsigned char)*lf->cursor))
		{
			(*(char **)&lf->cursor)++;	/* XXX */
		}
		else
			break;
	}

	setstackmark(&mark);
	list= getlist(el, &baselen, &isdir);
	if (list)
	{
		prefix= install_extra(el, list, baselen, isdir);
		el_push(el, "i");
	}
	popstackmark(&mark);
	if (list)
		return CC_REFRESH;
	else
		return CC_ERROR;
}

unsigned char complete_list(EditLine *el, int ch)
{
	struct stackmark mark;
	char **list;

	setstackmark(&mark);
	list= getlist(el, NULL, NULL);
	if (list)
	{
		print_list(list);
		re_goto_bottom(el);
	}
	popstackmark(&mark);
	if (list)
	{
		return CC_REFRESH;
	}
	else
		return CC_ERROR;
}

unsigned char complete_or_list(EditLine *el, int ch)
{
	struct stackmark mark;
	const LineInfo *lf;
	char **list;
	int baselen, prefix, isdir;

	setstackmark(&mark);
	list= getlist(el, &baselen, &isdir);
	if (list)
	{
		prefix= install_extra(el, list, baselen, isdir);
		if (prefix == baselen)
		{
			print_list(list);
			re_goto_bottom(el);
		}
	}
	popstackmark(&mark);
	if (list)
		return CC_REFRESH;
	else
		return CC_ERROR;
}

unsigned char complete_expand(EditLine *el, int ch)
{
	printf("complete_expand\n");
	return CC_ERROR;
}

static char **getlist(EditLine *el, int *baselen, int *isdir)
{
	const LineInfo *lf;
	const char *begin, *end;
	char *dirnam, *basenam;
	union node arg;
	struct arglist arglist;
	DIR *dir;
	struct dirent *dirent;
	int i, l, n;
	char *p, **list;
	struct strlist *slp, *nslp;
	struct stat sb;

	lf = el_line(el);

	/* Try to find to begin and end of the word that we have to comple. */
	begin= lf->cursor;
	while (begin > lf->buffer && !isspace((unsigned char)begin[-1]))
		begin--;
	end= lf->cursor;
	while (end < lf->lastchar && !isspace((unsigned char)end[0]))
		end++;

	*(const char **)&lf->cursor= end; /* XXX */

	/* Copy the word to a string */
	dirnam= stalloc(end-begin+1);
	strncpy(dirnam, begin, end-begin);
	dirnam[end-begin]= '\0';

	/* Cut the word in two pieces: a path and a (partial) component. */
	basenam= strrchr(dirnam, '/');
	if (basenam)
	{
		basenam++;
		p= stalloc(strlen(basenam) + 1);
		strcpy(p, basenam);
		*basenam= '\0';
		basenam= p;
	}
	else
	{
		if (dirnam[0] == '~')
			return getlist_tilde(dirnam);
		basenam= dirnam;
		dirnam= "./";
	}
	if (baselen)
		*baselen= strlen(basenam);

	arg.type= NARG;
	arg.narg.next= NULL;
	arg.narg.text= dirnam;
	arg.narg.backquote= NULL;
	arglist.list= NULL;
	arglist.lastp= &arglist.list;
	expandarg(&arg, &arglist, EXP_TILDE);

	INTOFF;
	list= NULL;
	dir= opendir(arglist.list->text);
	if (dir)
	{
		slp= NULL;
		n= 0;
		l= strlen(basenam);
		while(dirent= readdir(dir))
		{
			if (strncmp(dirent->d_name, basenam, l) != 0)
				continue;
			if (l == 0 && dirent->d_name[0] == '.')
				continue;
			nslp= stalloc(sizeof(*nslp));
			nslp->next= slp;
			slp= nslp;
			slp->text= stalloc(strlen(dirent->d_name)+1);
			strcpy(slp->text, dirent->d_name);
			n++;
			if (n == 1 && isdir != NULL)
			{
				/* Try to findout whether this entry is a
				 * file or a directory.
				 */
				p= stalloc(strlen(arglist.list->text) +
					strlen(dirent->d_name) + 1);
				strcpy(p, arglist.list->text);
				strcat(p, dirent->d_name);
				if (stat(p, &sb) == -1)
					printf("stat '%s' failed: %s\n",
						p, strerror(errno));
				if (stat(p, &sb) == 0 && S_ISDIR(sb.st_mode))
					*isdir= 1;
				else
					*isdir= 0;
			}
		}
		closedir(dir);
		if (n != 0)
		{
			list= stalloc((n+1)*sizeof(*list));
			for(i= 0; slp; i++, slp= slp->next)
				list[i]= slp->text;
			if (i != n)
				error("complete'make_list: i != n");
			list[i]= NULL;
			qsort(list, n, sizeof(*list), vstrcmp);
		}
	}
	INTON;
	return list;
}

static char **getlist_tilde(char *prefix)
{
	printf("should ~-complete '%s'\n", prefix);
	return NULL;
}

static int vstrcmp(const void *v1, const void *v2)
{
	return strcmp(*(char **)v1, *(char **)v2);
}

#define MAXCOLS 40
#define SEPWIDTH 4

static void print_list(char **list)
{
	struct
	{
		int cols;
		int start[MAXCOLS+1];
		int width[MAXCOLS];
	} best, next;
	int e, i, j, l, n, o, cols, maxw, width;
	int linewidth= 80;

	/* Count the number of entries. */
	for (n= 0; list[n]; n++)
		;	/* do nothing */
	if (n == 0)
		error("complete'print_list: n= 0");

	/* Try to maximize the number of columns */
	for (cols= 1; cols<= MAXCOLS; cols++)
	{
		next.cols= cols;

		o= 0;
		width= 0;
		for(j= 0; j<cols; j++)
		{
			next.start[j]= o;

			/* Number of entries in this column. */
			e= (n-o)/(cols-j);
			if ((n-o)%(cols-j))
				e++;

			maxw= 0;
			for (i= 0; i<e; i++)
			{
				l= strlen(list[o+i]);
				if (l < 6)
					l= 6;
				l += SEPWIDTH;
				if (l > maxw)
					maxw= l;
			}
			next.width[j]= maxw;
			width += maxw;
			o += e;
		}
		next.start[j]= o;
		if (cols > 1 && width-SEPWIDTH>linewidth)
			break;
		best= next;
	}
	cols= best.cols;
	e= best.start[1];
	printf("\n");
	for(i= 0; i<e; i++)
	{
		for (j= 0; j<cols; j++)
		{
			if (best.start[j]+i == best.start[j+1])
				continue;
			if (j < cols-1)
			{
				printf("%-*s", best.width[j],
					list[best.start[j]+i]);
			}
			else
			{
				printf("%s", list[best.start[j]+i]);
			}
		}
		if (i < e-1)
			printf("\n");
	}
	fflush(stdout);
}

static int install_extra(EditLine *el, char **list, int baselen, int isdir)
{
	int l;
	char *p, **lp;

	l= strlen(list[0]);
	for (lp= &list[1]; *lp; lp++)
	{
		while(l>0)
		{
			if (strncmp(list[0], *lp, l) != 0)
				l--;
			else
				break;
		}
	}
	if (l > baselen || list[1] == NULL)
	{
		p= stalloc(l-baselen+2);
		strncpy(p, list[0]+baselen, l-baselen);
		if (list[1] == NULL)
		{
			p[l-baselen]= isdir ? '/' : ' ';
			p[l-baselen+1]= '\0';
			l++;
		}
		else
			p[l-baselen]= '\0';
		if (el_insertstr(el, p) == -1)
			return -1;
	}
	return l;
}

/*
 * $PchId: complete.c,v 1.2 2006/04/10 14:35:53 philip Exp $
 */
