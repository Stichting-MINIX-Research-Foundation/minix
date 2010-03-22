
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

EXTERN message m_in;			/* request message */
EXTERN message m_out;			/* reply message */
EXTERN struct state state;		/* global state */
EXTERN struct opt opt;			/* global options */

extern _PROTOTYPE( int (*call_vec[]), (void) );
