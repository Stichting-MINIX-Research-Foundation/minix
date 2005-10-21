/* Function prototypes. */

/* main.c */
_PROTOTYPE( int main, (void));

/* manager.c */
_PROTOTYPE( int do_up, (message *m));
_PROTOTYPE( int do_down, (message *m));
_PROTOTYPE( int do_refresh, (message *m));
_PROTOTYPE( int do_rescue, (message *m));
_PROTOTYPE( int do_shutdown, (message *m));
_PROTOTYPE( void do_period, (message *m));
_PROTOTYPE( void do_exit, (message *m));
_PROTOTYPE( int do_getsysinfo, (message *m));


