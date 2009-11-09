/* The <locale.h> header is used to custom tailor currency symbols, decimal 
 * points, and other items to the local style.  It is ANSI's attempt at 
 * avoiding cultural imperialism.  The locale given below is for C.
 */

#ifndef _LOCALE_H
#define _LOCALE_H

#ifndef _ANSI_H
#include <ansi.h>
#endif

struct lconv {
  char *decimal_point;		/* "." */
  char *thousands_sep;		/* ""  */
  char *grouping;		/* ""  */
  char *int_curr_symbol;	/* ""  */
  char *currency_symbol;	/* ""  */
  char *mon_decimal_point;	/* ""  */
  char *mon_thousands_sep;	/* ""  */
  char *mon_grouping;		/* ""  */
  char *positive_sign;		/* ""  */
  char *negative_sign;		/* ""  */
  char int_frac_digits;		/* CHAR_MAX */
  char frac_digits;		/* CHAR_MAX */
  char p_cs_precedes;		/* CHAR_MAX */
  char p_sep_by_space;		/* CHAR_MAX */
  char n_cs_precedes;		/* CHAR_MAX */
  char n_sep_by_space;		/* CHAR_MAX */
  char p_sign_posn;		/* CHAR_MAX */
  char n_sign_posn;		/* CHAR_MAX */
};

#define NULL    ((void *)0)

#define LC_ALL             1
#define LC_COLLATE         2
#define LC_CTYPE           3
#define LC_MONETARY        4
#define LC_NUMERIC         5
#define LC_TIME            6

/* Function Prototypes. */
_PROTOTYPE( char *setlocale, (int _category, const char *_locale)	);
_PROTOTYPE( struct lconv *localeconv, (void)				);

#endif /* _LOCALE_H */
