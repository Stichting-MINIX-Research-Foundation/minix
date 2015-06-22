/* ptyfs.c - communication to PTYFS */

#include <minix/driver.h>
#include <minix/ds.h>

#include "ptyfs.h"

/*
 * Perform synchronous communication with PTYFS, if PTYFS is actually running.
 * This function is expected to return only once PTYFS has acknowledged
 * processing the request, in order to avoid race conditions between PTYFS and
 * userland.  The function must always fail when PTYFS is not available for any
 * reason.  Return OK on success, or an IPC-level error on failure.
 */
static int
ptyfs_sendrec(message * m_ptr)
{
	endpoint_t endpt;

	/*
	 * New pseudoterminals are created sufficiently rarely that we need not
	 * optimize this by for example caching the PTYFS endpoint,  especially
	 * since caching brings along new issues, such as having to reissue the
	 * request if the cached endpoint turns out to be outdated (e.g., when
	 * ptyfs is unmounted and remounted for whatever reason).
	 */
	if (ds_retrieve_label_endpt("ptyfs", &endpt) != OK)
		return EDEADSRCDST; /* ptyfs is not available */

	return ipc_sendrec(endpt, m_ptr);
}

/*
 * Add or update a node on PTYFS, with the given node index and attributes.
 * Return OK on success, or an error code on failure.  Errors may include
 * communication failures and out-of-memory conditions.
 */
int
ptyfs_set(unsigned int index, mode_t mode, uid_t uid, gid_t gid, dev_t dev)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));

	m.m_type = PTYFS_SET;
	m.m_pty_ptyfs_req.index = index;
	m.m_pty_ptyfs_req.mode = mode;
	m.m_pty_ptyfs_req.uid = uid;
	m.m_pty_ptyfs_req.gid = gid;
	m.m_pty_ptyfs_req.dev = dev;

	if ((r = ptyfs_sendrec(&m)) != OK)
		return r;

	return m.m_type;
}

/*
 * Remove a node from PTYFS.  Return OK on success, or an error code on
 * failure.  The function succeeds even if no node existed for the given index.
 */
int
ptyfs_clear(unsigned int index)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));

	m.m_type = PTYFS_CLEAR;
	m.m_pty_ptyfs_req.index = index;

	if ((r = ptyfs_sendrec(&m)) != OK)
		return r;

	return m.m_type;
}

/*
 * Obtain the file name for the PTYFS node with the given index, and store it
 * in the given 'name' buffer which consists of 'size' bytes.  On success,
 * return OK, with the file name stored as a null-terminated string.  The
 * returned name does not include the PTYFS mount path.  On failure, return an
 * error code.  Among other reasons, the function fails if no node is allocated
 * for the given index, and if the name does not fit in the given buffer.
 */
int
ptyfs_name(unsigned int index, char * name, size_t size)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));

	m.m_type = PTYFS_NAME;
	m.m_pty_ptyfs_req.index = index;

	if ((r = ptyfs_sendrec(&m)) != OK)
		return r;

	if (m.m_type != OK)
		return m.m_type;

	/* Ensure null termination, and make sure the string fits. */
	m.m_ptyfs_pty_name.name[sizeof(m.m_ptyfs_pty_name.name) - 1] = 0;
	if (strlen(m.m_ptyfs_pty_name.name) >= size)
		return ENAMETOOLONG;

	strlcpy(name, m.m_ptyfs_pty_name.name, size);
	return OK;
}
