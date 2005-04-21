/* id - return uid and gid		Author: John J. Marco */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
/* 		----- id.c -----					*/
/* Id - get real and effective user id and group id			*/
/* Author: John J. Marco						*/
/*	   pa1343@sdcc15.ucsd.edu					*/
/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>

_PROTOTYPE(int main, (void));

int main()
{
  struct passwd *pwd;
  struct group *grp;
  uid_t ruid, euid;
  gid_t rgid, egid;
#if __minix_vmd
  uid_t suid;
  gid_t sgid;
#else
# define suid ruid
# define sgid rgid
#endif
#if NGROUPS_MAX > 0
  gid_t groups[NGROUPS_MAX];
  int ngroups;
#else
# define groups (&rgid)
# define ngroups 0
#endif
  int g;
  int isug;

#if __minix_vmd
  get6id(&ruid, &euid, &suid, &rgid, &egid, &sgid);
  isug = issetugid();
#else
  ruid = getuid();
  euid = geteuid();
  rgid = getgid();
  egid = getegid();
  isug = 0;
#endif
#if NGROUPS_MAX > 0
  ngroups = getgroups(NGROUPS_MAX, groups);
#endif

  if ((pwd = getpwuid(ruid)) == NULL)
	printf("uid=%d", ruid);
  else
	printf("uid=%d(%s)", ruid, pwd->pw_name);

  if ((grp = getgrgid(rgid)) == NULL)
	printf(" gid=%d", rgid);
  else
	printf(" gid=%d(%s)", rgid, grp->gr_name);

  if (euid != ruid)
	if ((pwd = getpwuid(euid)) != NULL)
		printf(" euid=%d(%s)", euid, pwd->pw_name);
	else
		printf(" euid=%d", euid);

  if (egid != rgid)
	if ((grp = getgrgid(egid)) != NULL)
		printf(" egid=%d(%s)", egid, grp->gr_name);
	else
		printf(" egid=%d", egid);

  if (suid != euid)
	if ((pwd = getpwuid(suid)) != NULL)
		printf(" suid=%d(%s)", suid, pwd->pw_name);
	else
		printf(" suid=%d", suid);

  if (sgid != egid)
	if ((grp = getgrgid(sgid)) != NULL)
		printf(" sgid=%d(%s)", sgid, grp->gr_name);
	else
		printf(" sgid=%d", sgid);

  if (isug) {
	printf(" issetugid");
  }

  if (ngroups > 0) {
	printf(" groups=");
	for (g = 0; g < ngroups; g++) {
		if (g > 0) fputc(',', stdout);
		if ((grp = getgrgid(groups[g])) == NULL)
			printf("%d", groups[g]);
		else
			printf("%d(%s)", groups[g], grp->gr_name);
	}
  }

  printf("\n");
  return(0);
}
