#ifndef _TERMCAP_H
#define _TERMCAP_H

#include <ansi.h>

_PROTOTYPE( int tgetent, (char *_bp, char *_name)			);
_PROTOTYPE( int tgetflag, (char *_id)					);
_PROTOTYPE( int tgetnum, (char *_id)					);
_PROTOTYPE( char *tgetstr, (char *_id, char **_area)			);
_PROTOTYPE( char *tgoto, (char *_cm, int _destcol, int _destline)	);
_PROTOTYPE( int tputs, (char *_cp, int _affcnt, void (*_outc)(int))	);

#endif /* _TERMCAP_H */
