/*
 * Unix Domain Sockets Implementation (PF_UNIX, PF_LOCAL)
 * This code provides communication stubs for backcalls to VFS.
 */

#include "uds.h"

/*
 * Check the permissions of a socket file.
 */
int
vfs_check_perms(endpoint_t ep, struct sockaddr_un *addr)
{
	int rc;
	message m;
	cp_grant_id_t grant_id;

	dprintf(("UDS: vfs_check_perms(%d)\n", ep));

	grant_id = cpf_grant_direct(VFS_PROC_NR, (vir_bytes) addr->sun_path,
	    UNIX_PATH_MAX, CPF_READ | CPF_WRITE);

	/* Ask VFS to verify the permissions. */
	memset(&m, '\0', sizeof(message));

	m.m_type = VFS_UDS_CHECK_PERMS;
	m.VFS_UDS_ENDPT = ep;
	m.VFS_UDS_GRANT = grant_id;
	m.VFS_UDS_COUNT = UNIX_PATH_MAX;

	if ((rc = sendrec(VFS_PROC_NR, &m)) != OK)
                panic("error sending to VFS: %d\n", rc);

	cpf_revoke(grant_id);

	dprintf(("UDS: VFS reply => %d, \"%s\"\n", m.m_type,
	    addr->sun_path));

	return m.m_type; /* reply code: OK, ELOOP, etc. */
}

/*
 * Verify whether the given file descriptor is valid for the given process, and
 * obtain a filp object identifier upon success.
 */
int
vfs_verify_fd(endpoint_t ep, int fd, filp_id_t *filp)
{
	int rc;
	message m;

	dprintf(("UDS: vfs_verify_fd(%d, %d)\n", ep, fd));

	memset(&m, '\0', sizeof(message));

	m.m_type = VFS_UDS_VERIFY_FD;
	m.VFS_UDS_ENDPT = ep;
	m.VFS_UDS_FD = fd;

	if ((rc = sendrec(VFS_PROC_NR, &m)) != OK)
                panic("error sending to VFS: %d\n", rc);

	dprintf(("UDS: VFS reply => %d, %p\n", m.m_type, m.VFS_UDS_FILP));

	if (m.m_type != OK)
		return m.m_type;

	*filp = m.VFS_UDS_FILP;
	return OK;
}

/*
 * Mark a filp object as in flight, that is, in use by UDS.
 */
int
vfs_set_filp(filp_id_t sfilp)
{
	int rc;
	message m;

	dprintf(("UDS: set_filp(%p)\n", sfilp));

	memset(&m, '\0', sizeof(message));

	m.m_type = VFS_UDS_SET_FILP;
	m.VFS_UDS_FILP = sfilp;

	if ((rc = sendrec(VFS_PROC_NR, &m)) != OK)
                panic("error sending to VFS: %d\n", rc);

	dprintf(("UDS: VFS reply => %d\n", m.m_type));

	return m.m_type; /* reply code: OK, ELOOP, etc. */
}

/*
 * Copy a filp object into a process, yielding a file descriptor.
 */
int
vfs_copy_filp(endpoint_t to_ep, filp_id_t cfilp)
{
	int rc;
	message m;

	dprintf(("UDS: vfs_copy_filp(%d, %p)\n", to_ep, cfilp));

	memset(&m, '\0', sizeof(message));

	m.m_type = VFS_UDS_COPY_FILP;
	m.VFS_UDS_ENDPT = to_ep;
	m.VFS_UDS_FILP = cfilp;

	if ((rc = sendrec(VFS_PROC_NR, &m)) != OK)
                panic("error sending to VFS: %d\n", rc);

	dprintf(("UDS: VFS reply => %d\n", m.m_type));

	return m.m_type;
}

/*
 * Mark a filp object as no longer in flight.
 */
int
vfs_put_filp(filp_id_t pfilp)
{
	int rc;
	message m;

	dprintf(("UDS: vfs_put_filp(%p)\n", pfilp));

	memset(&m, '\0', sizeof(message));

	m.m_type = VFS_UDS_PUT_FILP;
	m.VFS_UDS_FILP = pfilp;

	if ((rc = sendrec(VFS_PROC_NR, &m)) != OK)
                panic("error sending to VFS: %d\n", rc);

	dprintf(("UDS: VFS reply => %d\n", m.m_type));

	return m.m_type; /* reply code: OK, ELOOP, etc. */
}

/*
 * Undo creation of a file descriptor in a process.
 */
int
vfs_cancel_fd(endpoint_t ep, int fd)
{
	int rc;
	message m;

	dprintf(("UDS: vfs_cancel_fd(%d,%d)\n", ep, fd));

	memset(&m, '\0', sizeof(message));

	m.m_type = VFS_UDS_CANCEL_FD;
	m.VFS_UDS_ENDPT = ep;
	m.VFS_UDS_FD = fd;

	if ((rc = sendrec(VFS_PROC_NR, &m)) != OK)
                panic("error sending to VFS: %d\n", rc);

	dprintf(("UDS: VFS reply => %d\n", m.m_type));

	return m.m_type; /* reply code: OK, ELOOP, etc. */
}
