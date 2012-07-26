/*****************************************************************
**
**	@(#) strlist.c (c) Mar 2005  Holger Zuleger
**
**	TODO:	Maybe we should use a special type for the list:
**		typedef struct { char cnt; char list[0+1]; } strlist__t;
**		This results in better type control of the function parameters
**
**	Copyright (c) Mar 2005, Holger Zuleger HZnet. All rights reserved.
**
**	This software is open source.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	Redistributions of source code must retain the above copyright notice,
**	this list of conditions and the following disclaimer.
**
**	Redistributions in binary form must reproduce the above copyright notice,
**	this list of conditions and the following disclaimer in the documentation
**	and/or other materials provided with the distribution.
**
**	Neither the name of Holger Zuleger HZnet nor the names of its contributors may
**	be used to endorse or promote products derived from this software without
**	specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
**	TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
**	PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
**	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
**	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
**	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
**	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
**	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
**	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************/

#ifdef TEST
# include <stdio.h>
#endif
#include <string.h>
#include <stdlib.h>
#include "strlist.h"


/*****************************************************************
**	prepstrlist (str, delim)
**	prepare a string with delimiters to a so called strlist.
**	'str' is a list of substrings delimeted by 'delim'
**	The # of strings is stored at the first byte of the allocated
**	memory. Every substring is stored as a '\0' terminated C-String.
**	The function returns a pointer to dynamic allocated memory
*****************************************************************/
char	*prepstrlist (const char *str, const char *delim)
{
	char	*p;
	char	*new;
	int	len;
	int	cnt;

	if ( str == NULL )
		return NULL;

	len = strlen (str);
	if ( (new = malloc (len + 2)) == NULL )
		return new;

	cnt = 0;
	p = new;
	for ( *p++ = '\0'; *str; str++ )
	{
		if ( strchr (delim, *str) == NULL )
			*p++ = *str;
		else if ( p[-1] != '\0' )
		{
			*p++ = '\0';
			cnt++;
		}
	}
	*p = '\0';	/*terminate string */
	if ( p[-1] != '\0' )
		cnt++;
	*new = cnt & 0xFF;

	return new;
}

/*****************************************************************
**	isinlist (str, list)
**	check if 'list' contains 'str'
*****************************************************************/
int	isinlist (const char *str, const char *list)
{
	int	cnt;

	if ( list == NULL || *list == '\0' )
		return 1;
	if ( str == NULL || *str == '\0' )
		return 0;

	cnt = *list;
	while ( cnt-- > 0 )
	{
		list++;
		if ( strcmp (str, list) == 0 )
			return 1;
		list += strlen (list);
	}

	return 0;
}

/*****************************************************************
**	unprepstrlist (list, delimc)
*****************************************************************/
char	*unprepstrlist (char *list, char delimc)
{
	char	*p;
	int	cnt;

	cnt = *list & 0xFF;
	p = list;
	for ( *p++ = delimc; cnt > 1; p++ )
		if ( *p == '\0' )
		{
			*p = delimc;
			cnt--;
		}

	return list;
}

#ifdef TEST
main (int argc, char *argv[])
{
	FILE	*fp;
	char	*p;
	char	*searchlist = NULL;
	char	group[255];

	if ( argc > 1 )
		searchlist = prepstrlist (argv[1], LISTDELIM);

	printf ("searchlist: %d entrys: \n", searchlist[0]);
	if ( (fp = fopen ("/etc/group", "r")) == NULL )
		exit (fprintf (stderr, "can't open file\n"));

	while ( fscanf (fp, "%[^:]:%*[^\n]\n", group) != EOF )
		if ( isinlist (group, searchlist) )
			printf ("%s\n", group);

	fclose (fp);

	printf ("searchlist: \"%s\"\n", unprepstrlist  (searchlist, *LISTDELIM));
	for ( p = searchlist; *p; p++ )
		if ( *p < 32 )
			printf ("<%d>", *p);
		else
			printf ("%c", *p);
	printf ("\n");
}
#endif
