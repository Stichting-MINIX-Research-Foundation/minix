/* LWIP service - mempool.c - memory pool management and slab allocation */
/*
 * This module should be considered a replacement for lwIP's PBUF_POOL and
 * custom-pools functionality.  lwIP's PBUF_POOL system allows a PBUF_POOL type
 * allocation for a moderately large amount of memory, for example for a full-
 * sized packet, to be turned into a chain of "pbuf" buffers, each of a static
 * size.  Most of lwIP can deal with such pbuf chains, because many other types
 * of allocations also end up consisting of pbuf chains.  However, lwIP will
 * never use PBUF_POOL for its own memory allocations, and use PBUF_RAM
 * allocations instead.  Such PBUF_RAM allocations always return one single
 * pbuf with a contiguous memory area.  lwIP's custom pools support allows such
 * PBUF_RAM allocations to draw from user-defined pools of statically allocated
 * memory, as an alternative to turning such allocations into malloc() calls.
 *
 * However, lwIP itself does not offer a way to combine these two pool systems:
 * the PBUF_POOL buffer pool and the custom pools are completely separate.  We
 * want to be able to draw both kinds of memory from the same pool.  This is
 * the first reason that we are using our own memory pools.  The second is
 * something that lwIP could never offer anyway: we would like to provide a
 * certain amount of static/preallocated memory for those types of allocations,
 * but optionally also add a much larger amount of dynamic memory when needed.
 *
 * In order to make this module work, we do not use PBUF_POOL anywhere.
 * Instead, we use chained static-sized PBUF_RAM allocations for all types of
 * allocations that we manage ourselves--see pchain_alloc().  We tell lwIP to
 * use the functions in this module to do the malloc-type allocations for those
 * PBUF_RAM buffers.  As such, this module manages all PBUF_RAM allocations,
 * both from our own code and from lwIP.  Note that we do still use lwIP's own
 * pools for various lwIP structures.  We do want to keep the isolation
 * provided by the use of such pools, even though that means that we have to
 * provision some of those pools for the worst case, resulting in some memory
 * overhead that is unnecessary for the common case.
 *
 * With PBUF_RAM allocation redirection system in place, this module has to
 * manage the memory for those allocations.  It does this based on the
 * assertion that there are three main classes of PBUF_RAM allocation sizes:
 *
 * - "large" allocations: these are allocations for up to MEMPOOL_BUFSIZE bytes
 *   of PBUF_RAM data, where MEMPOOL_BUFSIZE is the allocation granularity that
 *   we have picked for the individual buffers in larger chains.  It is set to
 *   512 bytes right now, mainly to keep pbuf chains for full-sized ethernet
 *   packets short, which has many performance advantages.  Since the pbuf
 *   header itself also takes some space (16 bytes, right now), this results in
 *   allocations seen by mempool_malloc() of up to just over 512 bytes.
 * - "small" allocations: these are allocations mostly for packet headers, as
 *   needed by lwIP to prepend to (mainly TCP) packet data that we give to it.
 *   The size of these allocations varies, but most are 76 bytes (80 bytes if
 *   we ever add VLAN support), plus once again the pbuf header.
 * - "excessive" allocations: these are allocations larger than the maximum
 *   we have configured, effectively requesting contiguous memory of (possibly
 *   far) more than 512 bytes.  We do not make such allocations ourselves, as
 *   we only ever create pbuf chains.  Thus, any such allocations come from
 *   lwIP.  There are a few locations in lwIP that attempt to make those kinds
 *   of allocations, but we replace one important case in the lwIP code with
 *   a chained allocation, (currently) leaving only one case: allocation of
 *   ICMP ping reply packets.  In this module, we outright *deny* any excessive
 *   allocations.  Practically, that means that no replies are generated for
 *   requests exceeding around 460 bytes, which is in fact not bad, especially
 *   since we have multicast ICMP ping replying enabled.  If any new cases of
 *   excessive allocations are added to lwIP in the future, we will have to
 *   deal with those on a case-by-case basis, but for now this should be all.
 *
 * This module caters to the first two types of allocations.  For large buffer
 * allocations, it provides a standard slab allocator, with a hardcoded slab
 * size of MEMPOOL_LARGE_COUNT buffers with a 512-byte data area each.  One
 * slab is allocated at service start-up; additional slabs up to a configured
 * maximum are allocated on demand.  Once fallen out of use, all but one slabs
 * will be freed after a while, using a timer.  The current per-slab count of
 * 512 large buffers, combined with the buffer size of 512 plus the pbuf header
 * plus a bit of extra overhead, results in about 266 KB per slab.
 *
 * For small buffer allocations, there are two facilities.  First, there is a
 * static pool of small buffers.  This pool currently provides 256 small-sized
 * buffers, mainly in order to allow packet headers to be produced even in low-
 * memory conditions.  In addition, small buffers may be formed by allocating
 * and then splitting up one large buffer.  The module is currently configured
 * to split one large buffer into four small buffers, which yields a small
 * buffer size of just over 100 bytes--enough for the packet headers while
 * leaving little slack on either side.
 *
 * It is important to note that large and small buffer allocations are freed up
 * through the same function, with no information on the original allocation
 * size.  As a result, we have to distinguish between large and small buffers
 * using a unified system.  In particular, this module prepends each of its
 * allocations by a single pointer, which points to a header structure that is
 * at the very beginning of the slab that contains the allocated buffer.  That
 * header structure contains information about the type of slab (large or
 * small) as well as some accounting information used by both types.
 *
 * For large-buffer slabs, this header is part of a larger structure with for
 * example the slab's list of free buffers.  This larger structure is then
 * followed by the actual buffers in the slab.
 *
 * For small-buffer slabs, the header is followed directly by the actual small
 * buffers.  Thus, when a large buffer is split up into four small buffers, the
 * data area of that large buffer consists of a small-type slab header and four
 * small buffers.  The large buffer itself is simply considered in use, as
 * though it was allocated for regular data.  This nesting approach saves a lot
 * of memory for small allocations, at the cost of a bit more computation.
 *
 * It should be noted that all allocations should be (and are) pointer-aligned.
 * Normally lwIP would check for this, but we cannot tell lwIP the platform
 * pointer size without hardcoding that size.  This module performs proper
 * alignment of all buffers itself though, regardless of the pointer size.
 */

#include "lwip.h"

#include <sys/mman.h>

/* Alignment to pointer sizes. */
#define MEMPOOL_ALIGN_DOWN(s)	((s) & ~(sizeof(void *) - 1))
#define MEMPOOL_ALIGN_UP(s)	MEMPOOL_ALIGN_DOWN((s) + sizeof(void *) - 1)

/* Large buffers: per-slab count and data area size. */
#define MEMPOOL_LARGE_COUNT	512
#define MEMPOOL_LARGE_SIZE	\
    (MEMPOOL_ALIGN_UP(sizeof(struct pbuf)) + MEMPOOL_BUFSIZE)

/* Small buffers: per-slab count and data area size. */
#define MEMPOOL_SMALL_COUNT	4
#define MEMPOOL_SMALL_SIZE	\
    (MEMPOOL_ALIGN_DOWN(MEMPOOL_LARGE_SIZE / MEMPOOL_SMALL_COUNT) - \
     sizeof(struct mempool_header))

/* Memory pool slab header, part of both small and large slabs. */
struct mempool_header {
	union {
		struct {
			uint8_t mhui_flags;
			uint32_t mhui_inuse;
		} mhu_info;
		void *mhu_align;	/* force pointer alignment */
	} mh_u;
};
#define mh_flags mh_u.mhu_info.mhui_flags
#define mh_inuse mh_u.mhu_info.mhui_inuse

/* Header flags. */
#define MHF_SMALL	0x01	/* slab is for small buffers, not large ones */
#define MHF_STATIC	0x02	/* small slab is statically allocated */
#define MHF_MARKED	0x04	/* large empty slab is up for deallocation */

/*
 * Large buffer.  When allocated, mlb_header points to the (header of) the
 * containing large slab, and mlb_data is returned for arbitrary use by the
 * user of the buffer.  When free, mlb_header is NULL and instead mlb_header2
 * points to the containing slab (allowing for double-free detection), and the
 * buffer is on the slab's free list by using mlb_next.
 */
struct mempool_large_buf {
	struct mempool_header *mlb_header;
	union {
		struct {
			struct mempool_header *mlbuf_header2;
			LIST_ENTRY(mempool_large_buf) mlbuf_next;
		} mlbu_free;
		char mlbu_data[MEMPOOL_LARGE_SIZE];
	} mlb_u;
};
#define mlb_header2 mlb_u.mlbu_free.mlbuf_header2
#define mlb_next mlb_u.mlbu_free.mlbuf_next
#define mlb_data mlb_u.mlbu_data

/* Small buffer.  Same idea, different size. */
struct mempool_small_buf {
	struct mempool_header *msb_header;
	union {
		struct {
			struct mempool_header *msbuf_header2;
			TAILQ_ENTRY(mempool_small_buf) msbuf_next;
		} msbu_free;
		char msbu_data[MEMPOOL_SMALL_SIZE];
	} msb_u;
};
#define msb_header2 msb_u.msbu_free.msbuf_header2
#define msb_next msb_u.msbu_free.msbuf_next
#define msb_data msb_u.msbu_data

/*
 * A large slab, including header, other per-slab fields, and large buffers.
 * Each of these structures is on exactly one of three slab lists, depending
 * on whether all its buffers are free (empty), some but not all of its buffers
 * are in use (partial), or all of its buffers are in use (full).  The mls_next
 * field is used for that list.  The mls_free field is the per-slab list of
 * free buffers.
 */
struct mempool_large_slab {
	struct mempool_header mls_header;		/* MUST be first */
	LIST_ENTRY(mempool_large_slab) mls_next;
	LIST_HEAD(, mempool_large_buf) mls_free;
	struct mempool_large_buf mls_buf[MEMPOOL_LARGE_COUNT];
};

/* The three slab lists for large slabs, as described above. */
static LIST_HEAD(, mempool_large_slab) mempool_empty_slabs;
static LIST_HEAD(, mempool_large_slab) mempool_partial_slabs;
static LIST_HEAD(, mempool_large_slab) mempool_full_slabs;

/*
 * A small slab, including header and small buffers.  We use unified free lists
 * for small buffers, and these small slabs are not part of any lists
 * themselves, so we need neither of the two fields from large slabs for that.
 */
struct mempool_small_slab {
	struct mempool_header mss_header;		/* MUST be first */
	struct mempool_small_buf mss_buf[MEMPOOL_SMALL_COUNT];
};

/*
 * The free lists for static small buffers (from the static pool, see below)
 * and dynamic small buffers (as obtained by splitting large buffers).
 */
static TAILQ_HEAD(, mempool_small_buf) mempool_small_static_freelist;
static TAILQ_HEAD(, mempool_small_buf) mempool_small_dynamic_freelist;

/*
 * A static pool of small buffers.  Small buffers are somewhat more important
 * than large buffers, because they are used for packet headers.  The purpose
 * of this static pool is to be able to make progress even if all large buffers
 * are allocated for data, typically in the case that the system is low on
 * memory.  Note that the number of static small buffers is the given number of
 * small slabs multiplied by MEMPOOL_SMALL_COUNT, hence the division.
 */
#define MEMPOOL_SMALL_SLABS	(256 / MEMPOOL_SMALL_COUNT)

static struct mempool_small_slab mempool_small_pool[MEMPOOL_SMALL_SLABS];

/*
 * The following setting (mempool_max_slabs) can be changed through sysctl(7).
 * As such it may be set by userland to a completely arbitrary value and must
 * be sanity-checked before any actual use.  The default is picked such that
 * all TCP sockets can fill up their send and receive queues: (TCP_SNDBUF_DEF +
 * TCP_RCVBUF_DEF) * NR_TCPSOCK / (MEMPOOL_BUFSIZE * MEMPOOL_LARGE_COUNT) =
 * (32768 + 32768) * 256 / (512 * 512) = 64.  We put in the resulting number
 * rather than the formula because not all those definitions are public.
 */
#define MEMPOOL_DEFAULT_MAX_SLABS	64	/* about 17 MB of memory */

static int mempool_max_slabs;	/* maximum number of large slabs */
static int mempool_nr_slabs;	/* current number of large slabs */

static int mempool_nr_large;	/* current number of large buffers */
static int mempool_used_large;	/* large buffers currently in use */
static int mempool_used_small;	/* small buffers currently in use */

/*
 * Number of clock ticks between timer invocations.  The timer is used to
 * deallocate unused slabs.
 */
#define MEMPOOL_TIMER_TICKS	(10 * sys_hz())

static minix_timer_t mempool_timer;

static int mempool_defer_alloc;		/* allocation failed, defer next try */

/* The CTL_MINIX MINIX_LWIP "mempool" subtree.  Dynamically numbered. */
static struct rmib_node minix_lwip_mempool_table[] = {
	RMIB_INTPTR(RMIB_RW, &mempool_max_slabs, "slab_max",
	    "Maximum number of memory slabs (configurable)"),
	RMIB_INTPTR(RMIB_RO, &mempool_nr_slabs, "slab_num",
	    "Current number of memory slabs"),
	RMIB_INT(RMIB_RO, sizeof(struct mempool_large_slab), "slab_size",
	    "Byte size of a single memory slab"),
	RMIB_INT(RMIB_RO, MEMPOOL_LARGE_COUNT, "slab_bufs",
	    "Number of large buffers per memory slab"),
	RMIB_INTPTR(RMIB_RO, &mempool_nr_large, "large_num",
	    "Current total number of large buffers"),
	RMIB_INTPTR(RMIB_RO, &mempool_used_large, "large_used",
	    "Current number of used large buffers"),
	RMIB_INT(RMIB_RO, MEMPOOL_LARGE_SIZE, "large_size",
	    "Byte size of a single large buffer"),
	RMIB_INTPTR(RMIB_RO, &mempool_used_small, "small_used",
	    "Current number of used small buffers"),
	RMIB_INT(RMIB_RO, MEMPOOL_SMALL_SIZE, "small_size",
	    "Byte size of a single small buffer"),
};

static struct rmib_node minix_lwip_mempool_node =
    RMIB_NODE(RMIB_RO, minix_lwip_mempool_table, "mempool",
	"Memory pool settings");

/*
 * Initialize the given "slab" of small buffers.  The slab may either come from
 * the statically allocated pool ('is_static' is TRUE) or a single large buffer
 * that we aim to chop up into small buffers.
 */
static void
mempool_prepare_small(struct mempool_small_slab * mss, int is_static)
{
	struct mempool_small_buf *msb;
	unsigned int count;

	mss->mss_header.mh_flags = MHF_SMALL | ((is_static) ? MHF_STATIC : 0);
	mss->mss_header.mh_inuse = 0;

	msb = mss->mss_buf;

	for (count = 0; count < MEMPOOL_SMALL_COUNT; count++, msb++) {
		msb->msb_header = NULL;
		msb->msb_header2 = &mss->mss_header;

		if (is_static)
			TAILQ_INSERT_HEAD(&mempool_small_static_freelist, msb,
			    msb_next);
		else
			TAILQ_INSERT_HEAD(&mempool_small_dynamic_freelist, msb,
			    msb_next);
	}
}

/*
 * Allocate a new slab for large buffers, if allowed by policy and possible.
 */
static void
mempool_new_slab(void)
{
	struct mempool_large_slab *mls;
	struct mempool_large_buf *mlb;
	unsigned int count;

	/*
	 * See if allocating a new slab would result in overrunning the
	 * configured maximum number of large buffers.  Round the maximum,
	 * which is probably what the user intended.
	 */
	if (mempool_cur_buffers() + MEMPOOL_LARGE_COUNT / 2 >
	    mempool_max_buffers()) {
		assert(mempool_nr_slabs > 0);

		return;
	}

	/*
	 * If a previous allocation failed before during this timer interval,
	 * do not try again now.
	 */
	if (mempool_defer_alloc)
		return;

	/*
	 * Allocate the slab.  Preallocate the memory, or we might crash later
	 * during low-memory conditions.  If allocation fails, simply do
	 * nothing further.  The caller will check the free lists.
	 */
	mls = (struct mempool_large_slab *)mmap(NULL,
	    sizeof(struct mempool_large_slab), PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE | MAP_PREALLOC, -1, 0);

	if (mls == MAP_FAILED) {
		if (mempool_nr_slabs == 0)
			panic("unable to allocate initial memory pool");

		/*
		 * Do not keep hammering VM with mmap requests when the system
		 * is out of memory.  Try again after the next timer tick.
		 */
		mempool_defer_alloc = TRUE;

		return;
	}

	/* Initialize the new slab. */
	mls->mls_header.mh_flags = 0;
	mls->mls_header.mh_inuse = 0;

	mlb = mls->mls_buf;

	LIST_INIT(&mls->mls_free);

	for (count = 0; count < MEMPOOL_LARGE_COUNT; count++, mlb++) {
		mlb->mlb_header = NULL;
		mlb->mlb_header2 = &mls->mls_header;

		LIST_INSERT_HEAD(&mls->mls_free, mlb, mlb_next);
	}

	LIST_INSERT_HEAD(&mempool_empty_slabs, mls, mls_next);

	mempool_nr_slabs++;
	mempool_nr_large += MEMPOOL_LARGE_COUNT;
}

/*
 * Deallocate a slab for large buffers, if allowed.
 */
static void
mempool_destroy_slab(struct mempool_large_slab * mls)
{

	assert(mempool_nr_slabs > 0);

	assert(!(mls->mls_header.mh_flags & MHF_SMALL));
	assert(mls->mls_header.mh_inuse == 0);

	/* Never deallocate the last large slab. */
	if (mempool_nr_slabs == 1)
		return;

	LIST_REMOVE(mls, mls_next);

	if (munmap(mls, sizeof(*mls)) != 0)
		panic("munmap failed: %d", -errno);

	assert(mempool_nr_large > MEMPOOL_LARGE_COUNT);
	mempool_nr_large -= MEMPOOL_LARGE_COUNT;
	mempool_nr_slabs--;
}

/*
 * Regular timer.  Deallocate empty slabs already marked for deallocation, and
 * mark any other empty slabs for deallocation.
 */
static void
mempool_tick(int arg __unused)
{
	struct mempool_large_slab *mls, *tmls;

	/*
	 * Go through all the empty slabs, destroying marked slabs and marking
	 * unmarked slabs.
	 */
	LIST_FOREACH_SAFE(mls, &mempool_empty_slabs, mls_next, tmls) {
		if (mls->mls_header.mh_flags & MHF_MARKED)
			mempool_destroy_slab(mls);
		else
			mls->mls_header.mh_flags |= MHF_MARKED;
	}

	/*
	 * If allocation failed during the last interval, allow a new attempt
	 * during the next.
	 */
	mempool_defer_alloc = FALSE;

	/* Set the next timer. */
	set_timer(&mempool_timer, MEMPOOL_TIMER_TICKS, mempool_tick, 0);
}

/*
 * Initialize the memory pool module.
 */
void
mempool_init(void)
{
	unsigned int slot;

	/* These checks are for absolutely essential points. */
	assert(sizeof(void *) == MEM_ALIGNMENT);
	assert(sizeof(struct mempool_small_slab) <= MEMPOOL_LARGE_SIZE);
	assert(offsetof(struct mempool_small_buf, msb_data) == sizeof(void *));
	assert(offsetof(struct mempool_large_buf, mlb_data) == sizeof(void *));

	/* Initialize module-local variables. */
	LIST_INIT(&mempool_empty_slabs);
	LIST_INIT(&mempool_partial_slabs);
	LIST_INIT(&mempool_full_slabs);

	TAILQ_INIT(&mempool_small_static_freelist);
	TAILQ_INIT(&mempool_small_dynamic_freelist);

	mempool_max_slabs = MEMPOOL_DEFAULT_MAX_SLABS;
	mempool_nr_slabs = 0;

	mempool_nr_large = 0;
	mempool_used_large = 0;
	mempool_used_small = 0;

	mempool_defer_alloc = FALSE;

	/* Initialize the static pool of small buffers. */
	for (slot = 0; slot < __arraycount(mempool_small_pool); slot++)
		mempool_prepare_small(&mempool_small_pool[slot],
		    TRUE /*is_static*/);

	/*
	 * Allocate one large slab.  The service needs at least one large slab
	 * for basic operation, and therefore will never deallocate the last.
	 */
	mempool_new_slab();

	/* Set a regular low-frequency timer to deallocate unused slabs. */
	set_timer(&mempool_timer, MEMPOOL_TIMER_TICKS, mempool_tick, 0);

	/* Register the minix.lwip.mempool subtree. */
	mibtree_register_lwip(&minix_lwip_mempool_node);
}

/*
 * Return the total number of large buffers currently in the system, regardless
 * of allocation status.
 */
unsigned int
mempool_cur_buffers(void)
{

	return mempool_nr_large;
}

/*
 * Return the maximum number of large buffers that the system has been allowed
 * to allocate.  Note that due to low-memory conditions, this maximum may not
 * be allocated in practice even when desired.
 */
unsigned int
mempool_max_buffers(void)
{

	if (mempool_max_slabs <= 1)
		return MEMPOOL_LARGE_COUNT;

	if ((size_t)mempool_max_slabs >
	    INT_MAX / sizeof(struct mempool_large_slab))
		return INT_MAX / sizeof(struct mempool_large_slab);

	return (size_t)mempool_max_slabs * MEMPOOL_LARGE_COUNT;
}

/*
 * Allocate a large buffer, either by taking one off a free list or by
 * allocating a new large slab.  On success, return a pointer to the data area
 * of the large buffer.  This data area is exactly MEMPOOL_LARGE_SIZE bytes in
 * size.  If no large buffer could be allocated, return NULL.
 */
static void *
mempool_alloc_large(void)
{
	struct mempool_large_slab *mls;
	struct mempool_large_buf *mlb;

	/*
	 * Find a large slab that has free large blocks.  As is standard for
	 * slab allocation, favor partially used slabs over empty slabs for
	 * eventual consolidation.  If both lists are empty, try allocating a
	 * new slab.  If that fails, we are out of memory, and return NULL.
	 */
	if (!LIST_EMPTY(&mempool_partial_slabs))
		mls = LIST_FIRST(&mempool_partial_slabs);
	else {
		if (LIST_EMPTY(&mempool_empty_slabs)) {
			mempool_new_slab();

			if (LIST_EMPTY(&mempool_empty_slabs))
				return NULL; /* out of memory */
		}

		mls = LIST_FIRST(&mempool_empty_slabs);
	}

	/* Allocate a block from the slab that we picked. */
	assert(mls != NULL);
	assert(!LIST_EMPTY(&mls->mls_free));

	mlb = LIST_FIRST(&mls->mls_free);
	LIST_REMOVE(mlb, mlb_next);

	assert(mlb->mlb_header == NULL);
	assert(mlb->mlb_header2 == &mls->mls_header);

	mlb->mlb_header = &mls->mls_header;

	/*
	 * Adjust accounting for the large slab, which may involve moving it
	 * to another list.
	 */
	assert(mls->mls_header.mh_inuse < MEMPOOL_LARGE_COUNT);
	mls->mls_header.mh_inuse++;

	if (mls->mls_header.mh_inuse == MEMPOOL_LARGE_COUNT) {
		LIST_REMOVE(mls, mls_next);

		LIST_INSERT_HEAD(&mempool_full_slabs, mls, mls_next);
	} else if (mls->mls_header.mh_inuse == 1) {
		LIST_REMOVE(mls, mls_next);

		LIST_INSERT_HEAD(&mempool_partial_slabs, mls, mls_next);
	}

	assert(mempool_used_large < mempool_nr_large);
	mempool_used_large++;

	/* Return the block's data area. */
	return (void *)mlb->mlb_data;
}

/*
 * Allocate a small buffer, either by taking one off a free list or by
 * allocating a large buffer and splitting it up in new free small buffers.  On
 * success, return a pointer to the data area of the small buffer.  This data
 * area is exactly MEMPOOL_SMALL_SIZE bytes in size.  If no small buffer could
 * be allocated, return NULL.
 */
static void *
mempool_alloc_small(void)
{
	struct mempool_small_slab *mss;
	struct mempool_small_buf *msb;
	struct mempool_header *mh;

	/*
	 * Find a free small block and take it off the free list.  Try the
	 * static free list before the dynamic one, so that after a peak in
	 * buffer usage we are likely to be able to free up the dynamic slabs
	 * quickly.  If both lists are empty, try allocating a large block to
	 * divvy up into small blocks.  If that fails, we are out of memory.
	 */
	if (!TAILQ_EMPTY(&mempool_small_static_freelist)) {
		msb = TAILQ_FIRST(&mempool_small_static_freelist);

		TAILQ_REMOVE(&mempool_small_static_freelist, msb, msb_next);
	} else {
		if (TAILQ_EMPTY(&mempool_small_dynamic_freelist)) {
			mss =
			    (struct mempool_small_slab *)mempool_alloc_large();

			if (mss == NULL)
				return NULL; /* out of memory */

			/* Initialize the small slab, including its blocks. */
			mempool_prepare_small(mss, FALSE /*is_static*/);
		}

		msb = TAILQ_FIRST(&mempool_small_dynamic_freelist);
		assert(msb != NULL);

		TAILQ_REMOVE(&mempool_small_dynamic_freelist, msb, msb_next);
	}

	/* Mark the small block as allocated, and return its data area. */
	assert(msb != NULL);

	assert(msb->msb_header == NULL);
	assert(msb->msb_header2 != NULL);

	mh = msb->msb_header2;
	msb->msb_header = mh;

	assert(mh->mh_inuse < MEMPOOL_SMALL_COUNT);
	mh->mh_inuse++;

	mempool_used_small++;

	return (void *)msb->msb_data;
}

/*
 * Memory pool wrapper function for malloc() calls from lwIP.
 */
void *
mempool_malloc(size_t size)
{

	/*
	 * It is currently expected that there will be allocation attempts for
	 * sizes larger than our large size, in particular for ICMP ping
	 * replies as described elsewhere.  As such, we cannot print any
	 * warnings here.  For now, refusing these excessive allocations should
	 * not be a problem in practice.
	 */
	if (size > MEMPOOL_LARGE_SIZE)
		return NULL;

	if (size <= MEMPOOL_SMALL_SIZE)
		return mempool_alloc_small();
	else
		return mempool_alloc_large();
}

/*
 * Memory pool wrapper function for free() calls from lwIP.
 */
void
mempool_free(void * ptr)
{
	struct mempool_large_slab *mls;
	struct mempool_large_buf *mlb;
	struct mempool_small_slab *mss;
	struct mempool_small_buf *msb;
	struct mempool_header *mh;
	unsigned int count;

	/*
	 * Get a pointer to the slab header, which is right before the data
	 * area for both large and small buffers.  This pointer is NULL if the
	 * buffer is free, which would indicate that something is very wrong.
	 */
	ptr = (void *)((char *)ptr - sizeof(mh));

	memcpy(&mh, ptr, sizeof(mh));

	if (mh == NULL)
		panic("mempool_free called on unallocated object!");

	/*
	 * If the slab header says that the slab is for small buffers, deal
	 * with that case first.  If we free up the last small buffer of a
	 * dynamically allocated small slab, we also free up the entire small
	 * slab, which is in fact the data area of a large buffer.
	 */
	if (mh->mh_flags & MHF_SMALL) {
		/*
		 * Move the small buffer onto the appropriate small free list.
		 */
		msb = (struct mempool_small_buf *)ptr;

		msb->msb_header2 = mh;
		msb->msb_header = NULL;

		/*
		 * Simple heuristic, unless the buffer is static: favor reuse
		 * of small buffers in containers that are already in use
		 * for other small buffers as well, for consolidation.
		 */
		if (mh->mh_flags & MHF_STATIC)
			TAILQ_INSERT_HEAD(&mempool_small_static_freelist, msb,
			    msb_next);
		else if (mh->mh_inuse > 1)
			TAILQ_INSERT_HEAD(&mempool_small_dynamic_freelist, msb,
			    msb_next);
		else
			TAILQ_INSERT_TAIL(&mempool_small_dynamic_freelist, msb,
			    msb_next);

		assert(mh->mh_inuse > 0);
		mh->mh_inuse--;

		assert(mempool_used_small > 0);
		mempool_used_small--;

		/*
		 * If the small buffer is statically allocated, or it was not
		 * the last allocated small buffer in its containing large
		 * buffer, then we are done.
		 */
		if (mh->mh_inuse > 0 || (mh->mh_flags & MHF_STATIC))
			return;

		/*
		 * Otherwise, free the containing large buffer as well.  First,
		 * remove all its small buffers from the free list.
		 */
		mss = (struct mempool_small_slab *)mh;
		msb = mss->mss_buf;

		for (count = 0; count < MEMPOOL_SMALL_COUNT; count++, msb++) {
			assert(msb->msb_header == NULL);
			assert(msb->msb_header2 == mh);

			TAILQ_REMOVE(&mempool_small_dynamic_freelist, msb,
			    msb_next);
		}

		/* Then, fall through to the large-buffer free code. */
		ptr = (void *)((char *)mh - sizeof(mh));

		memcpy(&mh, ptr, sizeof(mh));

		assert(mh != NULL);
		assert(!(mh->mh_flags & MHF_SMALL));
	}

	/*
	 * Move the large buffer onto the free list of the large slab to which
	 * it belongs.
	 */
	mls = (struct mempool_large_slab *)mh;
	mlb = (struct mempool_large_buf *)ptr;

	mlb->mlb_header2 = &mls->mls_header;
	mlb->mlb_header = NULL;

	LIST_INSERT_HEAD(&mls->mls_free, mlb, mlb_next);

	/*
	 * Adjust accounting for the large slab, which may involve moving it
	 * to another list.
	 */
	assert(mls->mls_header.mh_inuse > 0);
	mls->mls_header.mh_inuse--;

	if (mls->mls_header.mh_inuse == 0) {
		LIST_REMOVE(mls, mls_next);

		LIST_INSERT_HEAD(&mempool_empty_slabs, mls, mls_next);

		mls->mls_header.mh_flags &= ~MHF_MARKED;
	} else if (mls->mls_header.mh_inuse == MEMPOOL_LARGE_COUNT - 1) {
		LIST_REMOVE(mls, mls_next);

		LIST_INSERT_HEAD(&mempool_partial_slabs, mls, mls_next);
	}

	assert(mempool_used_large > 0);
	mempool_used_large--;
}

/*
 * Memory pool wrapper function for calloc() calls from lwIP.
 */
void *
mempool_calloc(size_t num, size_t size)
{
	void *ptr;
	size_t total;

	/*
	 * Standard overflow check.  This can be improved, but it doesn't have
	 * to be, because in practice lwIP never calls calloc() anyway.
	 */
	if (num > 0 && size > 0 && (size_t)-1 / size < num)
		return NULL;

	total = num * size;

	if ((ptr = mempool_malloc(total)) == NULL)
		return NULL;

	memset(ptr, 0, total);

	return ptr;
}
