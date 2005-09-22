/* Function prototypes. */

/* main.c */
_PROTOTYPE( int  main, (int argc, char **argv)				);

/* dmp.c */
_PROTOTYPE( int do_fkey_pressed, (message *m) 				);
_PROTOTYPE( void mapping_dmp, (void)	 				);

/* dmp_kernel.c */
_PROTOTYPE( void proctab_dmp, (void)					);
_PROTOTYPE( void memmap_dmp, (void)					);
_PROTOTYPE( void privileges_dmp, (void)					);
_PROTOTYPE( void sendmask_dmp, (void)					);
_PROTOTYPE( void image_dmp, (void)					);
_PROTOTYPE( void irqtab_dmp, (void)					);
_PROTOTYPE( void kmessages_dmp, (void)					);
_PROTOTYPE( void sched_dmp, (void)					);
_PROTOTYPE( void monparams_dmp, (void)					);
_PROTOTYPE( void kenv_dmp, (void)					);
_PROTOTYPE( void timing_dmp, (void)					);

/* dmp_pm.c */
_PROTOTYPE( void mproc_dmp, (void)					);
_PROTOTYPE( void sigaction_dmp, (void)					);

/* dmp_fs.c */
_PROTOTYPE( void dtab_dmp, (void)					);
_PROTOTYPE( void fproc_dmp, (void)					);

