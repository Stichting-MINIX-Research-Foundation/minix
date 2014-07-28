#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ucred.h>

/*
 * get the effective user ID and effective group ID of a peer
 * connected through a Unix domain socket.
 */
int getpeereid(int sd, uid_t *euid, gid_t *egid) {
	int rc;
	struct uucred cred;
	socklen_t ucred_length;

	/* Initialize Data Structures */
	ucred_length = sizeof(struct uucred);
	memset(&cred, '\0', ucred_length);

	/* Validate Input Parameters */
	if (euid == NULL || egid == NULL) {
		errno = EFAULT;
		return -1;
	} /* getsockopt will handle validating 'sd' */

	/* Get the credentials of the peer at the other end of 'sd' */
	rc = getsockopt(sd, SOL_SOCKET, SO_PEERCRED, &cred, &ucred_length);
	if (rc == 0) {
		/* Success - return the results */
		*euid = cred.cr_uid;
		*egid = cred.cr_gid;
		return 0;
	} else {
		/* Failure - getsockopt takes care of setting errno */
		return -1;
	}
}
