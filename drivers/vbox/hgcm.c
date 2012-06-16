/* VirtualBox driver - by D.C. van Moolenbroek */
#include <minix/drivers.h>
#include <minix/vboxtype.h>
#include <minix/vboxif.h>
#include <assert.h>

#include "vmmdev.h"
#include "proto.h"

#define MAX_CONNS	4	/* maximum number of HGCM connections */
#define MAX_REQS	2	/* number of concurrent requests per conn. */
#define MAX_PARAMS	8	/* maximum number of parameters per request */

/* HGCM connection states. */
enum {
  STATE_FREE,
  STATE_OPENING,
  STATE_OPEN,
  STATE_CLOSING
};

/* HGCM connection information. */
static struct {
  int state;					/* connection state */
  endpoint_t endpt;				/* caller endpoint */
  u32_t client_id;				/* VMMDev-given client ID */
  struct {
	int busy;				/* is this request ongoing? */
	struct VMMDevHGCMHeader *ptr;		/* request buffer */
	phys_bytes addr;			/* buffer's physical address */

	int status;				/* IPC status of request */
	long id;				/* request ID */

	cp_grant_id_t grant;			/* grant for parameters */
	int count;				/* number of parameters */
	vbox_param_t param[MAX_PARAMS];		/* local copy of parameters */
  } req[MAX_REQS];				/* concurrent requests */
} hgcm_conn[MAX_CONNS];

/*===========================================================================*
 *				convert_result				     *
 *===========================================================================*/
static int convert_result(int res)
{
	/* Convert a VirtualBox result code to a POSIX error code.
	 */

	/* HGCM transport error codes. */
	switch (res) {
	case VMMDEV_ERR_HGCM_NOT_FOUND:		return ESRCH;
	case VMMDEV_ERR_HGCM_DENIED:		return EPERM;
	case VMMDEV_ERR_HGCM_INVALID_ADDR:	return EFAULT;
	case VMMDEV_ERR_HGCM_ASYNC_EXEC:	return EDONTREPLY;
	case VMMDEV_ERR_HGCM_INTERNAL:		return EGENERIC;
	case VMMDEV_ERR_HGCM_INVALID_ID:	return EINVAL;
	}

	/* Positive codes are success codes. */
	if (res >= 0)
		return OK;

	/* Unsupported negative codes are translated to EGENERIC; it is up to
	 * the caller to check the actual VirtualBox result code in that case.
	 */
	return convert_err(res);
}

/*===========================================================================*
 *				send_reply				     *
 *===========================================================================*/
static void send_reply(endpoint_t endpt, int ipc_status, int result, int code,
	long id)
{
	/* Send a reply to an earlier request. */
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	m.m_type = VBOX_REPLY;
	m.VBOX_RESULT = result;
	m.VBOX_CODE = code;
	m.VBOX_ID = id;

	if (IPC_STATUS_CALL(ipc_status) == SENDREC)
		r = sendnb(endpt, &m);
	else
		r = asynsend3(endpt, &m, AMF_NOREPLY);

	if (r != OK)
		printf("VBOX: unable to send reply to %d: %d\n", endpt, r);
}

/*===========================================================================*
 *				alloc_req				     *
 *===========================================================================*/
static int alloc_req(int conn)
{
	/* Allocate a request for the given connection. Allocate memory as
	 * necessary. Do not mark the request as busy, as it may end up not
	 * being used.
	 */
	phys_bytes addr;
	void *ptr;
	int req;

	for (req = 0; req < MAX_REQS; req++)
		if (!hgcm_conn[conn].req[req].busy)
			break;

	if (req == MAX_REQS)
		return EMFILE;

	if (hgcm_conn[conn].req[req].ptr == NULL) {
		if ((ptr = alloc_contig(VMMDEV_BUF_SIZE, 0, &addr)) == NULL)
			return ENOMEM;

		hgcm_conn[conn].req[req].ptr = (struct VMMDevHGCMHeader *) ptr;
		hgcm_conn[conn].req[req].addr = addr;
	}

	return req;
}

/*===========================================================================*
 *				free_conn				     *
 *===========================================================================*/
static void free_conn(int conn)
{
	/* Free the memory for all requests of the given connections, and mark
	 * the connection as free.
	 */
	void *ptr;
	int req;

	for (req = 0; req < MAX_REQS; req++) {
		if ((ptr = (void *) hgcm_conn[conn].req[req].ptr) != NULL) {
			assert(!hgcm_conn[conn].req[req].busy);

			free_contig(ptr, VMMDEV_BUF_SIZE);

			hgcm_conn[conn].req[req].ptr = NULL;
		}
	}

	hgcm_conn[conn].state = STATE_FREE;
}

/*===========================================================================*
 *				start_req				     *
 *===========================================================================*/
static int start_req(int conn, int req, int type, size_t size, int ipc_status,
	long id, int *code)
{
	/* Start a request. */
	int r, res;

	hgcm_conn[conn].req[req].ptr->flags = 0;
	hgcm_conn[conn].req[req].ptr->result = VMMDEV_ERR_GENERIC;

	*code = res = vbox_request(&hgcm_conn[conn].req[req].ptr->header,
		hgcm_conn[conn].req[req].addr, type, size);

	r = convert_result(res);

	if (r != OK && r != EDONTREPLY)
		return r;

	/* The request may be processed either immediately or asynchronously.
	 * The caller of this function must be able to cope with both
	 * situations. In either case, mark the current request as ongoing.
	 */
	hgcm_conn[conn].req[req].busy = TRUE;
	hgcm_conn[conn].req[req].status = ipc_status;
	hgcm_conn[conn].req[req].id = id;

	return r;
}

/*===========================================================================*
 *				cancel_req				     *
 *===========================================================================*/
static void cancel_req(int conn, int req)
{
	/* Cancel an ongoing request. */

	assert(hgcm_conn[conn].req[req].ptr != NULL);

	/* The cancel request consists only of the HGCM header. The physical
	 * location determines the request to cancel. Note that request
	 * cancellation this is full of race conditions, so we simply ignore
	 * the return value and assumed all went well.
	 */
	hgcm_conn[conn].req[req].ptr->flags = 0;
	hgcm_conn[conn].req[req].ptr->result = VMMDEV_ERR_GENERIC;

	vbox_request(&hgcm_conn[conn].req[req].ptr->header,
		hgcm_conn[conn].req[req].addr, VMMDEV_REQ_HGCMCANCEL,
		sizeof(struct VMMDevHGCMCancel));

	hgcm_conn[conn].req[req].busy = FALSE;
}

/*===========================================================================*
 *				finish_req				     *
 *===========================================================================*/
static int finish_req(int conn, int req, int *code)
{
	/* The given request has finished. Take the appropriate action.
	 */
	struct VMMDevHGCMConnect *connreq;
	struct VMMDevHGCMCall *callreq;
	struct VMMDevHGCMParam *inp;
	vbox_param_t *outp;
	int i, count, res, r = OK;

	hgcm_conn[conn].req[req].busy = FALSE;

	*code = res = hgcm_conn[conn].req[req].ptr->result;

	r = convert_result(res);

	/* The request has finished, so it cannot still be in progress. */
	if (r == EDONTREPLY)
		r = EGENERIC;

	switch (hgcm_conn[conn].state) {
	case STATE_FREE:
		assert(0);

		break;

	case STATE_OPENING:
		if (r == OK) {
			connreq = (struct VMMDevHGCMConnect *)
				hgcm_conn[conn].req[req].ptr;
			hgcm_conn[conn].client_id = connreq->client_id;
			hgcm_conn[conn].state = STATE_OPEN;

			r = conn;
		} else {
			free_conn(conn);
		}

		break;

	case STATE_CLOSING:
		/* Neither we nor the caller can do anything with failures. */
		if (r != OK)
			printf("VBOX: disconnection failure #2 (%d)\n", res);

		free_conn(conn);

		r = OK;

		break;

	case STATE_OPEN:
		/* On success, extract and copy back parameters to the caller.
		 */
		if (r == OK) {
			callreq = (struct VMMDevHGCMCall *)
				hgcm_conn[conn].req[req].ptr;
			inp = (struct VMMDevHGCMParam *) &callreq[1];
			outp = &hgcm_conn[conn].req[req].param[0];
			count = hgcm_conn[conn].req[req].count;

			for (i = 0; i < count; i++) {
				switch (outp->type) {
				case VBOX_TYPE_U32:
					outp->u32 = inp->u32;
					break;

				case VBOX_TYPE_U64:
					outp->u64 = inp->u64;
					break;

				default:
					break;
				}

				inp++;
				outp++;
			}

			if (count > 0) {
				r = sys_safecopyto(hgcm_conn[conn].endpt,
					hgcm_conn[conn].req[req].grant, 0,
					(vir_bytes)
					hgcm_conn[conn].req[req].param,
					count * sizeof(vbox_param_t));
			}
		}

		break;
	}

	return r;
}

/*===========================================================================*
 *				check_conn				     *
 *===========================================================================*/
static void check_conn(int conn)
{
	/* Check all requests for the given connection for completion. */
	int r, req, code;

	for (req = 0; req < MAX_REQS; req++) {
		if (!hgcm_conn[conn].req[req].busy) continue;

		if (!(hgcm_conn[conn].req[req].ptr->flags &
				VMMDEV_HGCM_REQ_DONE))
			continue;

		r = finish_req(conn, req, &code);

		assert(r != EDONTREPLY);

		send_reply(hgcm_conn[conn].endpt,
			hgcm_conn[conn].req[req].status, r, code,
			hgcm_conn[conn].req[req].id);
	}
}

/*===========================================================================*
 *				do_open					     *
 *===========================================================================*/
static int do_open(message *m_ptr, int ipc_status, int *code)
{
	/* Process a connection request. */
	struct VMMDevHGCMConnect *connreq;
	int i, r, conn, count;

	if (m_ptr->VBOX_COUNT < 0 || m_ptr->VBOX_COUNT > VMMDEV_HGCM_NAME_SIZE)
		return EINVAL;

	/* Find a free connection slot. Make sure the sending endpoint is not
	 * already using up half of the connection slots.
	 */
	conn = -1;
	count = 0;
	for (i = 0; i < MAX_CONNS; i++) {
		if (conn < 0 && hgcm_conn[i].state == STATE_FREE)
			conn = i;
		if (hgcm_conn[i].endpt == m_ptr->m_source)
			count++;
	}

	if (count >= MAX(MAX_CONNS / 2, 2))
		return EMFILE;

	if (conn < 0)
		return ENFILE;

	/* Initialize the connection and request structures. */
	hgcm_conn[conn].state = STATE_OPENING;
	hgcm_conn[conn].endpt = m_ptr->m_source;

	for (i = 0; i < MAX_REQS; i++) {
		hgcm_conn[conn].req[i].busy = FALSE;
		hgcm_conn[conn].req[i].ptr = NULL;
	}

	/* Set up and start the connection request. */
	r = alloc_req(conn);

	if (r < 0)
		return r;
	assert(r == 0);

	connreq = (struct VMMDevHGCMConnect *) hgcm_conn[conn].req[0].ptr;
	connreq->type = VMMDEV_HGCM_SVCLOC_LOCALHOST_EXISTING;
	if ((r = sys_safecopyfrom(m_ptr->m_source, m_ptr->VBOX_GRANT, 0,
			(vir_bytes) connreq->name, m_ptr->VBOX_COUNT)) !=
			OK) {
		free_conn(conn);

		return r;
	}
	connreq->name[VMMDEV_HGCM_NAME_SIZE-1] = 0;

	r = start_req(conn, 0, VMMDEV_REQ_HGCMCONNECT, sizeof(*connreq),
		ipc_status, m_ptr->VBOX_ID, code);

	if (r != OK && r != EDONTREPLY) {
		free_conn(conn);

		return r;
	}

	return (r == OK) ? finish_req(conn, 0, code) : r;
}

/*===========================================================================*
 *				do_close				     *
 *===========================================================================*/
static int do_close(message *m_ptr, int ipc_status, int *code)
{
	/* Process a disconnection request. */
	struct VMMDevHGCMDisconnect *discreq;
	int r, conn, req;

	conn = m_ptr->VBOX_CONN;

	/* Sanity checks. */
	if (conn < 0 || conn >= MAX_CONNS)
		return EINVAL;
	if (hgcm_conn[conn].endpt != m_ptr->m_source ||
			hgcm_conn[conn].state != STATE_OPEN)
		return EINVAL;

	/* Cancel any ongoing requests. */
	for (req = 0; req < MAX_REQS; req++)
		if (hgcm_conn[conn].req[req].busy)
			cancel_req(conn, req);

	assert(hgcm_conn[conn].req[0].ptr != NULL);

	discreq = (struct VMMDevHGCMDisconnect *) hgcm_conn[conn].req[0].ptr;
	discreq->client_id = hgcm_conn[conn].client_id;

	r = start_req(conn, 0, VMMDEV_REQ_HGCMDISCONNECT, sizeof(*discreq),
		ipc_status, m_ptr->VBOX_ID, code);

	if (r != OK && r != EDONTREPLY) {
		/* Neither we nor the caller can do anything with failures. */
		printf("VBOX: disconnection failure #1 (%d)\n", r);

		free_conn(conn);

		return OK;
	}

	hgcm_conn[conn].state = STATE_CLOSING;

	return (r == OK) ? finish_req(conn, 0, code) : r;
}

/*===========================================================================*
 *				store_pages				     *
 *===========================================================================*/
static int store_pages(int conn, int req, vbox_param_t *inp, size_t *offp)
{
	/* Create a page list of physical pages that make up the provided
	 * buffer area.
	 */
	struct vumap_vir vvec;
	struct vumap_phys pvec[MAPVEC_NR];
	struct VMMDevHGCMPageList *pagelist;
	size_t offset, size, skip;
	int i, j, r, first, access, count, pages;

	/* Empty strings are allowed. */
	if (inp->ptr.size == 0)
		return OK;

	pagelist = (struct VMMDevHGCMPageList *)
		(((u8_t *) hgcm_conn[conn].req[req].ptr) + *offp);

	pagelist->flags = 0;
	if (inp->ptr.dir & VBOX_DIR_IN)
		pagelist->flags |= VMMDEV_HGCM_FLAG_FROM_HOST;
	if (inp->ptr.dir & VBOX_DIR_OUT)
		pagelist->flags |= VMMDEV_HGCM_FLAG_TO_HOST;
	pagelist->count = 0;

	/* Make sure there is room for the header (but no actual pages yet). */
	*offp += sizeof(*pagelist) - sizeof(pagelist->addr[0]);
	if (*offp > VMMDEV_BUF_SIZE)
		return ENOMEM;

	access = 0;
	if (inp->ptr.dir & VBOX_DIR_IN) access |= VUA_WRITE;
	if (inp->ptr.dir & VBOX_DIR_OUT) access |= VUA_READ;

	offset = 0;
	first = TRUE;
	do {
		/* If the caller gives us a huge buffer, we might need multiple
		 * calls to sys_vumap(). Note that the caller currently has no
		 * reliable way to know whether such a buffer will fit in our
		 * request page. In the future, we may dynamically reallocate
		 * the request area to make more room as necessary; for now we
		 * just return an ENOMEM error in such cases.
		 */
		vvec.vv_grant = inp->ptr.grant;
		vvec.vv_size = inp->ptr.off + inp->ptr.size;
		count = MAPVEC_NR;
		if ((r = sys_vumap(hgcm_conn[conn].endpt, &vvec, 1,
				inp->ptr.off + offset, access, pvec,
				&count)) != OK)
			return r;

		/* First get the number of bytes processed, before (possibly)
		 * adjusting the size of the first element.
		 */
		for (i = size = 0; i < count; i++)
			size += pvec[i].vp_size;

		/* VirtualBox wants aligned page addresses only, and an offset
		 * into the first page. All other pages except the last are
		 * full pages, and the last page is cut off using the size.
		 */
		skip = 0;
		if (first) {
			skip = pvec[0].vp_addr & (PAGE_SIZE - 1);
			pvec[0].vp_addr -= skip;
			pvec[0].vp_size += skip;
			pagelist->offset = skip;
			first = FALSE;
		}

		/* How many pages were mapped? */
		pages = (skip + size + PAGE_SIZE - 1) / PAGE_SIZE;

		/* Make sure there is room to store this many extra pages. */
		*offp += sizeof(pagelist->addr[0]) * pages;
		if (*offp > VMMDEV_BUF_SIZE)
			return ENOMEM;

		/* Actually store the pages in the page list. */
		for (i = j = 0; i < pages; i++) {
			assert(!(pvec[j].vp_addr & (PAGE_SIZE - 1)));

			pagelist->addr[pagelist->count++] =
				cvul64(pvec[j].vp_addr);

			if (pvec[j].vp_size > PAGE_SIZE) {
				pvec[j].vp_addr += PAGE_SIZE;
				pvec[j].vp_size -= PAGE_SIZE;
			}
			else j++;
		}
		assert(j == count);

		offset += size;
	} while (offset < inp->ptr.size);

	assert(offset == inp->ptr.size);

	return OK;
}

/*===========================================================================*
 *				do_call					     *
 *===========================================================================*/
static int do_call(message *m_ptr, int ipc_status, int *code)
{
	/* Perform a HGCM call. */
	vbox_param_t *inp;
	struct VMMDevHGCMParam *outp;
	struct VMMDevHGCMCall *callreq;
	size_t size;
	int i, r, conn, req, count;

	conn = m_ptr->VBOX_CONN;
	count = m_ptr->VBOX_COUNT;

	/* Sanity checks. */
	if (conn < 0 || conn >= MAX_CONNS)
		return EINVAL;
	if (hgcm_conn[conn].endpt != m_ptr->m_source ||
			hgcm_conn[conn].state != STATE_OPEN)
		return EINVAL;

	/* Allocate a request, and copy in the parameters. */
	req = alloc_req(conn);

	if (req < 0)
		return req;

	hgcm_conn[conn].req[req].grant = m_ptr->VBOX_GRANT;
	hgcm_conn[conn].req[req].count = count;

	if (count > 0) {
		if ((r = sys_safecopyfrom(m_ptr->m_source, m_ptr->VBOX_GRANT,
				0, (vir_bytes) hgcm_conn[conn].req[req].param,
				count * sizeof(vbox_param_t))) != OK)
			return r;
	}

	/* Set up the basic request. */
	callreq = (struct VMMDevHGCMCall *) hgcm_conn[conn].req[req].ptr;
	callreq->client_id = hgcm_conn[conn].client_id;
	callreq->function = m_ptr->VBOX_FUNCTION;
	callreq->count = count;

	/* Rewrite and convert the parameters. */
	inp = &hgcm_conn[conn].req[req].param[0];
	outp = (struct VMMDevHGCMParam *) &callreq[1];

	size = sizeof(*callreq) + sizeof(*outp) * count;
	assert(size < VMMDEV_BUF_SIZE);

	for (i = 0; i < count; i++) {
		switch (inp->type) {
		case VBOX_TYPE_U32:
			outp->type = VMMDEV_HGCM_PARAM_U32;
			outp->u32 = inp->u32;
			break;

		case VBOX_TYPE_U64:
			outp->type = VMMDEV_HGCM_PARAM_U64;
			outp->u64 = inp->u64;
			break;

		case VBOX_TYPE_PTR:
			outp->type = VMMDEV_HGCM_PARAM_PAGELIST;
			outp->pagelist.offset = size;
			outp->pagelist.size = inp->ptr.size;

			if ((r = store_pages(conn, req, inp, &size)) != OK)
				return r;

			break;

		default:
			return EINVAL;
		}

		inp++;
		outp++;
	}

	/* Start the request. */
	r = start_req(conn, req, VMMDEV_REQ_HGCMCALL, size, ipc_status,
		m_ptr->VBOX_ID, code);

	if (r != OK && r != EDONTREPLY)
		return r;

	return (r == OK) ? finish_req(conn, req, code) : r;
}

/*===========================================================================*
 *				do_cancel				     *
 *===========================================================================*/
static int do_cancel(message *m_ptr, int ipc_status)
{
	/* Cancel an ongoing call. */
	int conn, req;

	conn = m_ptr->VBOX_CONN;

	/* Sanity checks. Note that connection and disconnection requests
	 * cannot be cancelled.
	 */
	if (conn < 0 || conn >= MAX_CONNS)
		return EINVAL;
	if (hgcm_conn[conn].endpt != m_ptr->m_source ||
			hgcm_conn[conn].state != STATE_OPEN)
		return EINVAL;

	/* Find the request. */
	for (req = 0; req < MAX_REQS; req++) {
		if (hgcm_conn[conn].req[req].busy &&
				hgcm_conn[conn].req[req].id == m_ptr->VBOX_ID)
			break;
	}

	/* If no such request was ongoing, then our behavior depends on the
	 * way the request was made: we do not want to send two asynchronous
	 * replies for one request, but if the caller used SENDREC, we have to
	 * reply with something or the caller would deadlock.
	 */
	if (req == MAX_REQS) {
		if (IPC_STATUS_CALL(ipc_status) == SENDREC)
			return EINVAL;
		else
			return EDONTREPLY;
	}

	/* Actually cancel the request, and send a reply. */
	cancel_req(conn, req);

	return EINTR;
}

/*===========================================================================*
 *				hgcm_message				     *
 *===========================================================================*/
void hgcm_message(message *m_ptr, int ipc_status)
{
	/* Process a request message. */
	int r, code = VMMDEV_ERR_GENERIC;

	switch (m_ptr->m_type) {
	case VBOX_OPEN:		r = do_open(m_ptr, ipc_status, &code);	break;
	case VBOX_CLOSE:	r = do_close(m_ptr, ipc_status, &code);	break;
	case VBOX_CALL:		r = do_call(m_ptr, ipc_status, &code);	break;
	case VBOX_CANCEL:	r = do_cancel(m_ptr, ipc_status);	break;
	default:		r = ENOSYS;				break;
	}

	if (r != EDONTREPLY)
		send_reply(m_ptr->m_source, ipc_status, r, code,
			m_ptr->VBOX_ID);
}

/*===========================================================================*
 *				hgcm_intr				     *
 *===========================================================================*/
void hgcm_intr(void)
{
	/* We received an HGCM event. Check ongoing requests for completion. */
	int conn;

	for (conn = 0; conn < MAX_CONNS; conn++)
		if (hgcm_conn[conn].state != STATE_FREE)
			check_conn(conn);
}
