/* Block Device Driver Test driver, by D.C. van Moolenbroek */
#include <stdlib.h>
#include <minix/blockdriver.h>
#include <minix/drvlib.h>
#include <minix/ds.h>
#include <minix/optset.h>
#include <sys/ioc_disk.h>
#include <assert.h>

enum {
	RESULT_OK,			/* exactly as expected */
	RESULT_DEATH,			/* driver died */
	RESULT_COMMFAIL,		/* communication failed */
	RESULT_BADTYPE,			/* bad type in message */
	RESULT_BADID,			/* bad request ID in message */
	RESULT_BADSTATUS,		/* bad/unexpected status in message */
	RESULT_TRUNC,			/* request truncated unexpectedly */
	RESULT_CORRUPT,			/* buffer touched erroneously */
	RESULT_MISSING,			/* buffer left untouched erroneously */
	RESULT_OVERFLOW,		/* area around buffer touched */
	RESULT_BADVALUE			/* bad/unexpected return value */
};

typedef struct {
	int type;
	ssize_t value;
} result_t;

static char driver_label[32] = "";	/* driver DS label */
static dev_t driver_minor = -1;	/* driver's partition minor to use */
static endpoint_t driver_endpt;	/* driver endpoint */

static int may_write = FALSE;		/* may we write to the device? */
static int sector_size = 512;		/* size of a single disk sector */
static int min_read = 512;		/* minimum total size of read req */
static int element_size = 512;		/* minimum I/O vector element size */
static int max_size = 131072;		/* maximum total size of any req */
/* Note that we do not test exceeding the max_size limit, so it is safe to set
 * it to a value lower than the driver supports.
 */

static struct partition part;		/* base and size of target partition */

#define NR_OPENED 10			/* maximum number of opened devices */
static dev_t opened[NR_OPENED];	/* list of currently opened devices */
static int nr_opened = 0;		/* current number of opened devices */

static int total_tests = 0;		/* total number of tests performed */
static int failed_tests = 0;		/* number of tests that failed */
static int failed_groups = 0;		/* nr of groups that had failures */
static int group_failure;		/* has this group had a failure yet? */
static int driver_deaths = 0;		/* number of restarts that we saw */

/* Options supported by this driver. */
static struct optset optset_table[] = {
	{ "label",	OPT_STRING,	driver_label,	sizeof(driver_label) },
	{ "minor",	OPT_INT,	&driver_minor,	10		     },
	{ "rw",		OPT_BOOL,	&may_write,	TRUE		     },
	{ "ro",		OPT_BOOL,	&may_write,	FALSE		     },
	{ "sector",	OPT_INT,	&sector_size,	10		     },
	{ "element",	OPT_INT,	&element_size,	10		     },
	{ "min_read",	OPT_INT,	&min_read,	10		     },
	{ "max",	OPT_INT,	&max_size,	10		     },
	{ NULL,		0,		NULL,		0		     }
};

static int set_result(result_t *res, int type, ssize_t value)
{
	/* Set the result to the given result type and with the given optional
	 * extra value. Return the type.
	 */
	res->type = type;
	res->value = value;

	return type;
}

static int accept_result(result_t *res, int type, ssize_t value)
{
	/* If the result is of the given type and value, reset it to a success
	 * result. This allows for a logical OR on error codes. Return whether
	 * the result was indeed reset.
	 */

	if (res->type == type && res->value == value) {
		set_result(res, RESULT_OK, 0);

		return TRUE;
	}

	return FALSE;
}

static void got_result(result_t *res, char *desc)
{
	/* Process the result of a test. Keep statistics.
	 */
	static int i = 0;

	total_tests++;
	if (res->type != RESULT_OK) {
		failed_tests++;

		if (group_failure == FALSE) {
			failed_groups++;
			group_failure = TRUE;
		}
	}

	printf("#%02d: %-38s\t[%s]\n", ++i, desc,
		(res->type == RESULT_OK) ? "PASS" : "FAIL");

	switch (res->type) {
	case RESULT_DEATH:
		printf("- driver died\n");
		break;
	case RESULT_COMMFAIL:
		printf("- communication failed; sendrec returned %d\n",
			res->value);
		break;
	case RESULT_BADTYPE:
		printf("- bad type %d in reply message\n", res->value);
		break;
	case RESULT_BADID:
		printf("- mismatched ID %d in reply message\n", res->value);
		break;
	case RESULT_BADSTATUS:
		printf("- bad or unexpected status %d in reply message\n",
			res->value);
		break;
	case RESULT_TRUNC:
		printf("- result size not as expected (%u bytes left)\n",
			res->value);
		break;
	case RESULT_CORRUPT:
		printf("- buffer has been modified erroneously\n");
		break;
	case RESULT_MISSING:
		printf("- buffer has been left untouched erroneously\n");
		break;
	case RESULT_OVERFLOW:
		printf("- area around target buffer modified\n");
		break;
	case RESULT_BADVALUE:
		printf("- bad or unexpected return value %d from call\n",
			res->value);
		break;
	}
}

static void test_group(char *name, int exec)
{
	/* Start a new group of tests.
	 */

	printf("Test group: %s%s\n", name, exec ? "" : " (skipping)");

	group_failure = FALSE;
}

static void reopen_device(dev_t minor)
{
	/* Reopen a device after we were notified that the driver has died.
	 * Explicitly ignore any errors here; this is a feeble attempt to get
	 * ourselves back into business again.
	 */
	message m;

	memset(&m, 0, sizeof(m));
	m.m_type = BDEV_OPEN;
	m.BDEV_MINOR = minor;
	m.BDEV_ACCESS = (may_write) ? (R_BIT | W_BIT) : R_BIT;
	m.BDEV_ID = 0;

	(void) sendrec(driver_endpt, &m);
}

static int sendrec_driver(message *m_ptr, ssize_t exp, result_t *res)
{
	/* Make a call to the driver, and perform basic checks on the return
	 * message. Fill in the result structure, wiping out what was in there
	 * before. If the driver dies in the process, attempt to recover but
	 * fail the request.
	 */
	message m_orig;
	endpoint_t last_endpt;
	int i, r;

	m_orig = *m_ptr;

	r = sendrec(driver_endpt, m_ptr);

	if (r == EDEADSRCDST) {
		/* The driver has died. Find its new endpoint, and reopen all
		 * devices that we opened earlier. Then return failure.
		 */
		printf("WARNING: driver has died, attempting to proceed\n");

		driver_deaths++;

		/* Keep trying until we get a new endpoint. */
		last_endpt = driver_endpt;
		for (;;) {
			r = ds_retrieve_label_endpt(driver_label,
				&driver_endpt);

			if (r == OK && last_endpt != driver_endpt)
				break;

			micro_delay(100000);
		}

		for (i = 0; i < nr_opened; i++)
			reopen_device(opened[i]);

		return set_result(res, RESULT_DEATH, 0);
	}

	if (r != OK)
		return set_result(res, RESULT_COMMFAIL, r);

	if (m_ptr->m_type != BDEV_REPLY)
		return set_result(res, RESULT_BADTYPE, m_ptr->m_type);

	if (m_ptr->BDEV_ID != m_orig.BDEV_ID)
		return set_result(res, RESULT_BADID, m_ptr->BDEV_ID);

	if ((exp < 0 && m_ptr->BDEV_STATUS >= 0) ||
			(exp >= 0 && m_ptr->BDEV_STATUS < 0))
		return set_result(res, RESULT_BADSTATUS, m_ptr->BDEV_STATUS);

	return set_result(res, RESULT_OK, 0);
}

static void raw_xfer(dev_t minor, u64_t pos, iovec_s_t *iovec, int nr_req,
	int write, ssize_t exp, result_t *res)
{
	/* Perform a transfer with a safecopy iovec already supplied.
	 */
	cp_grant_id_t grant;
	message m;
	int r;

	assert(nr_req <= NR_IOREQS);
	assert(!write || may_write);

	if ((grant = cpf_grant_direct(driver_endpt, (vir_bytes) iovec,
			sizeof(*iovec) * nr_req, CPF_READ)) == GRANT_INVALID)
		panic("unable to allocate grant");

	memset(&m, 0, sizeof(m));
	m.m_type = write ? BDEV_SCATTER : BDEV_GATHER;
	m.BDEV_MINOR = minor;
	m.BDEV_POS_LO = ex64lo(pos);
	m.BDEV_POS_HI = ex64hi(pos);
	m.BDEV_COUNT = nr_req;
	m.BDEV_GRANT = grant;
	m.BDEV_ID = lrand48();

	r = sendrec_driver(&m, exp, res);

	if (cpf_revoke(grant) != OK)
		panic("unable to revoke grant");

	if (r != RESULT_OK)
		return;

	if (m.BDEV_STATUS == exp)
		return;

	if (exp < 0)
		set_result(res, RESULT_BADSTATUS, m.BDEV_STATUS);
	else
		set_result(res, RESULT_TRUNC, exp - m.BDEV_STATUS);
}

static void vir_xfer(dev_t minor, u64_t pos, iovec_t *iovec, int nr_req,
	int write, ssize_t exp, result_t *res)
{
	/* Perform a transfer, creating and revoking grants for the I/O vector.
	 */
	iovec_s_t iov_s[NR_IOREQS];
	int i;

	assert(nr_req <= NR_IOREQS);

	for (i = 0; i < nr_req; i++) {
		iov_s[i].iov_size = iovec[i].iov_size;

		if ((iov_s[i].iov_grant = cpf_grant_direct(driver_endpt,
			(vir_bytes) iovec[i].iov_addr, iovec[i].iov_size,
			write ? CPF_READ : CPF_WRITE)) == GRANT_INVALID)
			panic("unable to allocate grant");
	}

	raw_xfer(minor, pos, iov_s, nr_req, write, exp, res);

	for (i = 0; i < nr_req; i++) {
		iovec[i].iov_size = iov_s[i].iov_size;

		if (cpf_revoke(iov_s[i].iov_grant) != OK)
			panic("unable to revoke grant");
	}
}

static void simple_xfer(dev_t minor, u64_t pos, u8_t *buf, size_t size,
	int write, ssize_t exp, result_t *res)
{
	/* Perform a transfer involving a single buffer.
	 */
	iovec_t iov;

	iov.iov_addr = (vir_bytes) buf;
	iov.iov_size = size;

	vir_xfer(minor, pos, &iov, 1, write, exp, res);
}

static void alloc_buf_and_grant(u8_t **ptr, cp_grant_id_t *grant,
	size_t size, int perms)
{
	/* Allocate a buffer suitable for DMA (i.e. contiguous) and create a
	 * grant for it with the requested CPF_* grant permissions.
	 */

	*ptr = alloc_contig(size, 0, NULL);
	if (*ptr == NULL)
		panic("unable to allocate memory");

	if ((*grant = cpf_grant_direct(driver_endpt, (vir_bytes) *ptr, size,
			perms)) == GRANT_INVALID)
		panic("unable to allocate grant");
}

static void free_buf_and_grant(u8_t *ptr, cp_grant_id_t grant, size_t size)
{
	/* Revoke a grant and free a buffer.
	 */

	cpf_revoke(grant);

	free_contig(ptr, size);
}

static void bad_read1(void)
{
	/* Test various illegal read transfer requests, part 1.
	 */
	message mt, m;
	iovec_s_t iovt, iov;
	cp_grant_id_t grant, grant2, grant3;
	u8_t *buf_ptr;
	vir_bytes buf_size;
	result_t res;

	test_group("bad read requests, part one", TRUE);

#define BUF_SIZE	4096
	buf_size = BUF_SIZE;

	alloc_buf_and_grant(&buf_ptr, &grant2, buf_size, CPF_WRITE);

	if ((grant = cpf_grant_direct(driver_endpt, (vir_bytes) &iov,
			sizeof(iov), CPF_READ)) == GRANT_INVALID)
		panic("unable to allocate grant");

	/* Initialize the defaults for some of the tests.
	 * This is a legitimate request for the first block of the partition.
	 */
	memset(&mt, 0, sizeof(mt));
	mt.m_type = BDEV_GATHER;
	mt.BDEV_MINOR = driver_minor;
	mt.BDEV_POS_LO = 0L;
	mt.BDEV_POS_HI = 0L;
	mt.BDEV_COUNT = 1;
	mt.BDEV_GRANT = grant;
	mt.BDEV_ID = lrand48();

	memset(&iovt, 0, sizeof(iovt));
	iovt.iov_grant = grant2;
	iovt.iov_size = buf_size;

	/* Test normal request. */
	m = mt;
	iov = iovt;

	sendrec_driver(&m, OK, &res);

	if (res.type == RESULT_OK && m.BDEV_STATUS != (ssize_t) iov.iov_size) {
		res.type = RESULT_TRUNC;
		res.value = m.BDEV_STATUS;
	}

	got_result(&res, "normal request");

	/* Test zero iovec elements. */
	m = mt;
	iov = iovt;

	m.BDEV_COUNT = 0;

	sendrec_driver(&m, EINVAL, &res);

	got_result(&res, "zero iovec elements");

	/* Test bad iovec grant. */
	m = mt;

	m.BDEV_GRANT = GRANT_INVALID;

	sendrec_driver(&m, EINVAL, &res);

	got_result(&res, "bad iovec grant");

	/* Test revoked iovec grant. */
	m = mt;
	iov = iovt;

	if ((grant3 = cpf_grant_direct(driver_endpt, (vir_bytes) &iov,
			sizeof(iov), CPF_READ)) == GRANT_INVALID)
		panic("unable to allocate grant");

	cpf_revoke(grant3);

	m.BDEV_GRANT = grant3;

	sendrec_driver(&m, EINVAL, &res);

	accept_result(&res, RESULT_BADSTATUS, EPERM);

	got_result(&res, "revoked iovec grant");

	/* Test normal request (final check). */
	m = mt;
	iov = iovt;

	sendrec_driver(&m, OK, &res);

	if (res.type == RESULT_OK && m.BDEV_STATUS != (ssize_t) iov.iov_size) {
		res.type = RESULT_TRUNC;
		res.value = m.BDEV_STATUS;
	}

	got_result(&res, "normal request");

	/* Clean up. */
	free_buf_and_grant(buf_ptr, grant2, buf_size);

	cpf_revoke(grant);
}

static u32_t get_sum(u8_t *ptr, size_t size)
{
	/* Compute a checksum over the given buffer.
	 */
	u32_t sum;

	for (sum = 0; size > 0; size--, ptr++)
		sum = sum ^ (sum << 5) ^ *ptr;

	return sum;
}

static u32_t fill_rand(u8_t *ptr, size_t size)
{
	/* Fill the given buffer with random data. Return a checksum over the
	 * resulting data.
	 */
	size_t i;

	for (i = 0; i < size; i++)
		ptr[i] = lrand48() % 256;

	return get_sum(ptr, size);
}

static void test_sum(u8_t *ptr, size_t size, u32_t sum, int should_match,
	result_t *res)
{
	/* If the test succeeded so far, check whether the given buffer does
	 * or does not match the given checksum, and adjust the test result
	 * accordingly.
	 */
	u32_t sum2;

	if (res->type != RESULT_OK)
		return;

	sum2 = get_sum(ptr, size);

	if ((sum == sum2) != should_match) {
		res->type = should_match ? RESULT_CORRUPT : RESULT_MISSING;
		res->value = 0;		/* not much that's useful here */
	}
}

static void bad_read2(void)
{
	/* Test various illegal read transfer requests, part 2.
	 *
	 * Consider allowing this test to be run twice, with different buffer
	 * sizes. It appears that we can make at_wini misbehave by making the
	 * size exceed the per-operation size (128KB ?). On the other hand, we
	 * then need to start checking partition sizes, possibly.
	 */
	u8_t *buf_ptr, *buf2_ptr, *buf3_ptr, c1, c2;
	size_t buf_size, buf2_size, buf3_size;
	cp_grant_id_t buf_grant, buf2_grant, buf3_grant, grant;
	u32_t buf_sum, buf2_sum, buf3_sum;
	iovec_s_t iov[3], iovt[3];
	result_t res;

	test_group("bad read requests, part two", TRUE);

	buf_size = buf2_size = buf3_size = BUF_SIZE;

	alloc_buf_and_grant(&buf_ptr, &buf_grant, buf_size, CPF_WRITE);
	alloc_buf_and_grant(&buf2_ptr, &buf2_grant, buf2_size, CPF_WRITE);
	alloc_buf_and_grant(&buf3_ptr, &buf3_grant, buf3_size, CPF_WRITE);

	iovt[0].iov_grant = buf_grant;
	iovt[0].iov_size = buf_size;
	iovt[1].iov_grant = buf2_grant;
	iovt[1].iov_size = buf2_size;
	iovt[2].iov_grant = buf3_grant;
	iovt[2].iov_size = buf3_size;

	/* Test normal vector request. */
	memcpy(iov, iovt, sizeof(iovt));

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE,
		buf_size + buf2_size + buf3_size, &res);

	test_sum(buf_ptr, buf_size, buf_sum, FALSE, &res);
	test_sum(buf2_ptr, buf2_size, buf2_sum, FALSE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, FALSE, &res);

	got_result(&res, "normal vector request");

	/* Test zero sized iovec element. */
	memcpy(iov, iovt, sizeof(iovt));
	iov[1].iov_size = 0;

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE, EINVAL, &res);

	test_sum(buf_ptr, buf_size, buf_sum, TRUE, &res);
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "zero size in iovec element");

	/* Test negative sized iovec element. */
	memcpy(iov, iovt, sizeof(iovt));
	iov[1].iov_size = (vir_bytes) LONG_MAX + 1;

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE, EINVAL, &res);

	test_sum(buf_ptr, buf_size, buf_sum, TRUE, &res);
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "negative size in iovec element");

	/* Test iovec with negative total size. */
	memcpy(iov, iovt, sizeof(iovt));
	iov[0].iov_size = LONG_MAX / 2 - 1;
	iov[1].iov_size = LONG_MAX / 2 - 1;

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE, EINVAL, &res);

	test_sum(buf_ptr, buf_size, buf_sum, TRUE, &res);
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "negative total size");

	/* Test iovec with wrapping total size. */
	memcpy(iov, iovt, sizeof(iovt));
	iov[0].iov_size = LONG_MAX - 1;
	iov[1].iov_size = LONG_MAX - 1;

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE, EINVAL, &res);

	test_sum(buf_ptr, buf_size, buf_sum, TRUE, &res);
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "wrapping total size");

	/* Test word-unaligned iovec element size. */
	memcpy(iov, iovt, sizeof(iovt));
	iov[1].iov_size--;

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);
	c1 = buf2_ptr[buf2_size - 1];

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE, BUF_SIZE * 3 - 1,
		&res);

	if (accept_result(&res, RESULT_BADSTATUS, EINVAL)) {
		/* Do not test the first buffer, as it may contain a partial
		 * result.
		 */
		test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
		test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);
	} else {
		test_sum(buf_ptr, buf_size, buf_sum, FALSE, &res);
		test_sum(buf2_ptr, buf2_size, buf2_sum, FALSE, &res);
		test_sum(buf3_ptr, buf3_size, buf3_sum, FALSE, &res);
		if (c1 != buf2_ptr[buf2_size - 1])
			set_result(&res, RESULT_CORRUPT, 0);
	}

	got_result(&res, "word-unaligned size in iovec element");

	/* Test invalid grant in iovec element. */
	memcpy(iov, iovt, sizeof(iovt));
	iov[1].iov_grant = GRANT_INVALID;

	fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE, EINVAL, &res);

	/* Do not test the first buffer, as it may contain a partial result. */
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "invalid grant in iovec element");

	/* Test revoked grant in iovec element. */
	memcpy(iov, iovt, sizeof(iovt));
	if ((grant = cpf_grant_direct(driver_endpt, (vir_bytes) buf2_ptr,
			buf2_size, CPF_WRITE)) == GRANT_INVALID)
		panic("unable to allocate grant");

	cpf_revoke(grant);

	iov[1].iov_grant = grant;

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE, EINVAL, &res);

	accept_result(&res, RESULT_BADSTATUS, EPERM);

	/* Do not test the first buffer, as it may contain a partial result. */
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "revoked grant in iovec element");

	/* Test read-only grant in iovec element. */
	memcpy(iov, iovt, sizeof(iovt));
	if ((grant = cpf_grant_direct(driver_endpt, (vir_bytes) buf2_ptr,
			buf2_size, CPF_READ)) == GRANT_INVALID)
		panic("unable to allocate grant");

	iov[1].iov_grant = grant;

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE, EINVAL, &res);

	accept_result(&res, RESULT_BADSTATUS, EPERM);

	/* Do not test the first buffer, as it may contain a partial result. */
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "read-only grant in iovec element");

	cpf_revoke(grant);

	/* Test word-unaligned iovec element buffer. */
	memcpy(iov, iovt, sizeof(iovt));
	if ((grant = cpf_grant_direct(driver_endpt, (vir_bytes) (buf2_ptr + 1),
			buf2_size - 2, CPF_WRITE)) == GRANT_INVALID)
		panic("unable to allocate grant");

	iov[1].iov_grant = grant;
	iov[1].iov_size = buf2_size - 2;

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);
	c1 = buf2_ptr[0];
	c2 = buf2_ptr[buf2_size - 1];

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE, BUF_SIZE * 3 - 2,
		&res);

	if (accept_result(&res, RESULT_BADSTATUS, EINVAL)) {
		/* Do not test the first buffer, as it may contain a partial
		 * result.
		 */
		test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
		test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);
	} else {
		test_sum(buf_ptr, buf_size, buf_sum, FALSE, &res);
		test_sum(buf2_ptr, buf2_size, buf2_sum, FALSE, &res);
		test_sum(buf3_ptr, buf3_size, buf3_sum, FALSE, &res);
		if (c1 != buf2_ptr[0] || c2 != buf2_ptr[buf2_size - 1])
			set_result(&res, RESULT_CORRUPT, 0);
	}

	got_result(&res, "word-unaligned buffer in iovec element");

	cpf_revoke(grant);

	/* Test word-unaligned position. */
	memcpy(iov, iovt, sizeof(iovt));

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);

	raw_xfer(driver_minor, cvu64(1), iov, 3, FALSE, EINVAL, &res);

	test_sum(buf_ptr, buf_size, buf_sum, TRUE, &res);
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "word-unaligned position");

	/* Test normal vector request (final check). */
	memcpy(iov, iovt, sizeof(iovt));

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);

	raw_xfer(driver_minor, cvu64(0), iov, 3, FALSE,
		buf_size + buf2_size + buf3_size, &res);

	test_sum(buf_ptr, buf_size, buf_sum, FALSE, &res);
	test_sum(buf2_ptr, buf2_size, buf2_sum, FALSE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, FALSE, &res);

	got_result(&res, "normal vector request");

	/* Clean up. */
	free_buf_and_grant(buf3_ptr, buf3_grant, buf3_size);
	free_buf_and_grant(buf2_ptr, buf2_grant, buf2_size);
	free_buf_and_grant(buf_ptr, buf_grant, buf_size);
}

#define SECTOR_UNALIGN	2	/* word-aligned and sector-unaligned */

static void bad_write(void)
{
	/* Test various illegal write transfer requests, if writing is allowed.
	 * If handled correctly, these requests will not actually write data.
	 * However, the last test currently erroneously does end up writing.
	 */
	u8_t *buf_ptr, *buf2_ptr, *buf3_ptr;
	size_t buf_size, buf2_size, buf3_size;
	cp_grant_id_t buf_grant, buf2_grant, buf3_grant;
	cp_grant_id_t grant;
	u32_t buf_sum, buf2_sum, buf3_sum;
	iovec_s_t iov[3], iovt[3];
	result_t res;

	test_group("bad write requests", may_write);

	if (!may_write)
		return;

	buf_size = buf2_size = buf3_size = BUF_SIZE;

	alloc_buf_and_grant(&buf_ptr, &buf_grant, buf_size, CPF_READ);
	alloc_buf_and_grant(&buf2_ptr, &buf2_grant, buf2_size, CPF_READ);
	alloc_buf_and_grant(&buf3_ptr, &buf3_grant, buf3_size, CPF_READ);

	iovt[0].iov_grant = buf_grant;
	iovt[0].iov_size = buf_size;
	iovt[1].iov_grant = buf2_grant;
	iovt[1].iov_size = buf2_size;
	iovt[2].iov_grant = buf3_grant;
	iovt[2].iov_size = buf3_size;

	/* Test sector-unaligned write position. */
	memcpy(iov, iovt, sizeof(iovt));

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);

	raw_xfer(driver_minor, cvu64(SECTOR_UNALIGN), iov, 3, TRUE, EINVAL,
		&res);

	test_sum(buf_ptr, buf_size, buf_sum, TRUE, &res);
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "sector-unaligned write position");

	/* Test sector-unaligned write size. */
	memcpy(iov, iovt, sizeof(iovt));
	iov[1].iov_size -= SECTOR_UNALIGN;

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);

	raw_xfer(driver_minor, cvu64(0), iov, 3, TRUE, EINVAL, &res);

	test_sum(buf_ptr, buf_size, buf_sum, TRUE, &res);
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "sector-unaligned write size");

	/* Test write-only grant in iovec element. */
	memcpy(iov, iovt, sizeof(iovt));
	if ((grant = cpf_grant_direct(driver_endpt, (vir_bytes) buf2_ptr,
			buf2_size, CPF_WRITE)) == GRANT_INVALID)
		panic("unable to allocate grant");

	iov[1].iov_grant = grant;

	buf_sum = fill_rand(buf_ptr, buf_size);
	buf2_sum = fill_rand(buf2_ptr, buf2_size);
	buf3_sum = fill_rand(buf3_ptr, buf3_size);

	raw_xfer(driver_minor, cvu64(0), iov, 3, TRUE, EINVAL, &res);

	accept_result(&res, RESULT_BADSTATUS, EPERM);

	test_sum(buf_ptr, buf_size, buf_sum, TRUE, &res);
	test_sum(buf2_ptr, buf2_size, buf2_sum, TRUE, &res);
	test_sum(buf3_ptr, buf3_size, buf3_sum, TRUE, &res);

	got_result(&res, "write-only grant in iovec element");

	cpf_revoke(grant);

	/* Clean up. */
	free_buf_and_grant(buf3_ptr, buf3_grant, buf3_size);
	free_buf_and_grant(buf2_ptr, buf2_grant, buf2_size);
	free_buf_and_grant(buf_ptr, buf_grant, buf_size);
}

static void vector_and_large_sub(size_t small_size)
{
	/* Check whether large vectored requests, and large single requests,
	 * succeed.
	 */
	size_t large_size, buf_size, buf2_size;
	u8_t *buf_ptr, *buf2_ptr;
	iovec_t iovec[NR_IOREQS];
	u64_t base_pos;
	result_t res;
	int i;

	base_pos = cvu64(sector_size);

	large_size = small_size * NR_IOREQS;

	buf_size = large_size + sizeof(u32_t) * 2;
	buf2_size = large_size + sizeof(u32_t) * (NR_IOREQS + 1);

	buf_ptr = alloc_contig(buf_size, 0, NULL);
	buf2_ptr = alloc_contig(buf2_size, 0, NULL);
	if (buf_ptr == NULL || buf2_ptr == NULL)
		panic("unable to allocate memory");

	/* The first buffer has one large chunk with dword-sized guards on each
	 * side. LPTR(n) points to the start of the nth small data chunk within
	 * the large chunk. The second buffer contains several small chunks. It
	 * has dword-sized guards before each chunk and after the last chunk.
	 * SPTR(n) points to the start of the nth small chunk.
	 */
#define SPTR(n) (buf2_ptr + sizeof(u32_t) + (n) * (sizeof(u32_t) + small_size))
#define LPTR(n) (buf_ptr + sizeof(u32_t) + small_size * (n))

	/* Write one large chunk, if writing is allowed. */
	if (may_write) {
		fill_rand(buf_ptr, buf_size); /* don't need the checksum */

		iovec[0].iov_addr = (vir_bytes) (buf_ptr + sizeof(u32_t));
		iovec[0].iov_size = large_size;

		vir_xfer(driver_minor, base_pos, iovec, 1, TRUE, large_size,
			&res);

		got_result(&res, "large write");
	}

	/* Read back in many small chunks. If writing is not allowed, do not
	 * check checksums.
	 */
	for (i = 0; i < NR_IOREQS; i++) {
		* (((u32_t *) SPTR(i)) - 1) = 0xDEADBEEFL + i;
		iovec[i].iov_addr = (vir_bytes) SPTR(i);
		iovec[i].iov_size = small_size;
	}
	* (((u32_t *) SPTR(i)) - 1) = 0xFEEDFACEL;

	vir_xfer(driver_minor, base_pos, iovec, NR_IOREQS, FALSE, large_size,
		&res);

	if (res.type == RESULT_OK) {
		for (i = 0; i < NR_IOREQS; i++) {
			if (* (((u32_t *) SPTR(i)) - 1) != 0xDEADBEEFL + i)
				set_result(&res, RESULT_OVERFLOW, 0);
		}
		if (* (((u32_t *) SPTR(i)) - 1) != 0xFEEDFACEL)
			set_result(&res, RESULT_OVERFLOW, 0);
	}

	if (res.type == RESULT_OK && may_write) {
		for (i = 0; i < NR_IOREQS; i++) {
			test_sum(SPTR(i), small_size,
				get_sum(LPTR(i), small_size), TRUE, &res);
		}
	}

	got_result(&res, "vectored read");

	/* Write new data in many small chunks, if writing is allowed. */
	if (may_write) {
		fill_rand(buf2_ptr, buf2_size); /* don't need the checksum */

		for (i = 0; i < NR_IOREQS; i++) {
			iovec[i].iov_addr = (vir_bytes) SPTR(i);
			iovec[i].iov_size = small_size;
		}

		vir_xfer(driver_minor, base_pos, iovec, NR_IOREQS, TRUE,
			large_size, &res);

		got_result(&res, "vectored write");
	}

	/* Read back in one large chunk. If writing is allowed, the checksums
	 * must match the last write; otherwise, they must match the last read.
	 * In both cases, the expected content is in the second buffer.
	 */

	* (u32_t *) buf_ptr = 0xCAFEBABEL;
	* (u32_t *) (buf_ptr + sizeof(u32_t) + large_size) = 0xDECAFBADL;

	iovec[0].iov_addr = (vir_bytes) (buf_ptr + sizeof(u32_t));
	iovec[0].iov_size = large_size;

	vir_xfer(driver_minor, base_pos, iovec, 1, FALSE, large_size, &res);

	if (res.type == RESULT_OK) {
		if (* (u32_t *) buf_ptr != 0xCAFEBABEL)
			set_result(&res, RESULT_OVERFLOW, 0);
		if (* (u32_t *) (buf_ptr + sizeof(u32_t) + large_size) !=
				0xDECAFBADL)
			set_result(&res, RESULT_OVERFLOW, 0);
	}

	if (res.type == RESULT_OK) {
		for (i = 0; i < NR_IOREQS; i++) {
			test_sum(SPTR(i), small_size,
				get_sum(LPTR(i), small_size), TRUE, &res);
		}
	}

	got_result(&res, "large read");

#undef LPTR
#undef SPTR

	/* Clean up. */
	free_contig(buf2_ptr, buf2_size);
	free_contig(buf_ptr, buf_size);
}

static void vector_and_large(void)
{
	/* Check whether large vectored requests, and large single requests,
	 * succeed. These are request patterns commonly used by MFS and the
	 * filter driver, respectively. We try the same test twice: once with
	 * a common block size, and once to push against the max request size.
	 */
	size_t max_block;

	/* Compute the largest sector multiple which, when multiplied by
	 * NR_IOREQS, is no more than the maximum transfer size. Note that if
	 * max_size is not a multiple of sector_size, we're not going up to the
	 * limit entirely this way.
	 */
	max_block = max_size / NR_IOREQS;
	max_block -= max_block % sector_size;

#define COMMON_BLOCK_SIZE	4096

	test_group("vector and large, common block", TRUE);

	vector_and_large_sub(COMMON_BLOCK_SIZE);

	if (max_block != COMMON_BLOCK_SIZE) {
		test_group("vector and large, large block", TRUE);

		vector_and_large_sub(max_block);
	}
}

static void open_device(dev_t minor)
{
	/* Open a partition or subpartition. Remember that it has been opened,
	 * so that we can reopen it later in the event of a driver crash.
	 */
	message m;
	result_t res;

	memset(&m, 0, sizeof(m));
	m.m_type = BDEV_OPEN;
	m.BDEV_MINOR = minor;
	m.BDEV_ACCESS = may_write ? (R_BIT | W_BIT) : R_BIT;
	m.BDEV_ID = lrand48();

	sendrec_driver(&m, OK, &res);

	/* We assume that this call is supposed to succeed. We pretend it
	 * always succeeds, so that close_device() won't get confused later.
	 */
	assert(nr_opened < NR_OPENED);
	opened[nr_opened++] = minor;

	got_result(&res, minor == driver_minor ? "opening the main partition" :
		"opening a subpartition");
}

static void close_device(dev_t minor)
{
	/* Close a partition or subpartition. Remove it from the list of opened
	 * devices.
	 */
	message m;
	result_t res;
	int i;

	memset(&m, 0, sizeof(m));
	m.m_type = BDEV_CLOSE;
	m.BDEV_MINOR = minor;
	m.BDEV_ID = lrand48();

	sendrec_driver(&m, OK, &res);

	assert(nr_opened > 0);
	for (i = 0; i < nr_opened; i++) {
		if (opened[i] == minor) {
			opened[i] = opened[--nr_opened];
			break;
		}
	}

	got_result(&res, minor == driver_minor ? "closing the main partition" :
		"closing a subpartition");
}

static int vir_ioctl(dev_t minor, int req, void *ptr, ssize_t exp,
	result_t *res)
{
	/* Perform an I/O control request, using a local buffer.
	 */
	cp_grant_id_t grant;
	message m;
	int r, perm;

	assert(!_MINIX_IOCTL_BIG(req));	/* not supported */

	perm = 0;
	if (_MINIX_IOCTL_IOR(req)) perm |= CPF_WRITE;
	if (_MINIX_IOCTL_IOW(req)) perm |= CPF_READ;

	if ((grant = cpf_grant_direct(driver_endpt, (vir_bytes) ptr,
			_MINIX_IOCTL_SIZE(req), perm)) == GRANT_INVALID)
		panic("unable to allocate grant");

	memset(&m, 0, sizeof(m));
	m.m_type = BDEV_IOCTL;
	m.BDEV_MINOR = minor;
	m.BDEV_POS_LO = 0L;
	m.BDEV_POS_HI = 0L;
	m.BDEV_REQUEST = req;
	m.BDEV_GRANT = grant;
	m.BDEV_ID = lrand48();

	r = sendrec_driver(&m, exp, res);

	if (cpf_revoke(grant) != OK)
		panic("unable to revoke grant");

	return r;
}

static void misc_ioctl(void)
{
	/* Test some ioctls.
	 */
	result_t res;
	int openct;

	test_group("test miscellaneous ioctls", TRUE);

	/* Retrieve the main partition's base and size. Save for later. */
	vir_ioctl(driver_minor, DIOCGETP, &part, OK, &res);

	got_result(&res, "ioctl to get partition");

	/* The other tests do not check whether there is sufficient room. */
	if (res.type == RESULT_OK && cmp64u(part.size, max_size * 2) < 0)
		printf("WARNING: small partition, some tests may fail\n");

	/* Test retrieving global driver open count. */
	openct = 0x0badcafe;

	vir_ioctl(driver_minor, DIOCOPENCT, &openct, OK, &res);

	/* We assume that we're the only client to the driver right now. */
	if (res.type == RESULT_OK && openct != 1) {
		res.type = RESULT_BADVALUE;
		res.value = openct;
	}

	got_result(&res, "ioctl to get open count");

	/* Test increasing and re-retrieving open count. */
	open_device(driver_minor);

	openct = 0x0badcafe;

	vir_ioctl(driver_minor, DIOCOPENCT, &openct, OK, &res);

	if (res.type == RESULT_OK && openct != 2) {
		res.type = RESULT_BADVALUE;
		res.value = openct;
	}

	got_result(&res, "increased open count after opening");

	/* Test decreasing and re-retrieving open count. */
	close_device(driver_minor);

	openct = 0x0badcafe;

	vir_ioctl(driver_minor, DIOCOPENCT, &openct, OK, &res);

	if (res.type == RESULT_OK && openct != 1) {
		res.type = RESULT_BADVALUE;
		res.value = openct;
	}

	got_result(&res, "decreased open count after closing");
}

static void read_limits(dev_t sub0_minor, dev_t sub1_minor, size_t sub_size)
{
	/* Test reads up to, across, and beyond partition limits.
	 */
	u8_t *buf_ptr;
	size_t buf_size;
	u32_t sum, sum2, sum3;
	result_t res;

	test_group("read around subpartition limits", TRUE);

	buf_size = sector_size * 3;

	if ((buf_ptr = alloc_contig(buf_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	/* Read one sector up to the partition limit. */
	fill_rand(buf_ptr, buf_size);

	simple_xfer(sub0_minor, cvu64(sub_size - sector_size), buf_ptr,
		sector_size, FALSE, sector_size, &res);

	sum = get_sum(buf_ptr, sector_size);

	got_result(&res, "one sector read up to partition end");

	/* Read three sectors up to the partition limit. */
	fill_rand(buf_ptr, buf_size);

	simple_xfer(sub0_minor, cvu64(sub_size - buf_size), buf_ptr, buf_size,
		FALSE, buf_size, &res);

	test_sum(buf_ptr + sector_size * 2, sector_size, sum, TRUE, &res);

	sum2 = get_sum(buf_ptr + sector_size, sector_size * 2);

	got_result(&res, "multisector read up to partition end");

	/* Read three sectors, two up to and one beyond the partition end. */
	fill_rand(buf_ptr, buf_size);
	sum3 = get_sum(buf_ptr + sector_size * 2, sector_size);

	simple_xfer(sub0_minor, cvu64(sub_size - sector_size * 2), buf_ptr,
		buf_size, FALSE, sector_size * 2, &res);

	test_sum(buf_ptr, sector_size * 2, sum2, TRUE, &res);
	test_sum(buf_ptr + sector_size * 2, sector_size, sum3, TRUE, &res);

	got_result(&res, "read somewhat across partition end");

	/* Read three sectors, one up to and two beyond the partition end. */
	fill_rand(buf_ptr, buf_size);
	sum2 = get_sum(buf_ptr + sector_size, sector_size * 2);

	simple_xfer(sub0_minor, cvu64(sub_size - sector_size), buf_ptr,
		buf_size, FALSE, sector_size, &res);

	test_sum(buf_ptr, sector_size, sum, TRUE, &res);
	test_sum(buf_ptr + sector_size, sector_size * 2, sum2, TRUE, &res);

	got_result(&res, "read mostly across partition end");

	/* Read one sector starting at the partition end. */
	sum = fill_rand(buf_ptr, buf_size);
	sum2 = get_sum(buf_ptr, sector_size);

	simple_xfer(sub0_minor, cvu64(sub_size), buf_ptr, sector_size, FALSE,
		0, &res);

	test_sum(buf_ptr, sector_size, sum2, TRUE, &res);

	got_result(&res, "one sector read at partition end");

	/* Read three sectors starting at the partition end. */
	simple_xfer(sub0_minor, cvu64(sub_size), buf_ptr, buf_size, FALSE, 0,
		&res);

	test_sum(buf_ptr, buf_size, sum, TRUE, &res);

	got_result(&res, "multisector read at partition end");

	/* Read one sector beyond the partition end. */
	simple_xfer(sub0_minor, cvu64(sub_size + sector_size), buf_ptr,
		buf_size, FALSE, 0, &res);

	test_sum(buf_ptr, sector_size, sum2, TRUE, &res);

	got_result(&res, "single sector read beyond partition end");

	/* Read three sectors way beyond the partition end. */
	simple_xfer(sub0_minor, make64(0L, 0x10000000L), buf_ptr,
		buf_size, FALSE, 0, &res);

	test_sum(buf_ptr, buf_size, sum, TRUE, &res);

	/* Test negative offsets. This request should return EOF or fail; we
	 * assume that it return EOF here (because that is what the AHCI driver
	 * does, to avoid producing errors for requests close to the 2^64 byte
	 * position limit [yes, this will indeed never happen anyway]). This is
	 * more or less a bad requests test, but we cannot do it without
	 * setting up subpartitions first.
	 */
	simple_xfer(sub1_minor, make64(0xffffffffL - sector_size + 1,
		0xffffffffL), buf_ptr, sector_size, FALSE, 0, &res);

	test_sum(buf_ptr, sector_size, sum2, TRUE, &res);

	got_result(&res, "read with negative offset");

	/* Clean up. */
	free_contig(buf_ptr, buf_size);
}

static void write_limits(dev_t sub0_minor, dev_t sub1_minor, size_t sub_size)
{
	/* Test writes up to, across, and beyond partition limits. Use the
	 * first given subpartition to test, and the second to make sure there
	 * are no overruns. The given size is the size of each of the
	 * subpartitions. Note that the necessity to check the results using
	 * readback, makes this more or less a superset of the read test.
	 */
	u8_t *buf_ptr;
	size_t buf_size;
	u32_t sum, sum2, sum3, sub1_sum;
	result_t res;

	test_group("write around subpartition limits", may_write);

	if (!may_write)
		return;

	buf_size = sector_size * 3;

	if ((buf_ptr = alloc_contig(buf_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	/* Write to the start of the second subpartition, so that we can
	 * reliably check whether the contents have changed later.
	 */
	sub1_sum = fill_rand(buf_ptr, buf_size);

	simple_xfer(sub1_minor, cvu64(0), buf_ptr, buf_size, TRUE, buf_size,
		&res);

	got_result(&res, "write to second subpartition");

	/* Write one sector, up to the partition limit. */
	sum = fill_rand(buf_ptr, sector_size);

	simple_xfer(sub0_minor, cvu64(sub_size - sector_size), buf_ptr,
		sector_size, TRUE, sector_size, &res);

	got_result(&res, "write up to partition end");

	/* Read back to make sure the results have persisted. */
	fill_rand(buf_ptr, sector_size * 2);

	simple_xfer(sub0_minor, cvu64(sub_size - sector_size * 2), buf_ptr,
		sector_size * 2, FALSE, sector_size * 2, &res);

	test_sum(buf_ptr + sector_size, sector_size, sum, TRUE, &res);

	got_result(&res, "read up to partition end");

	/* Write three sectors, two up to and one beyond the partition end. */
	fill_rand(buf_ptr, buf_size);
	sum = get_sum(buf_ptr + sector_size, sector_size);
	sum3 = get_sum(buf_ptr, sector_size);

	simple_xfer(sub0_minor, cvu64(sub_size - sector_size * 2), buf_ptr,
		buf_size, TRUE, sector_size * 2, &res);

	got_result(&res, "write somewhat across partition end");

	/* Read three sectors, one up to and two beyond the partition end. */
	fill_rand(buf_ptr, buf_size);
	sum2 = get_sum(buf_ptr + sector_size, sector_size * 2);

	simple_xfer(sub0_minor, cvu64(sub_size - sector_size), buf_ptr,
		buf_size, FALSE, sector_size, &res);

	test_sum(buf_ptr, sector_size, sum, TRUE, &res);
	test_sum(buf_ptr + sector_size, sector_size * 2, sum2, TRUE, &res);

	got_result(&res, "read mostly across partition end");

	/* Repeat this but with write and read start positions swapped. */
	fill_rand(buf_ptr, buf_size);
	sum = get_sum(buf_ptr, sector_size);

	simple_xfer(sub0_minor, cvu64(sub_size - sector_size), buf_ptr,
		buf_size, TRUE, sector_size, &res);

	got_result(&res, "write mostly across partition end");

	fill_rand(buf_ptr, buf_size);
	sum2 = get_sum(buf_ptr + sector_size * 2, sector_size);

	simple_xfer(sub0_minor, cvu64(sub_size - sector_size * 2), buf_ptr,
		buf_size, FALSE, sector_size * 2, &res);

	test_sum(buf_ptr, sector_size, sum3, TRUE, &res);
	test_sum(buf_ptr + sector_size, sector_size, sum, TRUE, &res);
	test_sum(buf_ptr + sector_size * 2, sector_size, sum2, TRUE, &res);

	got_result(&res, "read somewhat across partition end");

	/* Write one sector at the end of the partition. */
	fill_rand(buf_ptr, sector_size);

	simple_xfer(sub0_minor, cvu64(sub_size), buf_ptr, sector_size, TRUE, 0,
		&res);

	got_result(&res, "write at partition end");

	/* Write one sector beyond the end of the partition. */
	simple_xfer(sub0_minor, cvu64(sub_size + sector_size), buf_ptr,
		sector_size, TRUE, 0, &res);

	got_result(&res, "write beyond partition end");

	/* Read from the start of the second subpartition, and see if it
	 * matches what we wrote into it earlier.
	 */
	fill_rand(buf_ptr, buf_size);

	simple_xfer(sub1_minor, cvu64(0), buf_ptr, buf_size, FALSE, buf_size,
		&res);

	test_sum(buf_ptr, buf_size, sub1_sum, TRUE, &res);

	got_result(&res, "read from second subpartition");

	/* Test offset wrapping, but this time for writes. */
	fill_rand(buf_ptr, sector_size);

	simple_xfer(sub1_minor, make64(0xffffffffL - sector_size + 1,
		0xffffffffL), buf_ptr, sector_size, TRUE, 0, &res);

	got_result(&res, "write with negative offset");

	/* If the last request erroneously succeeded, it would have overwritten
	 * the last sector of the first subpartition.
	 */
	simple_xfer(sub0_minor, cvu64(sub_size - sector_size), buf_ptr,
		sector_size, FALSE, sector_size, &res);

	test_sum(buf_ptr, sector_size, sum, TRUE, &res);

	got_result(&res, "read up to partition end");

	/* Clean up. */
	free_contig(buf_ptr, buf_size);
}

static void vir_limits(dev_t sub0_minor, dev_t sub1_minor, int part_secs)
{
	/* Create virtual, temporary subpartitions through the DIOCSETP ioctl,
	 * and perform tests on the resulting subpartitions.
	 */
	struct partition subpart, subpart2;
	size_t sub_size;
	result_t res;

	test_group("virtual subpartition limits", TRUE);

	/* Open the subpartitions. This is somewhat dodgy; we rely on the
	 * driver allowing this even if no subpartitions exist. We cannot do
	 * this test without doing a DIOCSETP on an open subdevice, though.
	 */
	open_device(sub0_minor);
	open_device(sub1_minor);

	sub_size = sector_size * part_secs;

	/* Set, and check, the size of the first subpartition. */
	subpart = part;
	subpart.size = cvu64(sub_size);

	vir_ioctl(sub0_minor, DIOCSETP, &subpart, OK, &res);

	got_result(&res, "ioctl to set first subpartition");

	vir_ioctl(sub0_minor, DIOCGETP, &subpart2, OK, &res);

	if (res.type == RESULT_OK && (cmp64(subpart.base, subpart2.base) ||
			cmp64(subpart.size, subpart2.size))) {
		res.type = RESULT_BADVALUE;
		res.value = 0;
	}

	got_result(&res, "ioctl to get first subpartition");

	/* Set, and check, the base and size of the second subpartition. */
	subpart = part;
	subpart.base = add64u(subpart.base, sub_size);
	subpart.size = cvu64(sub_size);

	vir_ioctl(sub1_minor, DIOCSETP, &subpart, OK, &res);

	got_result(&res, "ioctl to set second subpartition");

	vir_ioctl(sub1_minor, DIOCGETP, &subpart2, OK, &res);

	if (res.type == RESULT_OK && (cmp64(subpart.base, subpart2.base) ||
			cmp64(subpart.size, subpart2.size))) {
		res.type = RESULT_BADVALUE;
		res.value = 0;
	}

	got_result(&res, "ioctl to get second subpartition");

	/* Perform the actual I/O tests. */
	read_limits(sub0_minor, sub1_minor, sub_size);

	write_limits(sub0_minor, sub1_minor, sub_size);

	/* Clean up. */
	close_device(sub1_minor);
	close_device(sub0_minor);
}

static void real_limits(dev_t sub0_minor, dev_t sub1_minor, int part_secs)
{
	/* Create our own subpartitions by writing a partition table, and
	 * perform tests on the resulting real subpartitions.
	 */
	u8_t *buf_ptr;
	size_t buf_size, sub_size;
	struct partition subpart;
	struct part_entry *entry;
	result_t res;

	test_group("real subpartition limits", may_write);

	if (!may_write)
		return;

	sub_size = sector_size * part_secs;

	/* Technically, we should be using 512 instead of sector_size in
	 * various places, because even on CD-ROMs, the partition tables are
	 * 512 bytes and the sector counts are based on 512-byte sectors in it.
	 * We ignore this subtlety because CD-ROMs are assumed to be read-only
	 * anyway.
	 */
	buf_size = sector_size;

	if ((buf_ptr = alloc_contig(buf_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	memset(buf_ptr, 0, buf_size);

	/* Write an invalid partition table. */
	simple_xfer(driver_minor, cvu64(0), buf_ptr, buf_size, TRUE, buf_size,
		&res);

	got_result(&res, "write of invalid partition table");

	/* Get the disk driver to reread the partition table. This should
	 * happen (at least) when the device is fully closed and then reopened.
	 * The ioctl test already made sure that we're the only client.
	 */
	close_device(driver_minor);
	open_device(driver_minor);

	/* See if our changes are visible. We expect the subpartitions to have
	 * a size of zero now, indicating that they're not there. For actual
	 * subpartitions (as opposed to normal partitions), this requires the
	 * driver to zero them out, because the partition code does not do so.
	 */
	open_device(sub0_minor);
	open_device(sub1_minor);

	vir_ioctl(sub0_minor, DIOCGETP, &subpart, 0, &res);

	if (res.type == RESULT_OK && cmp64u(subpart.size, 0)) {
		res.type = RESULT_BADVALUE;
		res.value = ex64lo(subpart.size);
	}

	got_result(&res, "ioctl to get first subpartition");

	vir_ioctl(sub1_minor, DIOCGETP, &subpart, 0, &res);

	if (res.type == RESULT_OK && cmp64u(subpart.size, 0)) {
		res.type = RESULT_BADVALUE;
		res.value = ex64lo(subpart.size);
	}

	got_result(&res, "ioctl to get second subpartition");

	close_device(sub1_minor);
	close_device(sub0_minor);

	/* Now write a valid partition table. */
	memset(buf_ptr, 0, buf_size);

	entry = (struct part_entry *) &buf_ptr[PART_TABLE_OFF];

	entry[0].sysind = MINIX_PART;
	entry[0].lowsec = div64u(part.base, sector_size) + 1;
	entry[0].size = part_secs;
	entry[1].sysind = MINIX_PART;
	entry[1].lowsec = entry[0].lowsec + entry[0].size;
	entry[1].size = part_secs;

	buf_ptr[510] = 0x55;
	buf_ptr[511] = 0xAA;

	simple_xfer(driver_minor, cvu64(0), buf_ptr, buf_size, TRUE, buf_size,
		&res);

	got_result(&res, "write of valid partition table");

	/* Same as above. */
	close_device(driver_minor);
	open_device(driver_minor);

	/* Again, see if our changes are visible. This time the proper base and
	 * size should be there.
	 */
	open_device(sub0_minor);
	open_device(sub1_minor);

	vir_ioctl(sub0_minor, DIOCGETP, &subpart, 0, &res);

	if (res.type == RESULT_OK && (cmp64(subpart.base,
		add64u(part.base, sector_size)) ||
		cmp64u(subpart.size, part_secs * sector_size))) {

		res.type = RESULT_BADVALUE;
		res.value = 0;
	}

	got_result(&res, "ioctl to get first subpartition");

	vir_ioctl(sub1_minor, DIOCGETP, &subpart, 0, &res);

	if (res.type == RESULT_OK && (cmp64(subpart.base,
		add64u(part.base, (1 + part_secs) * sector_size)) ||
		cmp64u(subpart.size, part_secs * sector_size))) {

		res.type = RESULT_BADVALUE;
		res.value = 0;
	}

	got_result(&res, "ioctl to get second subpartition");

	/* Now perform the actual I/O tests. */
	read_limits(sub0_minor, sub1_minor, sub_size);

	write_limits(sub0_minor, sub1_minor, sub_size);

	/* Clean up. */
	close_device(sub0_minor);
	close_device(sub1_minor);

	free_contig(buf_ptr, buf_size);
}

static void part_limits(void)
{
	/* Test reads and writes up to, across, and beyond partition limits.
	 * As a side effect, test reading and writing partition sizes and
	 * rereading partition tables.
	 */
	dev_t par, sub0_minor, sub1_minor;

	/* First determine the first two subpartitions of the partition that we
	 * are operating on. If we are already operating on a subpartition, we
	 * cannot conduct this test.
	 */
	if (driver_minor >= MINOR_d0p0s0) {
		printf("WARNING: operating on subpartition, "
			"skipping partition tests\n");
		return;
	}
	par = driver_minor % DEV_PER_DRIVE;
	if (par > 0) /* adapted from libdriver's drvlib code */
		sub0_minor = MINOR_d0p0s0 + ((driver_minor / DEV_PER_DRIVE) *
			NR_PARTITIONS + par - 1) * NR_PARTITIONS;
	else
		sub0_minor = driver_minor + 1;
	sub1_minor = sub0_minor + 1;

#define PART_SECS	9	/* sectors in each partition. must be >= 4. */

	/* First try the test with temporarily specified subpartitions. */
	vir_limits(sub0_minor, sub1_minor, PART_SECS);

	/* Then, if we're allowed to write, try the test with real, persisted
	 * subpartitions.
	 */
	real_limits(sub0_minor, sub1_minor, PART_SECS - 1);

}

static void unaligned_size_io(u64_t base_pos, u8_t *buf_ptr, size_t buf_size,
	u8_t *sec_ptr[2], int sectors, int pattern, u32_t ssum[5])
{
	/* Perform a single small-element I/O read, write, readback test.
	 * The number of sectors and the pattern varies with each call.
	 * The ssum array has to be updated to reflect the five sectors'
	 * checksums on disk, if writing is enabled. Note that for
	 */
	iovec_t iov[3], iovt[3];
	u32_t rsum[3];
	result_t res;
	size_t total_size;
	int i, nr_req;

	base_pos = add64u(base_pos, sector_size);
	total_size = sector_size * sectors;

	/* If the limit is two elements per sector, we cannot test three
	 * elements in a single sector.
	 */
	if (sector_size / element_size == 2 && sectors == 1 && pattern == 2)
		return;

	/* Set up the buffers and I/O vector. We use different buffers for the
	 * elements to minimize the chance that something "accidentally" goes
	 * right, but that means we have to do memory copying to do checksum
	 * computation.
	 */
	fill_rand(sec_ptr[0], sector_size);
	rsum[0] =
		get_sum(sec_ptr[0] + element_size, sector_size - element_size);

	fill_rand(buf_ptr, buf_size);

	switch (pattern) {
	case 0:
		/* First pattern: a small element on the left. */
		iovt[0].iov_addr = (vir_bytes) sec_ptr[0];
		iovt[0].iov_size = element_size;

		iovt[1].iov_addr = (vir_bytes) buf_ptr;
		iovt[1].iov_size = total_size - element_size;
		rsum[1] = get_sum(buf_ptr + iovt[1].iov_size, element_size);

		nr_req = 2;
		break;
	case 1:
		/* Second pattern: a small element on the right. */
		iovt[0].iov_addr = (vir_bytes) buf_ptr;
		iovt[0].iov_size = total_size - element_size;
		rsum[1] = get_sum(buf_ptr + iovt[0].iov_size, element_size);

		iovt[1].iov_addr = (vir_bytes) sec_ptr[0];
		iovt[1].iov_size = element_size;

		nr_req = 2;
		break;
	case 2:
		/* Third pattern: a small element on each side. */
		iovt[0].iov_addr = (vir_bytes) sec_ptr[0];
		iovt[0].iov_size = element_size;

		iovt[1].iov_addr = (vir_bytes) buf_ptr;
		iovt[1].iov_size = total_size - element_size * 2;
		rsum[1] = get_sum(buf_ptr + iovt[1].iov_size,
			element_size * 2);

		fill_rand(sec_ptr[1], sector_size);
		iovt[2].iov_addr = (vir_bytes) sec_ptr[1];
		iovt[2].iov_size = element_size;
		rsum[2] = get_sum(sec_ptr[1] + element_size,
			sector_size - element_size);

		nr_req = 3;
		break;
	default:
		assert(0);
	}

	/* Perform a read with small elements, and test whether the result is
	 * as expected.
	 */
	memcpy(iov, iovt, sizeof(iov));
	vir_xfer(driver_minor, base_pos, iov, nr_req, FALSE, total_size, &res);

	test_sum(sec_ptr[0] + element_size, sector_size - element_size,
		rsum[0], TRUE, &res);

	switch (pattern) {
	case 0:
		test_sum(buf_ptr + iovt[1].iov_size, element_size, rsum[1],
			TRUE, &res);
		memmove(buf_ptr + element_size, buf_ptr, iovt[1].iov_size);
		memcpy(buf_ptr, sec_ptr[0], element_size);
		break;
	case 1:
		test_sum(buf_ptr + iovt[0].iov_size, element_size, rsum[1],
			TRUE, &res);
		memcpy(buf_ptr + iovt[0].iov_size, sec_ptr[0], element_size);
		break;
	case 2:
		test_sum(buf_ptr + iovt[1].iov_size, element_size * 2, rsum[1],
			TRUE, &res);
		test_sum(sec_ptr[1] + element_size, sector_size - element_size,
			rsum[2], TRUE, &res);
		memmove(buf_ptr + element_size, buf_ptr, iovt[1].iov_size);
		memcpy(buf_ptr, sec_ptr[0], element_size);
		memcpy(buf_ptr + element_size + iovt[1].iov_size, sec_ptr[1],
			element_size);

		break;
	}

	for (i = 0; i < sectors; i++)
		test_sum(buf_ptr + sector_size * i, sector_size, ssum[1 + i],
			TRUE, &res);

	got_result(&res, "read with small elements");

	/* In read-only mode, we have nothing more to do. */
	if (!may_write)
		return;

	/* Use the same I/O vector to perform a write with small elements.
	 * This will cause the checksums of the target sectors to change,
	 * so we need to update those for both verification and later usage.
	 */
	for (i = 0; i < sectors; i++)
		ssum[1 + i] =
			fill_rand(buf_ptr + sector_size * i, sector_size);

	switch (pattern) {
	case 0:
		memcpy(sec_ptr[0], buf_ptr, element_size);
		memmove(buf_ptr, buf_ptr + element_size, iovt[1].iov_size);
		fill_rand(buf_ptr + iovt[1].iov_size, element_size);
		break;
	case 1:
		memcpy(sec_ptr[0], buf_ptr + iovt[0].iov_size, element_size);
		fill_rand(buf_ptr + iovt[0].iov_size, element_size);
		break;
	case 2:
		memcpy(sec_ptr[0], buf_ptr, element_size);
		memcpy(sec_ptr[1], buf_ptr + element_size + iovt[1].iov_size,
			element_size);
		memmove(buf_ptr, buf_ptr + element_size, iovt[1].iov_size);
		fill_rand(buf_ptr + iovt[1].iov_size, element_size * 2);
		break;
	}

	memcpy(iov, iovt, sizeof(iov));

	vir_xfer(driver_minor, base_pos, iov, nr_req, TRUE, total_size, &res);

	got_result(&res, "write with small elements");

	/* Now perform normal readback verification. */
	fill_rand(buf_ptr, sector_size * 3);

	simple_xfer(driver_minor, base_pos, buf_ptr, sector_size * 3, FALSE,
		sector_size * 3, &res);

	for (i = 0; i < 3; i++)
		test_sum(buf_ptr + sector_size * i, sector_size, ssum[1 + i],
			TRUE, &res);

	got_result(&res, "readback verification");
}

static void unaligned_size(void)
{
	/* Test sector-unaligned sizes in I/O vector elements. The total size
	 * of the request, however, has to add up to the sector size.
	 */
	u8_t *buf_ptr, *sec_ptr[2];
	size_t buf_size;
	u32_t sum = 0L, ssum[5];
	u64_t base_pos;
	result_t res;
	int i;

	test_group("sector-unaligned elements", sector_size != element_size);

	/* We can only do this test if the driver allows small elements. */
	if (sector_size == element_size)
		return;

	/* Crashing on bad user input, terrible! */
	assert(sector_size % element_size == 0);

	/* Establish a baseline by writing and reading back five sectors; or
	 * by reading only, if writing is disabled.
	 */
	buf_size = sector_size * 5;

	base_pos = cvu64(sector_size * 2);

	if ((buf_ptr = alloc_contig(buf_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	if ((sec_ptr[0] = alloc_contig(sector_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	if ((sec_ptr[1] = alloc_contig(sector_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	if (may_write) {
		sum = fill_rand(buf_ptr, buf_size);

		for (i = 0; i < 5; i++)
			ssum[i] = get_sum(buf_ptr + sector_size * i,
				sector_size);

		simple_xfer(driver_minor, base_pos, buf_ptr, buf_size, TRUE,
			buf_size, &res);

		got_result(&res, "write several sectors");
	}

	fill_rand(buf_ptr, buf_size);

	simple_xfer(driver_minor, base_pos, buf_ptr, buf_size, FALSE, buf_size,
		&res);

	if (may_write) {
		test_sum(buf_ptr, buf_size, sum, TRUE, &res);
	}
	else {
		for (i = 0; i < 5; i++)
			ssum[i] = get_sum(buf_ptr + sector_size * i,
				sector_size);
	}

	got_result(&res, "read several sectors");

	/* We do nine subtests. The first three involve only the second sector;
	 * the second three involve the second and third sectors, and the third
	 * three involve all of the middle sectors. Each triplet tests small
	 * elements at the left, at the right, and at both the left and the
	 * right of the area. For each operation, we first do an unaligned
	 * read, and if writing is enabled, an unaligned write and an aligned
	 * read.
	 */
	for (i = 0; i < 9; i++) {
		unaligned_size_io(base_pos, buf_ptr, buf_size, sec_ptr,
			i / 3 + 1, i % 3, ssum);
	}

	/* If writing was enabled, make sure that the first and fifth sector
	 * have remained untouched.
	 */
	if (may_write) {
		fill_rand(buf_ptr, buf_size);

		simple_xfer(driver_minor, base_pos, buf_ptr, buf_size, FALSE,
			buf_size, &res);

		test_sum(buf_ptr, sector_size, ssum[0], TRUE, &res);
		test_sum(buf_ptr + sector_size * 4, sector_size, ssum[4], TRUE,
			&res);

		got_result(&res, "check first and last sectors");
	}

	/* Clean up. */
	free_contig(sec_ptr[1], sector_size);
	free_contig(sec_ptr[0], sector_size);
	free_contig(buf_ptr, buf_size);
}

static void unaligned_pos1(void)
{
	/* Test sector-unaligned positions and total sizes for requests. This
	 * is a read-only test as no driver currently supports sector-unaligned
	 * writes. In this context, the term "lead" means an unwanted first
	 * part of a sector, and "trail" means an unwanted last part of a
	 * sector.
	 */
	u8_t *buf_ptr, *buf2_ptr;
	size_t buf_size, buf2_size, size;
	u32_t sum, sum2;
	u64_t base_pos;
	result_t res;

	test_group("sector-unaligned positions, part one",
		min_read != sector_size);

	/* We can only do this test if the driver allows small read requests.
	 */
	if (min_read == sector_size)
		return;

	assert(sector_size % min_read == 0);
	assert(min_read % element_size == 0);

	/* Establish a baseline by writing and reading back three sectors; or
	 * by reading only, if writing is disabled.
	 */
	buf_size = buf2_size = sector_size * 3;

	base_pos = cvu64(sector_size * 3);

	if ((buf_ptr = alloc_contig(buf_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	if ((buf2_ptr = alloc_contig(buf2_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	if (may_write) {
		sum = fill_rand(buf_ptr, buf_size);

		simple_xfer(driver_minor, base_pos, buf_ptr, buf_size, TRUE,
			buf_size, &res);

		got_result(&res, "write several sectors");
	}

	fill_rand(buf_ptr, buf_size);

	simple_xfer(driver_minor, base_pos, buf_ptr, buf_size, FALSE, buf_size,
		&res);

	if (may_write)
		test_sum(buf_ptr, buf_size, sum, TRUE, &res);

	got_result(&res, "read several sectors");

	/* Start with a simple test that operates within a single sector,
	 * first using a lead.
	 */
	fill_rand(buf2_ptr, sector_size);
	sum = get_sum(buf2_ptr + min_read, sector_size - min_read);

	simple_xfer(driver_minor, add64u(base_pos, sector_size - min_read),
		buf2_ptr, min_read, FALSE, min_read, &res);

	test_sum(buf2_ptr, min_read, get_sum(buf_ptr + sector_size - min_read,
		min_read), TRUE, &res);
	test_sum(buf2_ptr + min_read, sector_size - min_read, sum, TRUE,
		&res);

	got_result(&res, "single sector read with lead");

	/* Then a trail. */
	fill_rand(buf2_ptr, sector_size);
	sum = get_sum(buf2_ptr, sector_size - min_read);

	simple_xfer(driver_minor, base_pos, buf2_ptr + sector_size - min_read,
		min_read, FALSE, min_read, &res);

	test_sum(buf2_ptr + sector_size - min_read, min_read, get_sum(buf_ptr,
		min_read), TRUE, &res);
	test_sum(buf2_ptr, sector_size - min_read, sum, TRUE, &res);

	got_result(&res, "single sector read with trail");

	/* And then a lead and a trail, unless min_read is half the sector
	 * size, in which case this will be another lead test.
	 */
	fill_rand(buf2_ptr, sector_size);
	sum = get_sum(buf2_ptr, min_read);
	sum2 = get_sum(buf2_ptr + min_read * 2, sector_size - min_read * 2);

	simple_xfer(driver_minor, add64u(base_pos, min_read),
		buf2_ptr + min_read, min_read, FALSE, min_read, &res);

	test_sum(buf2_ptr + min_read, min_read, get_sum(buf_ptr + min_read,
		min_read), TRUE, &res);
	test_sum(buf2_ptr, min_read, sum, TRUE, &res);
	test_sum(buf2_ptr + min_read * 2, sector_size - min_read * 2, sum2,
		TRUE, &res);

	got_result(&res, "single sector read with lead and trail");

	/* Now do the same but with three sectors, and still only one I/O
	 * vector element. First up: lead.
	 */
	size = min_read + sector_size * 2;

	fill_rand(buf2_ptr, buf2_size);
	sum = get_sum(buf2_ptr + size, buf2_size - size);

	simple_xfer(driver_minor, add64u(base_pos, sector_size - min_read),
		buf2_ptr, size, FALSE, size, &res);

	test_sum(buf2_ptr, size, get_sum(buf_ptr + sector_size - min_read,
		size), TRUE, &res);
	test_sum(buf2_ptr + size, buf2_size - size, sum, TRUE, &res);

	got_result(&res, "multisector read with lead");

	/* Then trail. */
	fill_rand(buf2_ptr, buf2_size);
	sum = get_sum(buf2_ptr + size, buf2_size - size);

	simple_xfer(driver_minor, base_pos, buf2_ptr, size, FALSE, size, &res);

	test_sum(buf2_ptr, size, get_sum(buf_ptr, size), TRUE, &res);
	test_sum(buf2_ptr + size, buf2_size - size, sum, TRUE, &res);

	got_result(&res, "multisector read with trail");

	/* Then lead and trail. Use sector size as transfer unit to throw off
	 * simplistic lead/trail detection.
	 */
	fill_rand(buf2_ptr, buf2_size);
	sum = get_sum(buf2_ptr + sector_size, buf2_size - sector_size);

	simple_xfer(driver_minor, add64u(base_pos, min_read), buf2_ptr,
		sector_size, FALSE, sector_size, &res);

	test_sum(buf2_ptr, sector_size, get_sum(buf_ptr + min_read,
		sector_size), TRUE, &res);
	test_sum(buf2_ptr + sector_size, buf2_size - sector_size, sum, TRUE,
		&res);

	got_result(&res, "multisector read with lead and trail");

	/* Clean up. */
	free_contig(buf2_ptr, buf2_size);
	free_contig(buf_ptr, buf_size);
}

static void unaligned_pos2(void)
{
	/* Test sector-unaligned positions and total sizes for requests, second
	 * part. This one tests the use of multiple I/O vector elements, and
	 * tries to push the limits of the driver by completely filling an I/O
	 * vector and going up to the maximum request size.
	 */
	u8_t *buf_ptr, *buf2_ptr;
	size_t buf_size, buf2_size, max_block;
	u32_t sum = 0L, sum2 = 0L, rsum[NR_IOREQS];
	u64_t base_pos;
	iovec_t iov[NR_IOREQS];
	result_t res;
	int i;

	test_group("sector-unaligned positions, part two",
		min_read != sector_size);

	/* We can only do this test if the driver allows small read requests.
	 */
	if (min_read == sector_size)
		return;

	buf_size = buf2_size = max_size + sector_size;

	base_pos = cvu64(sector_size * 3);

	if ((buf_ptr = alloc_contig(buf_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	if ((buf2_ptr = alloc_contig(buf2_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	/* First establish a baseline. We need two requests for this, as the
	 * total area intentionally exceeds the max request size.
	 */
	if (may_write) {
		sum = fill_rand(buf_ptr, max_size);

		simple_xfer(driver_minor, base_pos, buf_ptr, max_size, TRUE,
			max_size, &res);

		got_result(&res, "large baseline write");

		sum2 = fill_rand(buf_ptr + max_size, sector_size);

		simple_xfer(driver_minor, add64u(base_pos, max_size),
			buf_ptr + max_size, sector_size, TRUE, sector_size,
			&res);

		got_result(&res, "small baseline write");
	}

	fill_rand(buf_ptr, buf_size);

	simple_xfer(driver_minor, base_pos, buf_ptr, max_size, FALSE, max_size,
		&res);

	if (may_write)
		test_sum(buf_ptr, max_size, sum, TRUE, &res);

	got_result(&res, "large baseline read");

	simple_xfer(driver_minor, add64u(base_pos, max_size), buf_ptr +
		max_size, sector_size, FALSE, sector_size, &res);

	if (may_write)
		test_sum(buf_ptr + max_size, sector_size, sum2, TRUE, &res);

	got_result(&res, "small baseline read");

	/* First construct a full vector with minimal sizes. The resulting area
	 * may well fall within a single sector, if min_read is small enough.
	 */
	fill_rand(buf2_ptr, buf2_size);

	for (i = 0; i < NR_IOREQS; i++) {
		iov[i].iov_addr = (vir_bytes) buf2_ptr + i * sector_size;
		iov[i].iov_size = min_read;

		rsum[i] = get_sum(buf2_ptr + i * sector_size + min_read,
			sector_size - min_read);
	}

	vir_xfer(driver_minor, add64u(base_pos, min_read), iov, NR_IOREQS,
		FALSE, min_read * NR_IOREQS, &res);

	for (i = 0; i < NR_IOREQS; i++) {
		test_sum(buf2_ptr + i * sector_size + min_read,
			sector_size - min_read, rsum[i], TRUE, &res);
		memmove(buf2_ptr + i * min_read, buf2_ptr + i * sector_size,
			min_read);
	}

	test_sum(buf2_ptr, min_read * NR_IOREQS, get_sum(buf_ptr + min_read,
		min_read * NR_IOREQS), TRUE, &res);

	got_result(&res, "small fully unaligned filled vector");

	/* Sneak in a maximum sized request with a single I/O vector element,
	 * unaligned. If the driver splits up such large requests into smaller
	 * chunks, this tests whether it does so correctly in the presence of
	 * leads and trails.
	 */
	fill_rand(buf2_ptr, buf2_size);

	simple_xfer(driver_minor, add64u(base_pos, min_read), buf2_ptr,
		max_size, FALSE, max_size, &res);

	test_sum(buf2_ptr, max_size, get_sum(buf_ptr + min_read, max_size),
		TRUE, &res);

	got_result(&res, "large fully unaligned single element");

	/* Then try with a vector where each element is as large as possible.
	 * We don't have room to do bounds integrity checking here (we could
	 * make room, but this may be a lot of memory already).
	 */
	/* Compute the largest sector multiple which, when multiplied by
	 * NR_IOREQS, is no more than the maximum transfer size.
	 */
	max_block = max_size / NR_IOREQS;
	max_block -= max_block % sector_size;

	fill_rand(buf2_ptr, buf2_size);

	for (i = 0; i < NR_IOREQS; i++) {
		iov[i].iov_addr = (vir_bytes) buf2_ptr + i * max_block;
		iov[i].iov_size = max_block;
	}

	vir_xfer(driver_minor, add64u(base_pos, min_read), iov, NR_IOREQS,
		FALSE, max_block * NR_IOREQS, &res);

	test_sum(buf2_ptr, max_block * NR_IOREQS, get_sum(buf_ptr + min_read,
		max_block * NR_IOREQS), TRUE, &res);

	got_result(&res, "large fully unaligned filled vector");

	/* Clean up. */
	free_contig(buf2_ptr, buf2_size);
	free_contig(buf_ptr, buf_size);
}

static void sweep_area(u64_t base_pos)
{
	/* Go over an eight-sector area from left (low address) to right (high
	 * address), reading and optionally writing in three-sector chunks, and
	 * advancing one sector at a time.
	 */
	u8_t *buf_ptr;
	size_t buf_size;
	u32_t sum = 0L, ssum[8];
	result_t res;
	int i, j;

	buf_size = sector_size * 8;

	if ((buf_ptr = alloc_contig(buf_size, 0, NULL)) == NULL)
		panic("unable to allocate memory");

	/* First (write to, if allowed, and) read from the entire area in one
	 * go, so that we know the (initial) contents of the area.
	 */
	if (may_write) {
		sum = fill_rand(buf_ptr, buf_size);

		simple_xfer(driver_minor, base_pos, buf_ptr, buf_size, TRUE,
			buf_size, &res);

		got_result(&res, "write to full area");
	}

	fill_rand(buf_ptr, buf_size);

	simple_xfer(driver_minor, base_pos, buf_ptr, buf_size, FALSE, buf_size,
		&res);

	if (may_write)
		test_sum(buf_ptr, buf_size, sum, TRUE, &res);

	for (i = 0; i < 8; i++)
		ssum[i] = get_sum(buf_ptr + sector_size * i, sector_size);

	got_result(&res, "read from full area");

	/* For each of the six three-sector subareas, first read from the
	 * subarea, check its checksum, and then (if allowed) write new content
	 * to it.
	 */
	for (i = 0; i < 6; i++) {
		fill_rand(buf_ptr, sector_size * 3);

		simple_xfer(driver_minor, add64u(base_pos, sector_size * i),
			buf_ptr, sector_size * 3, FALSE, sector_size * 3,
			&res);

		for (j = 0; j < 3; j++)
			test_sum(buf_ptr + sector_size * j, sector_size,
				ssum[i + j], TRUE, &res);

		got_result(&res, "read from subarea");

		if (!may_write)
			continue;

		fill_rand(buf_ptr, sector_size * 3);

		simple_xfer(driver_minor, add64u(base_pos, sector_size * i),
			buf_ptr, sector_size * 3, TRUE, sector_size * 3, &res);

		for (j = 0; j < 3; j++)
			ssum[i + j] = get_sum(buf_ptr + sector_size * j,
				sector_size);

		got_result(&res, "write to subarea");
	}

	/* Finally, if writing was enabled, do one final readback. */
	if (may_write) {
		fill_rand(buf_ptr, buf_size);

		simple_xfer(driver_minor, base_pos, buf_ptr, buf_size, FALSE,
			buf_size, &res);

		for (i = 0; i < 8; i++)
			test_sum(buf_ptr + sector_size * i, sector_size,
				ssum[i], TRUE, &res);

		got_result(&res, "readback from full area");
	}

	/* Clean up. */
	free_contig(buf_ptr, buf_size);
}

static void sweep_and_check(u64_t pos, int check_integ)
{
	/* Perform an area sweep at the given position. If asked for, get an
	 * integrity checksum over the beginning of the disk (first writing
	 * known data into it if that is allowed) before doing the sweep, and
	 * test the integrity checksum against the disk contents afterwards.
	 */
	u8_t *buf_ptr;
	size_t buf_size;
	u32_t sum = 0L;
	result_t res;

	if (check_integ) {
		buf_size = sector_size * 3;

		if ((buf_ptr = alloc_contig(buf_size, 0, NULL)) == NULL)
			panic("unable to allocate memory");

		if (may_write) {
			sum = fill_rand(buf_ptr, buf_size);

			simple_xfer(driver_minor, cvu64(0), buf_ptr, buf_size,
				TRUE, buf_size, &res);

			got_result(&res, "write integrity zone");
		}

		fill_rand(buf_ptr, buf_size);

		simple_xfer(driver_minor, cvu64(0), buf_ptr, buf_size, FALSE,
			buf_size, &res);

		if (may_write)
			test_sum(buf_ptr, buf_size, sum, TRUE, &res);
		else
			sum = get_sum(buf_ptr, buf_size);

		got_result(&res, "read integrity zone");
	}

	sweep_area(pos);

	if (check_integ) {
		fill_rand(buf_ptr, buf_size);

		simple_xfer(driver_minor, cvu64(0), buf_ptr, buf_size, FALSE,
			buf_size, &res);

		test_sum(buf_ptr, buf_size, sum, TRUE, &res);

		got_result(&res, "check integrity zone");

		free_contig(buf_ptr, buf_size);
	}
}

static void basic_sweep(void)
{
	/* Perform a basic area sweep.
	 */

	test_group("basic area sweep", TRUE);

	sweep_area(cvu64(sector_size));
}

static void high_disk_pos(void)
{
	/* Test 64-bit absolute disk positions. This means that after adding
	 * partition base to the given position, the driver will be dealing
	 * with a position above 32 bit. We want to test the transition area
	 * only; if the entire partition base is above 32 bit, we have already
	 * effectively performed this test many times over. In other words, for
	 * this test, the partition must start below 4GB and end above 4GB,
	 * with at least four sectors on each side.
	 */
	u64_t base_pos;

	base_pos = make64(sector_size * 4, 1L);
	base_pos = sub64u(base_pos, rem64u(base_pos, sector_size));

	/* The partition end must exceed 32 bits. */
	if (cmp64(add64(part.base, part.size), base_pos) < 0) {
		test_group("high disk positions", FALSE);

		return;
	}

	base_pos = sub64u(base_pos, sector_size * 8);

	/* The partition start must not. */
	if (cmp64(base_pos, part.base) < 0) {
		test_group("high disk positions", FALSE);
		return;
	}

	test_group("high disk positions", TRUE);

	base_pos = sub64(base_pos, part.base);

	sweep_and_check(base_pos, !cmp64u(part.base, 0));
}

static void high_part_pos(void)
{
	/* Test 64-bit partition-relative disk positions. In other words, use
	 * within the current partition a position that exceeds a 32-bit value.
	 * This requires the partition to be more than 4GB in size; we need an
	 * additional 4 sectors, to be exact.
	 */
	u64_t base_pos;

	/* If the partition starts at the beginning of the disk, this test is
	 * no different from the high disk position test.
	 */
	if (cmp64u(part.base, 0) == 0) {
		/* don't complain: the test is simply superfluous now */
		return;
	}

	base_pos = make64(sector_size * 4, 1L);
	base_pos = sub64u(base_pos, rem64u(base_pos, sector_size));

	if (cmp64(part.size, base_pos) < 0) {
		test_group("high partition positions", FALSE);

		return;
	}

	test_group("high partition positions", TRUE);

	base_pos = sub64u(base_pos, sector_size * 8);

	sweep_and_check(base_pos, TRUE);
}

static void high_lba_pos1(void)
{
	/* Test 48-bit LBA positions, as opposed to *24-bit*. Drivers that only
	 * support 48-bit LBA ATA transfers, will treat the lower and upper 24
	 * bits differently. This is again relative to the disk start, not the
	 * partition start. For 512-byte sectors, the lowest position exceeding
	 * 24 bit is at 8GB. As usual, we need four sectors more, and fewer, on
	 * the other side. The partition that we're operating on, must cover
	 * this area.
	 */
	u64_t base_pos;

	base_pos = mul64u(1L << 24, sector_size);

	/* The partition end must exceed the 24-bit sector point. */
	if (cmp64(add64(part.base, part.size), base_pos) < 0) {
		test_group("high LBA positions, part one", FALSE);

		return;
	}

	base_pos = sub64u(base_pos, sector_size * 8);

	/* The partition start must not. */
	if (cmp64(base_pos, part.base) < 0) {
		test_group("high LBA positions, part one", FALSE);

		return;
	}

	test_group("high LBA positions, part one", TRUE);

	base_pos = sub64(base_pos, part.base);

	sweep_and_check(base_pos, !cmp64u(part.base, 0));
}

static void high_lba_pos2(void)
{
	/* Test 48-bit LBA positions, as opposed to *28-bit*. That means sector
	 * numbers in excess of 28-bit values; the old ATA upper limit. The
	 * same considerations as above apply, except that we now need a 128+GB
	 * partition.
	 */
	u64_t base_pos;

	base_pos = mul64u(1L << 28, sector_size);

	/* The partition end must exceed the 28-bit sector point. */
	if (cmp64(add64(part.base, part.size), base_pos) < 0) {
		test_group("high LBA positions, part two", FALSE);

		return;
	}

	base_pos = sub64u(base_pos, sector_size * 8);

	/* The partition start must not. */
	if (cmp64(base_pos, part.base) < 0) {
		test_group("high LBA positions, part two", FALSE);

		return;
	}

	test_group("high LBA positions, part two", TRUE);

	base_pos = sub64(base_pos, part.base);

	sweep_and_check(base_pos, !cmp64u(part.base, 0));
}

static void high_pos(void)
{
	/* Check whether the driver deals well with 64-bit positions and
	 * 48-bit LBA addresses. We test three cases: disk byte position beyond
	 * what fits in 32 bit, in-partition byte position beyond what fits in
	 * 32 bit, and disk sector position beyond what fits in 24 bit. With
	 * the partition we've been given, we may not be able to test all of
	 * them (or any, for that matter).
	 */
	/* In certain rare cases, we might be able to perform integrity
	 * checking on the area that would be affected if a 32-bit/24-bit
	 * counter were to wrap. More specifically: we can do that if we can
	 * access the start of the disk. This is why we should be given the
	 * entire disk as test area if at all possible.
	 */

	basic_sweep();

	high_disk_pos();

	high_part_pos();

	high_lba_pos1();

	high_lba_pos2();
}

static void open_primary(void)
{
	/* Open the primary device. This call has its own test group.
	 */

	test_group("device open", TRUE);

	open_device(driver_minor);
}

static void close_primary(void)
{
	/* Close the primary device. This call has its own test group.
	 */

	test_group("device close", TRUE);

	close_device(driver_minor);

	assert(nr_opened == 0);
}

static void do_tests(void)
{
	/* Perform all the tests.
	 */

	open_primary();

	misc_ioctl();

	bad_read1();

	bad_read2();

	/* It is assumed that the driver implementation uses shared
	 * code paths for read and write for the basic checks, so we do
	 * not repeat those for writes.
	 */
	bad_write();

	vector_and_large();

	part_limits();

	unaligned_size();

	unaligned_pos1();

	unaligned_pos2();

	high_pos();

	close_primary();
}

static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	/* Initialize.
	 */
	int r;
	clock_t now;

	if (env_argc > 1)
		optset_parse(optset_table, env_argv[1]);

	if (driver_label[0] == '\0')
		panic("no driver label given");

	if (ds_retrieve_label_endpt(driver_label, &driver_endpt))
		panic("unable to resolve driver label");

	if (driver_minor > 255)
		panic("invalid or no driver minor given");

	if ((r = getuptime(&now)) != OK)
		panic("unable to get uptime: %d", r);

	srand48(now);

	return OK;
}

static void sef_local_startup(void)
{
	/* Initialize the SEF framework.
	 */

	sef_setcb_init_fresh(sef_cb_init_fresh);

	sef_startup();
}

int main(int argc, char **argv)
{
	/* Driver task.
	 */

	env_setargs(argc, argv);
	sef_local_startup();

	printf("BLOCKTEST: driver label '%s' (endpt %d), minor %d\n",
		driver_label, driver_endpt, driver_minor);

	do_tests();

	printf("BLOCKTEST: summary: %d out of %d tests failed "
		"across %d group%s; %d driver deaths\n",
		failed_tests, total_tests, failed_groups,
		failed_groups == 1 ? "" : "s", driver_deaths);

	return 0;
}
