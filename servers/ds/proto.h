#ifndef _DS_PROTO_H
#define _DS_PROTO_H

/* Function prototypes. */

/* main.c */
_PROTOTYPE(int main, (int argc, char **argv));

/* store.c */
_PROTOTYPE(int do_publish, (message *m_ptr));
_PROTOTYPE(int do_retrieve, (message *m_ptr));
_PROTOTYPE(int do_retrieve_label, (const message *m_ptr));
_PROTOTYPE(int do_subscribe, (message *m_ptr));
_PROTOTYPE(int do_check, (message *m_ptr));
_PROTOTYPE(int do_delete, (message *m_ptr));
_PROTOTYPE(int do_snapshot, (message *m_ptr));
_PROTOTYPE(int do_getsysinfo, (const message *m_ptr));
_PROTOTYPE(int sef_cb_init_fresh, (int type, sef_init_info_t *info));

#endif
