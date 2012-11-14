#ifndef _IPC_H
#define _IPC_H

#ifdef __NBSD_LIBC
#endif
#include <minix/ipcconst.h>
#include <minix/type.h>
#include <minix/const.h>

/*==========================================================================* 
 * Types relating to messages. 						    *
 *==========================================================================*/ 

#define M1                 1
#define M3                 3
#define M4                 4
#define M3_STRING         14	/* legacy m3_ca1 size (must not be changed) */
#define M3_LONG_STRING    16	/* current m3_ca1 size (may be increased) */

typedef struct {int m1i1, m1i2, m1i3; char *m1p1, *m1p2, *m1p3;} mess_1;
typedef struct {int m2i1, m2i2, m2i3; long m2l1, m2l2; char *m2p1; 
        short m2s1;} mess_2;
typedef struct {int m3i1, m3i2; char *m3p1; char m3ca1[M3_LONG_STRING];} mess_3;
typedef struct {long m4l1, m4l2, m4l3, m4l4, m4l5;} mess_4;
typedef struct {short m5s1, m5s2; int m5i1, m5i2; long m5l1, m5l2, m5l3;}mess_5;
typedef struct {long m6l1, m6l2, m6l3; short m6s1, m6s2, m6s3; char m6c1, m6c2;
        char *m6p1, *m6p2;} mess_6;
typedef struct {int m7i1, m7i2, m7i3, m7i4, m7i5; char *m7p1, *m7p2;} mess_7;
typedef struct {int m8i1, m8i2; char *m8p1, *m8p2, *m8p3, *m8p4;} mess_8;
typedef struct {long m9l1, m9l2, m9l3, m9l4, m9l5;
	short m9s1, m9s2, m9s3, m9s4; } mess_9;
typedef struct {int m10i1, m10i2, m10i3, m10i4;
	long m10l1, m10l2, m10l3; } mess_10;

typedef struct {
  endpoint_t m_source;		/* who sent the message */
  int m_type;			/* what kind of message is it */
  union {
	mess_1 m_m1;
	mess_2 m_m2;
	mess_3 m_m3;
	mess_4 m_m4;
	mess_5 m_m5;
	mess_7 m_m7;
	mess_8 m_m8;
	mess_6 m_m6;
	mess_9 m_m9;
	mess_10 m_m10;
  } m_u;
} message __aligned(16);

/* The following defines provide names for useful members. */
#define m1_i1  m_u.m_m1.m1i1
#define m1_i2  m_u.m_m1.m1i2
#define m1_i3  m_u.m_m1.m1i3
#define m1_p1  m_u.m_m1.m1p1
#define m1_p2  m_u.m_m1.m1p2
#define m1_p3  m_u.m_m1.m1p3

#define m2_i1  m_u.m_m2.m2i1
#define m2_i2  m_u.m_m2.m2i2
#define m2_i3  m_u.m_m2.m2i3
#define m2_l1  m_u.m_m2.m2l1
#define m2_l2  m_u.m_m2.m2l2
#define m2_p1  m_u.m_m2.m2p1

#define m2_s1  m_u.m_m2.m2s1

#define m3_i1  m_u.m_m3.m3i1
#define m3_i2  m_u.m_m3.m3i2
#define m3_p1  m_u.m_m3.m3p1
#define m3_ca1 m_u.m_m3.m3ca1

#define m4_l1  m_u.m_m4.m4l1
#define m4_l2  m_u.m_m4.m4l2
#define m4_l3  m_u.m_m4.m4l3
#define m4_l4  m_u.m_m4.m4l4
#define m4_l5  m_u.m_m4.m4l5

#define m5_s1  m_u.m_m5.m5s1
#define m5_s2  m_u.m_m5.m5s2
#define m5_i1  m_u.m_m5.m5i1
#define m5_i2  m_u.m_m5.m5i2
#define m5_l1  m_u.m_m5.m5l1
#define m5_l2  m_u.m_m5.m5l2
#define m5_l3  m_u.m_m5.m5l3

#define m6_l1  m_u.m_m6.m6l1
#define m6_l2  m_u.m_m6.m6l2
#define m6_l3  m_u.m_m6.m6l3
#define m6_s1  m_u.m_m6.m6s1
#define m6_s2  m_u.m_m6.m6s2
#define m6_s3  m_u.m_m6.m6s3
#define m6_c1  m_u.m_m6.m6c1
#define m6_c2  m_u.m_m6.m6c2
#define m6_p1  m_u.m_m6.m6p1
#define m6_p2  m_u.m_m6.m6p2

#define m7_i1  m_u.m_m7.m7i1
#define m7_i2  m_u.m_m7.m7i2
#define m7_i3  m_u.m_m7.m7i3
#define m7_i4  m_u.m_m7.m7i4
#define m7_i5  m_u.m_m7.m7i5
#define m7_p1  m_u.m_m7.m7p1
#define m7_p2  m_u.m_m7.m7p2

#define m8_i1  m_u.m_m8.m8i1
#define m8_i2  m_u.m_m8.m8i2
#define m8_p1  m_u.m_m8.m8p1
#define m8_p2  m_u.m_m8.m8p2
#define m8_p3  m_u.m_m8.m8p3
#define m8_p4  m_u.m_m8.m8p4

#define m9_l1  m_u.m_m9.m9l1
#define m9_l2  m_u.m_m9.m9l2
#define m9_l3  m_u.m_m9.m9l3
#define m9_l4  m_u.m_m9.m9l4
#define m9_l5  m_u.m_m9.m9l5
#define m9_s1  m_u.m_m9.m9s1
#define m9_s2  m_u.m_m9.m9s2
#define m9_s3  m_u.m_m9.m9s3
#define m9_s4  m_u.m_m9.m9s4

#define m10_i1 m_u.m_m10.m10i1
#define m10_i2 m_u.m_m10.m10i2
#define m10_i3 m_u.m_m10.m10i3
#define m10_i4 m_u.m_m10.m10i4
#define m10_l1 m_u.m_m10.m10l1
#define m10_l2 m_u.m_m10.m10l2
#define m10_l3 m_u.m_m10.m10l3

/*==========================================================================* 
 * Minix run-time system (IPC). 					    *
 *==========================================================================*/ 

/* Datastructure for asynchronous sends */
typedef struct asynmsg
{
	unsigned flags;
	endpoint_t dst;
	int result;
	message msg;
} asynmsg_t;

/* Defines for flags field */
#define AMF_EMPTY	000	/* slot is not inuse */
#define AMF_VALID	001	/* slot contains message */
#define AMF_DONE	002	/* Kernel has processed the message. The
				 * result is stored in 'result'
				 */
#define AMF_NOTIFY	004	/* Send a notification when AMF_DONE is set */
#define AMF_NOREPLY	010	/* Not a reply message for a SENDREC */
#define AMF_NOTIFY_ERR	020	/* Send a notification when AMF_DONE is set and
				 * delivery of the message failed */

int _send_orig(endpoint_t dest, message *m_ptr);
int _receive_orig(endpoint_t src, message *m_ptr, int *status_ptr);
int _sendrec_orig(endpoint_t src_dest, message *m_ptr);
int _sendnb_orig(endpoint_t dest, message *m_ptr);
int _notify_orig(endpoint_t dest);
int _senda_orig(asynmsg_t *table, size_t count);
int _do_kernel_call_orig(message *m_ptr);

int _minix_kernel_info_struct(struct minix_kerninfo **);

/* Hide names to avoid name space pollution. */
#define notify          _notify
#define sendrec         _sendrec
#define receive         _receive
#define receivenb       _receivenb
#define send            _send
#define sendnb          _sendnb
#define senda           _senda

struct minix_ipcvecs {
	int (*send)(endpoint_t dest, message *m_ptr);
	int (*receive)(endpoint_t src, message *m_ptr, int *st);
	int (*sendrec)(endpoint_t src_dest, message *m_ptr);
	int (*sendnb)(endpoint_t dest, message *m_ptr);
	int (*notify)(endpoint_t dest);
	int (*do_kernel_call)(message *m_ptr);
	int (*senda)(asynmsg_t *table, size_t count);
};

/* kernel-set IPC vectors retrieved by a constructor in libc/sys-minix/init.c */
extern struct minix_ipcvecs _minix_ipcvecs;

static inline int _send(endpoint_t dest, message *m_ptr)
{
	return _minix_ipcvecs.send(dest, m_ptr);
}

static inline int _receive(endpoint_t src, message *m_ptr, int *st)
{
	return _minix_ipcvecs.receive(src, m_ptr, st);
}

static inline int _sendrec(endpoint_t src_dest, message *m_ptr)
{
	return _minix_ipcvecs.sendrec(src_dest, m_ptr);
}

static inline int _sendnb(endpoint_t dest, message *m_ptr)
{
	return _minix_ipcvecs.send(dest, m_ptr);
}

static inline int _notify(endpoint_t dest)
{
	return _minix_ipcvecs.notify(dest);
}

static inline int do_kernel_call(message *m_ptr)
{
	return _minix_ipcvecs.do_kernel_call(m_ptr);
}

static inline int _senda(asynmsg_t *table, size_t count)
{
	return _minix_ipcvecs.senda(table, count);
}

#endif /* _IPC_H */
