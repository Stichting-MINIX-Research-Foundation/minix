/* Test for sys_vumap() - by D.C. van Moolenbroek */
#include <minix/drivers.h>
#include <minix/ds.h>
#include <sys/mman.h>
#include <assert.h>

#include "com.h"

struct buf {
	int pages;
	int flags;
	vir_bytes addr;
	phys_bytes phys;
};
#define BUF_PREALLOC	0x1	/* if set, immediately allocate the page */
#define BUF_ADJACENT	0x2	/* virtually contiguous with the last buffer */

static unsigned int count = 0, failures = 0;

static int success;
static char *fail_file;
static int fail_line;

static int relay;
static endpoint_t endpt;

static int verbose;

static enum {
	GE_NONE,		/* no exception */
	GE_REVOKED,		/* revoked grant */
	GE_INVALID		/* invalid grant */
} grant_exception = GE_NONE;

static int grant_access = 0;

#define expect(r)	expect_f((r), __FILE__, __LINE__)

static void alloc_buf(struct buf *buf, phys_bytes next)
{
	void *tmp = NULL;
	vir_bytes addr;
	size_t len;
	int r, prealloc, flags;

	/* is_allocated() cannot handle buffers that are not physically
	 * contiguous, and we cannot guarantee physical contiguity if not
	 * not preallocating.
	 */
	assert((buf->flags & BUF_PREALLOC) || buf->pages == 1);

	len = buf->pages * PAGE_SIZE;
	prealloc = (buf->flags & BUF_PREALLOC);
	flags = MAP_ANON | (prealloc ? (MAP_CONTIG | MAP_PREALLOC) : 0);

	if (prealloc) {
		/* Allocate a same-sized piece of memory elsewhere, to make it
		 * very unlikely that the actual piece of memory will end up
		 * being physically contiguous with the last piece.
		 */
		tmp = mmap((void *) (buf->addr + len + PAGE_SIZE), len,
			PROT_READ | PROT_WRITE, MAP_ANON | MAP_PREALLOC |
			MAP_CONTIG, -1, 0L);

		if (tmp == MAP_FAILED)
			panic("unable to allocate temporary buffer");
	}

	addr = (vir_bytes) mmap((void *) buf->addr, len,
		PROT_READ | PROT_WRITE, flags, -1, 0L);

	if (addr != buf->addr)
		panic("unable to allocate buffer (2)");

	if (!prealloc)
		return;

	if ((r = munmap(tmp, len)) != OK)
		panic("unable to unmap buffer (%d)", errno);

	if ((r = sys_umap(SELF, VM_D, addr, len, &buf->phys)) < 0)
		panic("unable to get physical address of buffer (%d)", r);

	if (buf->phys != next)
		return;

	if (verbose)
		printf("WARNING: alloc noncontigous range, second try\n");

	/* Can't remap this to elsewhere, so we run the risk of allocating the
	 * exact same physically contiguous page again. However, now that we've
	 * unmapped the temporary memory also, there's a small chance we'll end
	 * up with a different physical page this time. Who knows.
	 */
	munmap((void *) addr, len);

	addr = (vir_bytes) mmap((void *) buf->addr, len,
		PROT_READ | PROT_WRITE, flags, -1, 0L);

	if (addr != buf->addr)
		panic("unable to allocate buffer, second try");

	if ((r = sys_umap(SELF, VM_D, addr, len, &buf->phys)) < 0)
		panic("unable to get physical address of buffer (%d)", r);

	/* Still the same page? Screw it. */
	if (buf->phys == next)
		panic("unable to allocate noncontiguous range");
}

static void alloc_bufs(struct buf *buf, int count)
{
	static vir_bytes base = 0x80000000L;
	phys_bytes next;
	int i;

	/* Allocate the given memory in virtually contiguous blocks whenever
	 * each next buffer is requested to be adjacent. Insert a virtual gap
	 * after each such block. Make sure that each two adjacent buffers in a
	 * block are physically non-contiguous.
	 */
	for (i = 0; i < count; i++) {
		if (i > 0 && (buf[i].flags & BUF_ADJACENT)) {
			next = buf[i-1].phys + buf[i-1].pages * PAGE_SIZE;
		} else {
			base += PAGE_SIZE * 16;
			next = 0L;
		}

		buf[i].addr = base;

		alloc_buf(&buf[i], next);

		base += buf[i].pages * PAGE_SIZE;
	}

#if DEBUG
	for (i = 0; i < count; i++)
		printf("Buf %d: %d pages, flags %x, vir %08x, phys %08x\n", i,
			buf[i].pages, buf[i].flags, buf[i].addr, buf[i].phys);
#endif
}

static void free_bufs(struct buf *buf, int count)
{
	int i, j, r;

	for (i = 0; i < count; i++) {
		for (j = 0; j < buf[i].pages; j++) {
			r = munmap((void *) (buf[i].addr + j * PAGE_SIZE),
				PAGE_SIZE);

			if (r != OK)
				panic("unable to unmap range (%d)", errno);
		}
	}
}

static int is_allocated(vir_bytes addr, size_t bytes, phys_bytes *phys)
{
	int r;

	/* This will have to do for now. Of course, we could use sys_vumap with
	 * VUA_READ for this, but that would defeat the point of one test. It
	 * is still a decent alternative in case sys_umap's behavior ever
	 * changes, though.
	 */
	r = sys_umap(SELF, VM_D, addr, bytes, phys);

	return r == OK;
}

static int is_buf_allocated(struct buf *buf)
{
	return is_allocated(buf->addr, buf->pages * PAGE_SIZE, &buf->phys);
}

static void test_group(char *name)
{
	if (verbose)
		printf("Test group: %s (%s)\n",
			name, relay ? "relay" : "local");
}

static void expect_f(int res, char *file, int line)
{
	if (!res && success) {
		success = FALSE;
		fail_file = file;
		fail_line = line;
	}
}

static void got_result(char *desc)
{
	count++;

	if (!success) {
		failures++;

		printf("#%02d: %-38s\t[FAIL]\n", count, desc);
		printf("- failure at %s:%d\n", fail_file, fail_line);
	} else {
		if (verbose)
			printf("#%02d: %-38s\t[PASS]\n", count, desc);
	}
}

static int relay_vumap(struct vumap_vir *vvec, int vcount, size_t offset,
	int access, struct vumap_phys *pvec, int *pcount)
{
	struct vumap_vir gvvec[MAPVEC_NR + 3];
	cp_grant_id_t vgrant, pgrant;
	message m;
	int i, r, gaccess;

	assert(vcount > 0 && vcount <= MAPVEC_NR + 3);
	assert(*pcount > 0 && *pcount <= MAPVEC_NR + 3);

	/* Allow grant access flags to be overridden for testing purposes. */
	if (!(gaccess = grant_access)) {
		if (access & VUA_READ) gaccess |= CPF_READ;
		if (access & VUA_WRITE) gaccess |= CPF_WRITE;
	}

	for (i = 0; i < vcount; i++) {
		gvvec[i].vv_grant = cpf_grant_direct(endpt, vvec[i].vv_addr,
			vvec[i].vv_size, gaccess);
		assert(gvvec[i].vv_grant != GRANT_INVALID);
		gvvec[i].vv_size = vvec[i].vv_size;
	}

	vgrant = cpf_grant_direct(endpt, (vir_bytes) gvvec,
		sizeof(gvvec[0]) * vcount, CPF_READ);
	assert(vgrant != GRANT_INVALID);

	pgrant = cpf_grant_direct(endpt, (vir_bytes) pvec,
		sizeof(pvec[0]) * *pcount, CPF_WRITE);
	assert(pgrant != GRANT_INVALID);

	/* This must be done after allocating all other grants. */
	if (grant_exception != GE_NONE) {
		cpf_revoke(gvvec[vcount - 1].vv_grant);
		if (grant_exception == GE_INVALID)
			gvvec[vcount - 1].vv_grant = GRANT_INVALID;
	}

	m.m_type = VTR_RELAY;
	m.VTR_VGRANT = vgrant;
	m.VTR_VCOUNT = vcount;
	m.VTR_OFFSET = offset;
	m.VTR_ACCESS = access;
	m.VTR_PGRANT = pgrant;
	m.VTR_PCOUNT = *pcount;

	r = ipc_sendrec(endpt, &m);

	cpf_revoke(pgrant);
	cpf_revoke(vgrant);

	for (i = 0; i < vcount - !!grant_exception; i++)
		cpf_revoke(gvvec[i].vv_grant);

	*pcount = m.VTR_PCOUNT;

	return (r != OK) ? r : m.m_type;
}

static int do_vumap(endpoint_t endpt, struct vumap_vir *vvec, int vcount,
	size_t offset, int access, struct vumap_phys *pvec, int *pcount)
{
	struct vumap_phys pv_backup[MAPVEC_NR + 3];
	int r, pc_backup, pv_test = FALSE;

	/* Make a copy of pvec and pcount for later. */
	pc_backup = *pcount;

	/* We cannot compare pvec contents before and after when relaying,
	 * since the original contents are not transferred.
	 */
	if (!relay && pvec != NULL && pc_backup >= 1 &&
			pc_backup <= MAPVEC_NR + 3) {
		pv_test = TRUE;
		memcpy(pv_backup, pvec, sizeof(*pvec) * pc_backup);
	}

	/* Reset the test result. */
	success = TRUE;

	/* Perform the vumap call, either directly or through a relay. */
	if (relay) {
		assert(endpt == SELF);
		r = relay_vumap(vvec, vcount, offset, access, pvec, pcount);
	} else {
		r = sys_vumap(endpt, vvec, vcount, offset, access, pvec,
			pcount);
	}

	/* Upon failure, pvec and pcount must be unchanged. */
	if (r != OK) {
		expect(pc_backup == *pcount);

		if (pv_test)
			expect(memcmp(pv_backup, pvec,
				sizeof(*pvec) * pc_backup) == 0);
	}

	return r;
}

static void test_basics(void)
{
	struct vumap_vir vvec[2];
	struct vumap_phys pvec[4];
	struct buf buf[4];
	int r, pcount;

	test_group("basics");

	buf[0].pages = 1;
	buf[0].flags = BUF_PREALLOC;
	buf[1].pages = 2;
	buf[1].flags = BUF_PREALLOC;
	buf[2].pages = 1;
	buf[2].flags = BUF_PREALLOC;
	buf[3].pages = 1;
	buf[3].flags = BUF_PREALLOC | BUF_ADJACENT;

	alloc_bufs(buf, 4);

	/* Test single whole page. */
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE;
	pcount = 1;

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_addr == buf[0].phys);
	expect(pvec[0].vp_size == vvec[0].vv_size);

	got_result("single whole page");

	/* Test single partial page. */
	vvec[0].vv_addr = buf[0].addr + 123;
	vvec[0].vv_size = PAGE_SIZE - 456;
	pcount = 1;

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_addr == buf[0].phys + 123);
	expect(pvec[0].vp_size == vvec[0].vv_size);

	got_result("single partial page");

	/* Test multiple contiguous whole pages. */
	vvec[0].vv_addr = buf[1].addr;
	vvec[0].vv_size = PAGE_SIZE * 2;
	pcount = 1;

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_addr == buf[1].phys);
	expect(pvec[0].vp_size == vvec[0].vv_size);

	got_result("multiple contiguous whole pages");

	/* Test range in multiple contiguous pages. */
	vvec[0].vv_addr = buf[1].addr + 234;
	vvec[0].vv_size = PAGE_SIZE * 2 - 234;
	pcount = 2;

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_addr == buf[1].phys + 234);
	expect(pvec[0].vp_size == vvec[0].vv_size);

	got_result("range in multiple contiguous pages");

	/* Test multiple noncontiguous whole pages. */
	vvec[0].vv_addr = buf[2].addr;
	vvec[0].vv_size = PAGE_SIZE * 2;
	pcount = 3;

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 2);
	expect(pvec[0].vp_addr == buf[2].phys);
	expect(pvec[0].vp_size == PAGE_SIZE);
	expect(pvec[1].vp_addr == buf[3].phys);
	expect(pvec[1].vp_size == PAGE_SIZE);

	got_result("multiple noncontiguous whole pages");

	/* Test range in multiple noncontiguous pages. */
	vvec[0].vv_addr = buf[2].addr + 1;
	vvec[0].vv_size = PAGE_SIZE * 2 - 2;
	pcount = 2;

	r = do_vumap(SELF, vvec, 1, 0, VUA_WRITE, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 2);
	expect(pvec[0].vp_addr == buf[2].phys + 1);
	expect(pvec[0].vp_size == PAGE_SIZE - 1);
	expect(pvec[1].vp_addr == buf[3].phys);
	expect(pvec[1].vp_size == PAGE_SIZE - 1);

	got_result("range in multiple noncontiguous pages");

	/* Test single-input result truncation. */
	vvec[0].vv_addr = buf[2].addr + PAGE_SIZE / 2;
	vvec[0].vv_size = PAGE_SIZE;
	pvec[1].vp_addr = 0L;
	pvec[1].vp_size = 0;
	pcount = 1;

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_addr == buf[2].phys + PAGE_SIZE / 2);
	expect(pvec[0].vp_size == PAGE_SIZE / 2);
	expect(pvec[1].vp_addr == 0L);
	expect(pvec[1].vp_size == 0);

	got_result("single-input result truncation");

	/* Test multiple inputs, contiguous first. */
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE;
	vvec[1].vv_addr = buf[2].addr + PAGE_SIZE - 1;
	vvec[1].vv_size = 2;
	pcount = 3;

	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 3);
	expect(pvec[0].vp_addr == buf[0].phys);
	expect(pvec[0].vp_size == PAGE_SIZE);
	expect(pvec[1].vp_addr == buf[2].phys + PAGE_SIZE - 1);
	expect(pvec[1].vp_size == 1);
	expect(pvec[2].vp_addr == buf[3].phys);
	expect(pvec[2].vp_size == 1);

	got_result("multiple inputs, contiguous first");

	/* Test multiple inputs, contiguous last. */
	vvec[0].vv_addr = buf[2].addr + 123;
	vvec[0].vv_size = PAGE_SIZE * 2 - 456;
	vvec[1].vv_addr = buf[1].addr + 234;
	vvec[1].vv_size = PAGE_SIZE * 2 - 345;
	pcount = 4;

	r = do_vumap(SELF, vvec, 2, 0, VUA_WRITE, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 3);
	expect(pvec[0].vp_addr == buf[2].phys + 123);
	expect(pvec[0].vp_size == PAGE_SIZE - 123);
	expect(pvec[1].vp_addr == buf[3].phys);
	expect(pvec[1].vp_size == PAGE_SIZE - (456 - 123));
	expect(pvec[2].vp_addr == buf[1].phys + 234);
	expect(pvec[2].vp_size == vvec[1].vv_size);

	got_result("multiple inputs, contiguous last");

	/* Test multiple-inputs result truncation. */
	vvec[0].vv_addr = buf[2].addr + 2;
	vvec[0].vv_size = PAGE_SIZE * 2 - 3;
	vvec[1].vv_addr = buf[0].addr;
	vvec[1].vv_size = 135;
	pvec[2].vp_addr = 0xDEADBEEFL;
	pvec[2].vp_size = 1234;
	pcount = 2;

	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 2);
	expect(pvec[0].vp_addr == buf[2].phys + 2);
	expect(pvec[0].vp_size == PAGE_SIZE - 2);
	expect(pvec[1].vp_addr == buf[3].phys);
	expect(pvec[1].vp_size == PAGE_SIZE - 1);
	expect(pvec[2].vp_addr == 0xDEADBEEFL);
	expect(pvec[2].vp_size == 1234);

	got_result("multiple-inputs result truncation");

	free_bufs(buf, 4);
}

static void test_endpt(void)
{
	struct vumap_vir vvec[1];
	struct vumap_phys pvec[1];
	struct buf buf[1];
	int r, pcount;

	test_group("endpoint");

	buf[0].pages = 1;
	buf[0].flags = BUF_PREALLOC;

	alloc_bufs(buf, 1);

	/* Test NONE endpoint. */
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE;
	pcount = 1;

	r = do_vumap(NONE, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == EINVAL);

	got_result("NONE endpoint");

	/* Test ANY endpoint. */
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE;
	pcount = 1;

	r = do_vumap(ANY, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == EINVAL);

	got_result("ANY endpoint");

	free_bufs(buf, 1);
}

static void test_vector1(void)
{
	struct vumap_vir vvec[2];
	struct vumap_phys pvec[3];
	struct buf buf[2];
	int r, pcount;

	test_group("vector, part 1");

	buf[0].pages = 2;
	buf[0].flags = BUF_PREALLOC;
	buf[1].pages = 1;
	buf[1].flags = BUF_PREALLOC;

	alloc_bufs(buf, 2);

	/* Test zero virtual memory size. */
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE * 2;
	vvec[1].vv_addr = buf[1].addr;
	vvec[1].vv_size = 0;
	pcount = 3;

	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == EINVAL);

	got_result("zero virtual memory size");

	/* Test excessive virtual memory size. */
	vvec[1].vv_size = (vir_bytes) -1;

	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == EFAULT || r == EPERM);

	got_result("excessive virtual memory size");

	/* Test invalid virtual memory. */
	vvec[1].vv_addr = 0L;
	vvec[1].vv_size = PAGE_SIZE;

	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == EFAULT);

	got_result("invalid virtual memory");

	/* Test virtual memory overrun. */
	vvec[0].vv_size++;
	vvec[1].vv_addr = buf[1].addr;

	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == EFAULT);

	got_result("virtual memory overrun");

	free_bufs(buf, 2);
}

static void test_vector2(void)
{
	struct vumap_vir vvec[2], *vvecp;
	struct vumap_phys pvec[3], *pvecp;
	struct buf buf[2];
	phys_bytes dummy;
	int r, pcount;

	test_group("vector, part 2");

	buf[0].pages = 2;
	buf[0].flags = BUF_PREALLOC;
	buf[1].pages = 1;
	buf[1].flags = BUF_PREALLOC;

	alloc_bufs(buf, 2);

	/* Test zero virtual count. */
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE * 2;
	vvec[1].vv_addr = buf[1].addr;
	vvec[1].vv_size = PAGE_SIZE;
	pcount = 3;

	r = do_vumap(SELF, vvec, 0, 0, VUA_READ, pvec, &pcount);

	expect(r == EINVAL);

	got_result("zero virtual count");

	/* Test negative virtual count. */
	r = do_vumap(SELF, vvec, -1, 0, VUA_WRITE, pvec, &pcount);

	expect(r == EINVAL);

	got_result("negative virtual count");

	/* Test zero physical count. */
	pcount = 0;

	r = do_vumap(SELF, vvec, 2, 0, VUA_WRITE, pvec, &pcount);

	expect(r == EINVAL);

	got_result("zero physical count");

	/* Test negative physical count. */
	pcount = -1;

	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == EINVAL);

	got_result("negative physical count");

	/* Test invalid virtual vector pointer. */
	pcount = 2;

	r = do_vumap(SELF, NULL, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == EFAULT);

	got_result("invalid virtual vector pointer");

	/* Test unallocated virtual vector. */
	vvecp = (struct vumap_vir *) mmap(NULL, PAGE_SIZE,
		PROT_READ | PROT_WRITE, MAP_ANON, -1, 0L);

	if (vvecp == MAP_FAILED)
		panic("unable to allocate virtual vector");

	r = do_vumap(SELF, vvecp, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == EFAULT);
	expect(!is_allocated((vir_bytes) vvecp, PAGE_SIZE, &dummy));

	got_result("unallocated virtual vector pointer");

	munmap((void *) vvecp, PAGE_SIZE);

	/* Test invalid physical vector pointer. */
	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, NULL, &pcount);

	expect(r == EFAULT);

	got_result("invalid physical vector pointer");

	/* Test unallocated physical vector. */
	pvecp = (struct vumap_phys *) mmap(NULL, PAGE_SIZE,
		PROT_READ | PROT_WRITE, MAP_ANON, -1, 0L);

	if (pvecp == MAP_FAILED)
		panic("unable to allocate physical vector");

	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, pvecp, &pcount);

	expect(r == OK);
	expect(is_allocated((vir_bytes) pvecp, PAGE_SIZE, &dummy));
	expect(pcount == 2);
	expect(pvecp[0].vp_size == PAGE_SIZE * 2);
	expect(pvecp[0].vp_addr == buf[0].phys);
	expect(pvecp[1].vp_size == PAGE_SIZE);
	expect(pvecp[1].vp_addr == buf[1].phys);

	got_result("unallocated physical vector pointer");

	munmap((void *) pvecp, PAGE_SIZE);

	free_bufs(buf, 2);
}

static void test_grant(void)
{
	struct vumap_vir vvec[2];
	struct vumap_phys pvec[3];
	struct buf buf[2];
	int r, pcount;

	test_group("grant");

	buf[0].pages = 1;
	buf[0].flags = BUF_PREALLOC;
	buf[1].pages = 2;
	buf[1].flags = BUF_PREALLOC;

	alloc_bufs(buf, 2);

	/* Test write-only access on read-only grant. */
	grant_access = CPF_READ; /* override */

	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE;
	pcount = 1;

	r = do_vumap(SELF, vvec, 1, 0, VUA_WRITE, pvec, &pcount);

	expect(r == EPERM);

	got_result("write-only access on read-only grant");

	/* Test read-write access on read-only grant. */
	r = do_vumap(SELF, vvec, 1, 0, VUA_READ | VUA_WRITE, pvec, &pcount);

	expect(r == EPERM);

	got_result("read-write access on read-only grant");

	/* Test read-only access on write-only grant. */
	grant_access = CPF_WRITE; /* override */

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == EPERM);

	got_result("read-only access on write-only grant");

	/* Test read-write access on write grant. */
	r = do_vumap(SELF, vvec, 1, 0, VUA_READ | VUA_WRITE, pvec, &pcount);

	expect(r == EPERM);

	got_result("read-write access on write-only grant");

	/* Test read-only access on read-write grant. */
	grant_access = CPF_READ | CPF_WRITE; /* override */

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_size == PAGE_SIZE);
	expect(pvec[0].vp_addr == buf[0].phys);

	got_result("read-only access on read-write grant");

	grant_access = 0; /* reset */

	/* Test invalid grant. */
	grant_exception = GE_INVALID;

	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE;
	vvec[1].vv_addr = buf[1].addr;
	vvec[1].vv_size = PAGE_SIZE * 2;
	pcount = 3;

	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == EINVAL);

	got_result("invalid grant");

	/* Test revoked grant. */
	grant_exception = GE_REVOKED;

	r = do_vumap(SELF, vvec, 2, 0, VUA_READ, pvec, &pcount);

	expect(r == EPERM);

	got_result("revoked grant");

	grant_exception = GE_NONE;

	free_bufs(buf, 2);
}

static void test_offset(void)
{
	struct vumap_vir vvec[2];
	struct vumap_phys pvec[3];
	struct buf buf[4];
	size_t off, off2;
	int r, pcount;

	test_group("offsets");

	buf[0].pages = 1;
	buf[0].flags = BUF_PREALLOC;
	buf[1].pages = 2;
	buf[1].flags = BUF_PREALLOC;
	buf[2].pages = 1;
	buf[2].flags = BUF_PREALLOC;
	buf[3].pages = 1;
	buf[3].flags = BUF_PREALLOC | BUF_ADJACENT;

	alloc_bufs(buf, 4);

	/* Test offset into aligned page. */
	off = 123;
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE;
	pcount = 2;

	r = do_vumap(SELF, vvec, 1, off, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_addr == buf[0].phys + off);
	expect(pvec[0].vp_size == vvec[0].vv_size - off);

	got_result("offset into aligned page");

	/* Test offset into unaligned page. */
	off2 = 456;
	assert(off + off2 < PAGE_SIZE);
	vvec[0].vv_addr = buf[0].addr + off;
	vvec[0].vv_size = PAGE_SIZE - off;
	pcount = 2;

	r = do_vumap(SELF, vvec, 1, off2, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_addr == buf[0].phys + off + off2);
	expect(pvec[0].vp_size == vvec[0].vv_size - off2);

	got_result("offset into unaligned page");

	/* Test offset into unaligned page set. */
	off = 1234;
	off2 = 567;
	assert(off + off2 < PAGE_SIZE);
	vvec[0].vv_addr = buf[1].addr + off;
	vvec[0].vv_size = (PAGE_SIZE - off) * 2;
	pcount = 3;

	r = do_vumap(SELF, vvec, 1, off2, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_addr == buf[1].phys + off + off2);
	expect(pvec[0].vp_size == vvec[0].vv_size - off2);

	got_result("offset into contiguous page set");

	/* Test offset into noncontiguous page set. */
	vvec[0].vv_addr = buf[2].addr + off;
	vvec[0].vv_size = (PAGE_SIZE - off) * 2;
	pcount = 3;

	r = do_vumap(SELF, vvec, 1, off2, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 2);
	expect(pvec[0].vp_addr == buf[2].phys + off + off2);
	expect(pvec[0].vp_size == PAGE_SIZE - off - off2);
	expect(pvec[1].vp_addr == buf[3].phys);
	expect(pvec[1].vp_size == PAGE_SIZE - off);

	got_result("offset into noncontiguous page set");

	/* Test offset to last byte. */
	off = PAGE_SIZE - off2 - 1;
	vvec[0].vv_addr = buf[0].addr + off2;
	vvec[0].vv_size = PAGE_SIZE - off2;
	pcount = 2;

	r = do_vumap(SELF, vvec, 1, off, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_addr == buf[0].phys + off + off2);
	expect(pvec[0].vp_size == 1);

	got_result("offset to last byte");

	/* Test offset at range end. */
	off = 234;
	vvec[0].vv_addr = buf[1].addr + off;
	vvec[0].vv_size = PAGE_SIZE - off * 2;
	vvec[1].vv_addr = vvec[0].vv_addr + vvec[0].vv_size;
	vvec[1].vv_size = off;

	r = do_vumap(SELF, vvec, 2, vvec[0].vv_size, VUA_READ, pvec, &pcount);

	expect(r == EINVAL);

	got_result("offset at range end");

	/* Test offset beyond range end. */
	vvec[0].vv_addr = buf[1].addr;
	vvec[0].vv_size = PAGE_SIZE;
	vvec[1].vv_addr = buf[1].addr + PAGE_SIZE;
	vvec[1].vv_size = PAGE_SIZE;

	r = do_vumap(SELF, vvec, 2, PAGE_SIZE + off, VUA_READ, pvec, &pcount);

	expect(r == EINVAL);

	got_result("offset beyond range end");

	/* Test negative offset. */
	vvec[0].vv_addr = buf[1].addr + off + off2;
	vvec[0].vv_size = PAGE_SIZE;

	r = do_vumap(SELF, vvec, 1, (size_t) -1, VUA_READ, pvec, &pcount);

	expect(r == EINVAL);

	got_result("negative offset");

	free_bufs(buf, 4);
}

static void test_access(void)
{
	struct vumap_vir vvec[3];
	struct vumap_phys pvec[4], *pvecp;
	struct buf buf[7];
	int i, r, pcount, pindex;

	test_group("access");

	buf[0].pages = 1;
	buf[0].flags = 0;
	buf[1].pages = 1;
	buf[1].flags = BUF_PREALLOC | BUF_ADJACENT;
	buf[2].pages = 1;
	buf[2].flags = BUF_ADJACENT;

	alloc_bufs(buf, 3);

	/* Test no access flags. */
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE * 3;
	pcount = 4;

	r = do_vumap(SELF, vvec, 1, 0, 0, pvec, &pcount);

	expect(r == EINVAL);
	expect(!is_buf_allocated(&buf[0]));
	expect(is_buf_allocated(&buf[1]));
	expect(!is_buf_allocated(&buf[2]));

	got_result("no access flags");

	/* Test read-only access. */
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE * 3;
	pcount = 1;

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == EFAULT);
	expect(!is_buf_allocated(&buf[0]));
	expect(is_buf_allocated(&buf[1]));
	expect(!is_buf_allocated(&buf[2]));

	got_result("read-only access");

	/* Test read-write access. */
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE * 3;
	pcount = 4;

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ | VUA_WRITE, pvec, &pcount);

	expect(r == EFAULT);
	expect(!is_buf_allocated(&buf[0]));
	expect(is_buf_allocated(&buf[1]));
	expect(!is_buf_allocated(&buf[2]));

	got_result("read-write access");

	/* Test write-only access. */
	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = PAGE_SIZE * 3;
	pcount = 4;

	r = do_vumap(SELF, vvec, 1, 0, VUA_WRITE, pvec, &pcount);

	expect(r == OK);
	/* We don't control the physical addresses of the faulted-in pages, so
	 * they may or may not end up being contiguous with their neighbours.
	 */
	expect(pcount >= 1 && pcount <= 3);
	expect(is_buf_allocated(&buf[0]));
	expect(is_buf_allocated(&buf[1]));
	expect(is_buf_allocated(&buf[2]));
	expect(pvec[0].vp_addr == buf[0].phys);
	switch (pcount) {
	case 1:
		expect(pvec[0].vp_size == PAGE_SIZE * 3);
		break;
	case 2:
		expect(pvec[0].vp_size + pvec[1].vp_size == PAGE_SIZE * 3);
		if (pvec[0].vp_size > PAGE_SIZE)
			expect(pvec[1].vp_addr == buf[2].phys);
		else
			expect(pvec[1].vp_addr == buf[1].phys);
		break;
	case 3:
		expect(pvec[0].vp_size == PAGE_SIZE);
		expect(pvec[1].vp_addr == buf[1].phys);
		expect(pvec[1].vp_size == PAGE_SIZE);
		expect(pvec[2].vp_addr == buf[2].phys);
		expect(pvec[2].vp_size == PAGE_SIZE);
		break;
	}

	got_result("write-only access");

	free_bufs(buf, 3);

	/* Test page faulting. */
	buf[0].pages = 1;
	buf[0].flags = 0;
	buf[1].pages = 1;
	buf[1].flags = BUF_PREALLOC | BUF_ADJACENT;
	buf[2].pages = 1;
	buf[2].flags = 0;
	buf[3].pages = 2;
	buf[3].flags = BUF_PREALLOC;
	buf[4].pages = 1;
	buf[4].flags = BUF_ADJACENT;
	buf[5].pages = 1;
	buf[5].flags = BUF_ADJACENT;
	buf[6].pages = 1;
	buf[6].flags = 0;

	alloc_bufs(buf, 7);

	vvec[0].vv_addr = buf[0].addr + PAGE_SIZE - 1;
	vvec[0].vv_size = PAGE_SIZE - 1;
	vvec[1].vv_addr = buf[2].addr;
	vvec[1].vv_size = PAGE_SIZE;
	vvec[2].vv_addr = buf[3].addr + 123;
	vvec[2].vv_size = PAGE_SIZE * 4 - 456;
	pvecp = (struct vumap_phys *) buf[6].addr;
	pcount = 7;
	assert(sizeof(struct vumap_phys) * pcount <= PAGE_SIZE);

	r = do_vumap(SELF, vvec, 3, 0, VUA_WRITE, pvecp, &pcount);

	expect(r == OK);
	/* Same story but more possibilities. I hope I got this right. */
	expect(pcount >= 3 || pcount <= 6);
	for (i = 0; i < 7; i++)
		expect(is_buf_allocated(&buf[i]));
	expect(pvecp[0].vp_addr = buf[0].phys);
	if (pvecp[0].vp_size == 1) {
		expect(pvecp[1].vp_addr == buf[1].phys);
		expect(pvecp[1].vp_size == PAGE_SIZE - 2);
		pindex = 2;
	} else {
		expect(pvecp[0].vp_size == PAGE_SIZE - 1);
		pindex = 1;
	}
	expect(pvecp[pindex].vp_addr == buf[2].phys);
	expect(pvecp[pindex].vp_size == PAGE_SIZE);
	pindex++;
	expect(pvecp[pindex].vp_addr == buf[3].phys + 123);
	switch (pcount - pindex) {
	case 1:
		expect(pvecp[pindex].vp_size == PAGE_SIZE * 4 - 456);
		break;
	case 2:
		if (pvecp[pindex].vp_size > PAGE_SIZE * 2 - 123) {
			expect(pvecp[pindex].vp_size == PAGE_SIZE * 3 - 123);
			expect(pvecp[pindex + 1].vp_addr == buf[5].phys);
			expect(pvecp[pindex + 1].vp_size ==
				PAGE_SIZE - (456 - 123));
		} else {
			expect(pvecp[pindex].vp_size == PAGE_SIZE * 2 - 123);
			expect(pvecp[pindex + 1].vp_addr == buf[4].phys);
			expect(pvecp[pindex + 1].vp_size ==
				PAGE_SIZE * 2 - (456 - 123));
		}
		break;
	case 3:
		expect(pvecp[pindex].vp_size == PAGE_SIZE * 2 - 123);
		expect(pvecp[pindex + 1].vp_addr == buf[4].phys);
		expect(pvecp[pindex + 1].vp_size == PAGE_SIZE);
		expect(pvecp[pindex + 2].vp_addr == buf[5].phys);
		expect(pvecp[pindex + 2].vp_size == PAGE_SIZE - (456 - 123));
		break;
	default:
		expect(0);
	}

	got_result("page faulting");

	free_bufs(buf, 7);

	/* MISSING: tests to see whether a request with VUA_WRITE or
	 * (VUA_READ|VUA_WRITE) correctly gets an EFAULT for a read-only page.
	 * As of writing, support for such protection is missing from the
	 * system at all.
	 */
}

static void phys_limit(struct vumap_vir *vvec, int vcount,
	struct vumap_phys *pvec, int pcount, struct buf *buf, char *desc)
{
	int i, r;

	r = do_vumap(SELF, vvec, vcount, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == MAPVEC_NR);
	for (i = 0; i < MAPVEC_NR; i++) {
		expect(pvec[i].vp_addr == buf[i].phys);
		expect(pvec[i].vp_size == PAGE_SIZE);
	}

	got_result(desc);
}

static void test_limits(void)
{
	struct vumap_vir vvec[MAPVEC_NR + 3];
	struct vumap_phys pvec[MAPVEC_NR + 3];
	struct buf buf[MAPVEC_NR + 9];
	int i, r, vcount, pcount, nr_bufs;

	test_group("limits");

	/* Test large contiguous range. */
	buf[0].pages = MAPVEC_NR + 2;
	buf[0].flags = BUF_PREALLOC;

	alloc_bufs(buf, 1);

	vvec[0].vv_addr = buf[0].addr;
	vvec[0].vv_size = (MAPVEC_NR + 2) * PAGE_SIZE;
	pcount = 2;

	r = do_vumap(SELF, vvec, 1, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == 1);
	expect(pvec[0].vp_addr == buf[0].phys);
	expect(pvec[0].vp_size == vvec[0].vv_size);

	got_result("large contiguous range");

	free_bufs(buf, 1);

	/* I'd like to test MAPVEC_NR contiguous ranges of MAPVEC_NR pages
	 * each, but chances are we don't have that much contiguous memory
	 * available at all. In fact, the previous test may already fail
	 * because of this..
	 */

	for (i = 0; i < MAPVEC_NR + 2; i++) {
		buf[i].pages = 1;
		buf[i].flags = BUF_PREALLOC;
	}
	buf[i].pages = 1;
	buf[i].flags = BUF_PREALLOC | BUF_ADJACENT;

	alloc_bufs(buf, MAPVEC_NR + 3);

	/* Test virtual limit, one below. */
	for (i = 0; i < MAPVEC_NR + 2; i++) {
		vvec[i].vv_addr = buf[i].addr;
		vvec[i].vv_size = PAGE_SIZE;
	}
	vvec[i - 1].vv_size += PAGE_SIZE;

	pcount = MAPVEC_NR + 3;

	r = do_vumap(SELF, vvec, MAPVEC_NR - 1, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == MAPVEC_NR - 1);
	for (i = 0; i < MAPVEC_NR - 1; i++) {
		expect(pvec[i].vp_addr == buf[i].phys);
		expect(pvec[i].vp_size == PAGE_SIZE);
	}

	got_result("virtual limit, one below");

	/* Test virtual limit, exact match. */
	pcount = MAPVEC_NR + 3;

	r = do_vumap(SELF, vvec, MAPVEC_NR, 0, VUA_WRITE, pvec, &pcount);

	expect(r == OK);
	expect(pcount == MAPVEC_NR);
	for (i = 0; i < MAPVEC_NR; i++) {
		expect(pvec[i].vp_addr == buf[i].phys);
		expect(pvec[i].vp_size == PAGE_SIZE);
	}

	got_result("virtual limit, exact match");

	/* Test virtual limit, one above. */
	pcount = MAPVEC_NR + 3;

	r = do_vumap(SELF, vvec, MAPVEC_NR + 1, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == MAPVEC_NR);
	for (i = 0; i < MAPVEC_NR; i++) {
		expect(pvec[i].vp_addr == buf[i].phys);
		expect(pvec[i].vp_size == PAGE_SIZE);
	}

	got_result("virtual limit, one above");

	/* Test virtual limit, two above. */
	pcount = MAPVEC_NR + 3;

	r = do_vumap(SELF, vvec, MAPVEC_NR + 2, 0, VUA_WRITE, pvec, &pcount);

	expect(r == OK);
	expect(pcount == MAPVEC_NR);
	for (i = 0; i < MAPVEC_NR; i++) {
		expect(pvec[i].vp_addr == buf[i].phys);
		expect(pvec[i].vp_size == PAGE_SIZE);
	}

	got_result("virtual limit, two above");

	/* Test physical limit, one below, aligned. */
	pcount = MAPVEC_NR - 1;

	r = do_vumap(SELF, vvec + 2, MAPVEC_NR, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == MAPVEC_NR - 1);
	for (i = 0; i < MAPVEC_NR - 1; i++) {
		expect(pvec[i].vp_addr == buf[i + 2].phys);
		expect(pvec[i].vp_size == PAGE_SIZE);
	}

	got_result("physical limit, one below, aligned");

	/* Test physical limit, one below, unaligned. */
	pcount = MAPVEC_NR - 1;

	r = do_vumap(SELF, vvec + 3, MAPVEC_NR, 0, VUA_READ, pvec, &pcount);

	expect(r == OK);
	expect(pcount == MAPVEC_NR - 1);
	for (i = 0; i < MAPVEC_NR - 1; i++) {
		expect(pvec[i].vp_addr == buf[i + 3].phys);
		expect(pvec[i].vp_size == PAGE_SIZE);
	}

	got_result("physical limit, one below, unaligned");

	free_bufs(buf, MAPVEC_NR + 3);

	nr_bufs = sizeof(buf) / sizeof(buf[0]);

	/* This ends up looking in our virtual address space as follows:
	 * [P] [P] [P] [PPP] [PPP] ...(MAPVEC_NR x [PPP])... [PPP]
	 * ..where P is a page, and the blocks are virtually contiguous.
	 */
	for (i = 0; i < nr_bufs; i += 3) {
		buf[i].pages = 1;
		buf[i].flags = BUF_PREALLOC;
		buf[i + 1].pages = 1;
		buf[i + 1].flags =
			BUF_PREALLOC | ((i >= 3) ? BUF_ADJACENT : 0);
		buf[i + 2].pages = 1;
		buf[i + 2].flags =
			BUF_PREALLOC | ((i >= 3) ? BUF_ADJACENT : 0);
	}

	alloc_bufs(buf, nr_bufs);

	for (i = 0; i < 3; i++) {
		vvec[i].vv_addr = buf[i].addr;
		vvec[i].vv_size = PAGE_SIZE;
	}
	for ( ; i < nr_bufs / 3 + 1; i++) {
		vvec[i].vv_addr = buf[(i - 2) * 3].addr;
		vvec[i].vv_size = PAGE_SIZE * 3;
	}
	vcount = i;

	/* Out of each of the following tests, one will be aligned (that is,
	 * the last pvec entry will be for the last page in a vvec entry) and
	 * two will be unaligned.
	 */

	/* Test physical limit, exact match. */
	phys_limit(vvec, vcount, pvec, MAPVEC_NR, buf,
		"physical limit, exact match, try 1");
	phys_limit(vvec + 1, vcount - 1, pvec, MAPVEC_NR, buf + 1,
		"physical limit, exact match, try 2");
	phys_limit(vvec + 2, vcount - 2, pvec, MAPVEC_NR, buf + 2,
		"physical limit, exact match, try 3");

	/* Test physical limit, one above. */
	phys_limit(vvec, vcount, pvec, MAPVEC_NR + 1, buf,
		"physical limit, one above, try 1");
	phys_limit(vvec + 1, vcount - 1, pvec, MAPVEC_NR + 1, buf + 1,
		"physical limit, one above, try 2");
	phys_limit(vvec + 2, vcount - 2, pvec, MAPVEC_NR + 1, buf + 2,
		"physical limit, one above, try 3");

	/* Test physical limit, two above. */
	phys_limit(vvec, vcount, pvec, MAPVEC_NR + 2, buf,
		"physical limit, two above, try 1");
	phys_limit(vvec + 1, vcount - 1, pvec, MAPVEC_NR + 2, buf + 1,
		"physical limit, two above, try 2");
	phys_limit(vvec + 2, vcount - 2, pvec, MAPVEC_NR + 2, buf + 2,
		"physical limit, two above, try 3");

	free_bufs(buf, nr_bufs);
}

static void do_tests(int use_relay)
{
	relay = use_relay;

	test_basics();

	if (!relay) test_endpt();	/* local only */

	test_vector1();

	if (!relay) test_vector2();	/* local only */

	if (relay) test_grant();	/* remote only */

	test_offset();

	test_access();

	test_limits();
}

static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	int r;

	verbose = (env_argc > 1 && !strcmp(env_argv[1], "-v"));

	if (verbose)
		printf("Starting sys_vumap test set\n");

	do_tests(FALSE /*use_relay*/);

	if ((r = ds_retrieve_label_endpt("vumaprelay", &endpt)) != OK)
		panic("unable to obtain endpoint for 'vumaprelay' (%d)", r);

	do_tests(TRUE /*use_relay*/);

	if (verbose)
		printf("Completed sys_vumap test set, %u/%u tests failed\n",
			failures, count);

	/* The returned code will determine the outcome of the RS call, and
	 * thus the entire test. The actual error code does not matter.
	 */
	return (failures) ? EINVAL : OK;
}

static void sef_local_startup(void)
{
	sef_setcb_init_fresh(sef_cb_init_fresh);

	sef_startup();
}

int main(int argc, char **argv)
{
	env_setargs(argc, argv);

	sef_local_startup();

	return 0;
}
