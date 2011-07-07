/*
 * grotesque hack to get these functions working.
 */

#include <sys/types.h>
#include <compat/pwd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

/*
 * group_from_gid()
 *      caches the name (if any) for the gid. If noname clear, we always
 *      return the stored name (if valid or invalid match).
 *      We use a simple hash table.
 * Return
 *      Pointer to stored name (or a empty string)
 */
const char *
group_from_gid(gid_t gid, int noname)
{
	static char buf[16];
	struct group *g = getgrgid(gid);
	if (g == NULL) {
		if (noname) {
			return NULL;
		} else {
			sprintf(buf, "%d", gid);
			return buf;
		}
	}
	return g->gr_name;
}

/*
 * user_from_uid() 
 *      caches the name (if any) for the uid. If noname clear, we always
 *      return the stored name (if valid or invalid match).
 *      We use a simple hash table.
 * Return
 *      Pointer to stored name (or a empty string)
 */
const char *
user_from_uid(uid_t uid, int noname)
{
	static char buf[16];
	struct passwd *p = getpwuid(uid);
	if (p == NULL) {
		if (noname) {
			return NULL;
		} else {
			sprintf(buf, "%d", uid);
			return buf;
		}
	}
	return p->pw_name;
}

/*
 * uid_from_user()
 *      caches the uid for a given user name. We use a simple hash table.
 * Return
 *      the uid (if any) for a user name, or a -1 if no match can be found
 */
int
uid_from_user(const char *name, uid_t *uid)
{
	struct passwd *p = getpwnam(name);
	if (p == NULL) {
		return -1;
	}
	*uid = p->pw_uid;
	return *uid;
}

/*
 * gid_from_group()
 *      caches the gid for a given group name. We use a simple hash table.
 * Return
 *      the gid (if any) for a group name, or a -1 if no match can be found
 */
int
gid_from_group(const char *name, gid_t *gid)
{
	struct group *g = getgrnam(name);
	if (g == NULL) {
		return -1;
	}
	*gid = g->gr_gid;
	return *gid;
}
