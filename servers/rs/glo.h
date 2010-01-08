/* Global variables used in RS.
 */
#ifndef RS_GLO_H
#define RS_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

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
 * slot number.
 */
EXTERN struct rprocpub rprocpub[NR_SYS_PROCS];  /* public entries */
EXTERN struct rproc rproc[NR_SYS_PROCS];
EXTERN struct rproc *rproc_ptr[NR_PROCS];       /* mapping for fast access */

/* Pipe for detection of exec failures. The pipe is close-on-exec, and
 * no data will be written to the pipe if the exec succeeds. After an 
 * exec failure, the slot number is written to the pipe. After each exit,
 * a non-blocking read retrieves the slot number from the pipe.
 */
EXTERN int exec_pipe[2];

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

#endif /* RS_GLO_H */

