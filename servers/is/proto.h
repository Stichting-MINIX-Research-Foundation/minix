/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */

/* main.c */
_PROTOTYPE( void main, (void)						);
_PROTOTYPE( void report, (char *, int n)				);

/* putk.c */
_PROTOTYPE( void kputc, (int c)						);

/* diag.c */
_PROTOTYPE( int do_new_kmess, (message *m)				);
_PROTOTYPE( int do_diagnostics, (message *m)				);
_PROTOTYPE( void diag_putc, (int c)					);

/* dmp.c */
_PROTOTYPE( int do_fkey_pressed, (message *m)				);


