/* Function prototypes. */

/* main.c */
_PROTOTYPE( void main, (void)						);

/* putk.c */
_PROTOTYPE( void kputc, (int c)						);

/* diag.c */
_PROTOTYPE( int do_new_kmess, (message *m)				);
_PROTOTYPE( int do_diagnostics, (message *m)				);
_PROTOTYPE( void diag_putc, (int c)					);
_PROTOTYPE( void diagnostics_dmp, (void) 				);

/* dmp.c */
_PROTOTYPE( int do_fkey_pressed, (message *m) 				);

/* dmp_kernel.c */
_PROTOTYPE( void proctab_dmp, (void)					);
_PROTOTYPE( void memmap_dmp, (void)					);
_PROTOTYPE( void sendmask_dmp, (void)					);
_PROTOTYPE( void image_dmp, (void)					);
_PROTOTYPE( void irqtab_dmp, (void)					);
_PROTOTYPE( void kmessages_dmp, (void)					);
_PROTOTYPE( void sched_dmp, (void)					);
_PROTOTYPE( void monparams_dmp, (void)					);
_PROTOTYPE( void kenv_dmp, (void)					);
_PROTOTYPE( void memchunks_dmp, (void)					);
_PROTOTYPE( void timing_dmp, (void)					);

/* dmp_pm.c */
_PROTOTYPE( void mproc_dmp, (void)					);

/* dmp_fs.c */
_PROTOTYPE( void dtab_dmp, (void)					);
_PROTOTYPE( void fproc_dmp, (void)					);

