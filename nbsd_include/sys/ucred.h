#ifndef __SYS_UCRED_H
#define __SYS_UCRED_H

struct ucred_old
{
	pid_t   pid;
	short   uid;
	char    gid;
};

struct ucred
{
	pid_t	pid;
	uid_t	uid;
	gid_t	gid;
};

#endif
