/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# ifndef lint
static char rcsid[] = "$Header$";
# endif not lint

# define _PATTERN_

# include "in_all.h"
# include "pattern.h"
# include "getline.h"

# if V8_REGEX
# include <regexp.h>
# endif V8_REGEX

/*
 * Interface to regular expression routines.
 * Also: simple minded patterns without meta-characters.
 */

# if USG_REGEX
static char *pattern;		/* Pointer to compiled pattern */
char *regcmp(), *regex();
# endif USG_REGEX
# if V8_REGEX
static struct regexp *pattern;
static char *rc_error;
struct regexp *regcomp();
# endif V8_REGEX

# if USG_REGEX || V8_REGEX
/*
 * Compile a new pattern, but first free previous result.
 */

char *
re_comp(s) char *s; {

	if (!*s) {
		/*
		 * user wants previous pattern
		 */
		return (char *) 0;
	}
	if (pattern) {
		/*
		 * there was a compiled pattern
		 */
		free(pattern);
		pattern = 0;
	}
# if USG_REGEX
	return (pattern = regcmp(s, (char *) 0)) ?
		(char *) 0 :
		"Error in pattern";
# endif USG_REGEX
# if V8_REGEX
	pattern = regcomp(s);
	if (pattern) return (char *) 0;
	if (rc_error) return rc_error;
	return "Error in pattern";
# endif V8_REGEX
}

# if V8_REGEX
VOID
regerror(str) char *str; {
	rc_error = str;
}
# endif V8_REGEX

/*
 * Search for compiled pattern in string "s". Return 0 if not found.
 */

re_exec(s) char *s; {

# if USG_REGEX
	return !(regex(pattern,s) == 0);
# endif USG_REGEX
# if V8_REGEX
#  if _MINIX
	return regexec(pattern,s,1);
#  else
	return regexec(pattern,s);
#  endif
# endif V8_REGEX
}
# else
# ifndef BSD_REGEX
/*
 * In this case, simple minded pattern search without meta-characters
 */

char	*strcpy();

static char *pattern;

/*
 * re_comp : Just remember pattern.
 */

char *
re_comp(s) char *s; {

	if (!*s) {
		/*
		 * User wants previous pattern
		 */
		if (!pattern) {
			return "No previous regular expression";
		}
		return (char *) 0;
	}
	if (pattern) {
		/*
		 * Free old pattern
		 */
		free(pattern);
	}
	pattern = alloc((unsigned) (strlen(s) + 1), 0);
	(VOID) strcpy(pattern,s);
	return (char *) 0;
}

/*
 * re-exec : Simple minded pattern matcher
 */

re_exec(s) register char *s; {

	register char *ppat, *pstr;

	for (; *s; s++) {
		/*
		 * As long as there are characters ...
		 */
		ppat = pattern; /* Try the pattern again */
		pstr = s;
		while (*ppat == *pstr) {
			if (*++ppat == '\0') {
				/*
				 * The pattern matched! Report success
				 */
				return 1;
			}
			if (*++pstr == '\0') {
				/*
				 * Not enough characters left in the string.
				 * Report failure
				 */
				return 0;
			}
		}
	}
	return 0;		/* Failure */
}
# endif not BSD_REGEX
# endif
