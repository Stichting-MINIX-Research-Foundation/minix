#ifndef _DS_PROTO_H
#define _DS_PROTO_H

/* Function prototypes. */

/* main.c */
int main(int argc, char **argv);

/* store.c */
int do_publish(message *m_ptr);
int do_retrieve(message *m_ptr);
int do_retrieve_label(const message *m_ptr);
int do_subscribe(message *m_ptr);
int do_check(message *m_ptr);
int do_delete(message *m_ptr);
int do_snapshot(message *m_ptr);
int do_getsysinfo(const message *m_ptr);
int sef_cb_init_fresh(int type, sef_init_info_t *info);

#endif
