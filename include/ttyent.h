/* <ttyent.h> is used by getttyent(3).			Author: Kees J. Bot
 *								28 Oct 1995
 */
#ifndef _TTYENT_H
#define _TTYENT_H

#ifndef _ANSI_H
#include <ansi.h>
#endif

struct ttyent {
	char	*ty_name;	/* Name of the terminal device. */
	char	*ty_type;	/* Terminal type name (termcap(3)). */
	char	**ty_getty;	/* Program to run, normally getty. */
	char	**ty_init;	/* Initialization command, normally stty. */
};

_PROTOTYPE( struct ttyent *getttyent, (void)				);
_PROTOTYPE( struct ttyent *getttynam, (const char *_name)		);
_PROTOTYPE( int setttyent, (void)					);
_PROTOTYPE( void endttyent, (void)					);

#endif /* _TTYENT_H */
