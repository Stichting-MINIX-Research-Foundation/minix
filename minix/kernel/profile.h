#ifndef PROFILE_H
#define PROFILE_H

#include <minix/profile.h> // Kept

#if SPROFILE	/* statistical profiling */

#include "arch_watchdog.h" // Kept (local kernel header)

// Added kernel headers (precautionary for consistency)
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


#define SAMPLE_BUFFER_SIZE	(64 << 20)
extern char sprof_sample_buffer[SAMPLE_BUFFER_SIZE];

EXTERN int sprofiling;			/* whether profiling is running */
EXTERN int sprofiling_type;			/* whether profiling is running */
EXTERN int sprof_mem_size;		/* available user memory for data */
EXTERN struct sprof_info_s sprof_info;	/* profiling info for user program */
EXTERN vir_bytes sprof_data_addr_vir;	/* user address to write data */
EXTERN endpoint_t sprof_ep;		/* user process */

void nmi_sprofile_handler(struct nmi_frame * frame);

#endif /* SPROFILE */

#endif /* PROFILE_H */
