/* Global variables used in RS.
 */
#ifndef RS_GLO_H
#define RS_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

#include <minix/param.h>

/* The boot image priv table. This table has entries for all system
 * services in the boot image.
 */
extern struct boot_image_priv boot_image_priv_table[];

/* The boot image sys table. This table has entries for system services in
 * the boot image that override default sys properties.
 */
extern struct boot_image_sys boot_image_sys_table[];

/* The boot image dev table. This table has entries for system services in
 * the boot image that support dev properties.
 */
extern struct boot_image_dev boot_image_dev_table[];

/* The system process table. This table only has entries for system
 * services (servers and drivers), and thus is not directly indexed by
 * slot number. The size of the table must match the size of the privilege
 * table in the kernel.
 */
EXTERN struct rprocpub rprocpub[NR_SYS_PROCS];  /* public entries */
EXTERN struct rproc rproc[NR_SYS_PROCS];
EXTERN struct rproc *rproc_ptr[NR_PROCS];       /* mapping for fast access */

/* Global init descriptor. This descriptor holds data to initialize system
 * services.
 */
EXTERN sef_init_info_t rinit;

/* Global update descriptor. This descriptor holds data when a live update
 * is in progress.
 */
EXTERN struct rupdate rupdate;

/* Enable/disable verbose output. */
EXTERN long rs_verbose;

/* Set when we are shutting down. */
EXTERN int shutting_down;

EXTERN unsigned system_hz;

EXTERN struct machine machine;		/* machine info */

#endif /* RS_GLO_H */

