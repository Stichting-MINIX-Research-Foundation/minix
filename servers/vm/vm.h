
#define NO_MEM ((phys_clicks) 0)  /* returned by alloc_mem() with mem is up */

/* Memory flags to pt_allocmap() and alloc_mem(). */
#define PAF_CLEAR	0x01	/* Clear physical memory. */
#define PAF_CONTIG	0x02	/* Physically contiguous. */

/* special value for v in pt_allocmap */
#define AM_AUTO         ((u32_t) -1)

#define CLICK2ABS(v) ((v) << CLICK_SHIFT)
#define ABS2CLICK(a) ((a) >> CLICK_SHIFT)

/* Compile in asserts and custom sanity checks at all? */
#define SANITYCHECKS	0
#define VMSTATS		1

/* If so, this level: */
#define SCL_NONE	0	/* No sanity checks - vm_assert()s only. */
#define SCL_TOP		1	/* Main loop and other high-level places. */
#define SCL_FUNCTIONS	2	/* Function entry/exit. */
#define SCL_DETAIL	3	/* Detailled steps. */
#define SCL_MAX		3	/* Highest value. */

/* Type of page allocations. */
#define VMP_SPARE	0
#define VMP_PAGETABLE	1
#define VMP_PAGEDIR	2
#define VMP_SLAB	3
#define VMP_CATEGORIES	4

/* Flags to pt_writemap(). */
#define WMF_OVERWRITE	0x01	/* Caller knows map may overwrite. */
