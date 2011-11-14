#ifndef __SYS_UCRED_H
#define __SYS_UCRED_H

struct ucred_old
{
	pid_t   pid;
	short   uid;
	char   gid;
};

struct ucred
{
	pid_t   pid;
	uid_t   uid;
	gid_t   gid;
};

/* Userland's view of credentials. This should not change */
struct uucred {
        unsigned short  cr_unused;              /* not used, compat */
        uid_t           cr_uid;                 /* effective user id */
        gid_t           cr_gid;                 /* effective group id */
        short           cr_ngroups;             /* number of groups */
        gid_t           cr_groups[NGROUPS_MAX];     /* groups */
};

#endif
