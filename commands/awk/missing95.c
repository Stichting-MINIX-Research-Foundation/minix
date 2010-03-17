/* popen and pclose are not part of win 95 and nt,
   but it appears that _popen and _pclose "work".
   if this won't load, use the return NULL statements. */

#include <stdio.h>
FILE *popen(char *s, char *m) {
	return _popen(s, m);	/* return NULL; */
}

int pclose(FILE *f) {
	return _pclose(f);	/* return NULL; */
}
