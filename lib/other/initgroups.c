/*
initgroups.c
*/
#include <sys/param.h>
#define setgroups _setgroups
#include <unistd.h>
#include <string.h>
#include <grp.h>


int initgroups(const char *name, gid_t basegid)
{
  struct group *gr;
  int r, found = 0, n = 0;
  gid_t groups[NGROUPS];

  if((r = setgid(basegid)) < 0)
	return(r);

  setgrent();
  while ((gr = getgrent()) != NULL && (n + 1) <= NGROUPS) {
	char **mem;
	for(mem = gr->gr_mem; mem && *mem; mem++) {
		if(!strcmp(name, *mem)) {
			groups[n++] = gr->gr_gid;
			break;
		}
	}
  }

  endgrent();

  return setgroups(n, groups);
}

