/*
buf.h

Copyright 1995 Philip Homburg
*/

#ifndef BUF_H
#define BUF_H

/* Note: BUF_S should be defined in const.h */

#define MAX_BUFREQ_PRI	10

#define ETH_PRI_PORTBUFS	3
#define ETH_PRI_FDBUFS_EXTRA	5
#define ETH_PRI_FDBUFS		6

#define IP_PRI_PORTBUFS		3
#define IP_PRI_ASSBUFS		4
#define IP_PRI_FDBUFS_EXTRA	5
#define IP_PRI_FDBUFS		6

#define ICMP_PRI_QUEUE		1

#define TCP_PRI_FRAG2SEND	4
#define TCP_PRI_CONN_EXTRA	5
#define TCP_PRI_CONNwoUSER	7
#define TCP_PRI_CONN_INUSE	9

#define UDP_PRI_FDBUFS_EXTRA	5
#define UDP_PRI_FDBUFS		6

#define PSIP_PRI_EXP_PROMISC	2

struct acc;
typedef void (*buffree_t) ARGS(( struct acc *acc ));
typedef void (*bf_freereq_t) ARGS(( int priority ));

#ifdef BUF_CONSISTENCY_CHECK
typedef void (*bf_checkreq_t) ARGS(( void ));
#endif

typedef struct buf
{
	int buf_linkC;
	buffree_t buf_free;
	size_t buf_size;
	char *buf_data_p;

#ifdef BUF_TRACK_ALLOC_FREE
	char *buf_alloc_file;
	int buf_alloc_line;
	char *buf_free_file;
	int buf_free_line;
#endif
#ifdef BUF_CONSISTENCY_CHECK
	unsigned buf_generation;
	int buf_check_linkC;
#endif
} buf_t;

typedef struct acc
{
	int acc_linkC;
	int acc_offset, acc_length;
	buf_t *acc_buffer;
	struct acc *acc_next, *acc_ext_link;

#ifdef BUF_TRACK_ALLOC_FREE
	char *acc_alloc_file;
	int acc_alloc_line;
	char *acc_free_file;
	int acc_free_line;
#endif
#ifdef BUF_CONSISTENCY_CHECK
	unsigned acc_generation;
	int acc_check_linkC;
#endif
} acc_t;

extern acc_t *bf_temporary_acc;

/* For debugging... */

#ifdef BUF_TRACK_ALLOC_FREE

#ifndef BUF_IMPLEMENTATION

#define bf_memreq(a) _bf_memreq(this_file, __LINE__, a)
#define bf_cut(a,b,c) _bf_cut(this_file, __LINE__, a, b, c)
#define bf_delhead(a,b) _bf_delhead(this_file, __LINE__, a, b)
#define bf_packIffLess(a,b) _bf_packIffLess(this_file, __LINE__, \
									a, b)
#define bf_afree(a) _bf_afree(this_file, __LINE__, a)
#define bf_pack(a) _bf_pack(this_file, __LINE__, a)
#define bf_append(a,b) _bf_append(this_file, __LINE__, a, b)
#define bf_dupacc(a) _bf_dupacc(this_file, __LINE__, a)
#define bf_mark_acc(a) _bf_mark_acc(this_file, __LINE__, a)
#define bf_align(a,s,al) _bf_align(this_file, __LINE__, a, s, al)

#else /* BUF_IMPLEMENTATION */

#define bf_afree(a) _bf_afree(clnt_file, clnt_line, a)
#define bf_pack(a) _bf_pack(clnt_file, clnt_line, a)
#define bf_memreq(a) _bf_memreq(clnt_file, clnt_line, a)
#define bf_dupacc(a) _bf_dupacc(clnt_file, clnt_line, a)
#define bf_cut(a,b,c) _bf_cut(clnt_file, clnt_line, a, b, c)
#define bf_delhead(a,b) _bf_delhead(clnt_file, clnt_line, a, b)
#define bf_align(a,s,al) _bf_align(clnt_file, clnt_line, a, s, al)

#endif /* !BUF_IMPLEMENTATION */

#else

#define bf_mark_acc(acc)	((void)0)

#endif /* BUF_TRACK_ALLOC_FREE */

/* Prototypes */

void bf_init ARGS(( void ));
#ifndef BUF_CONSISTENCY_CHECK
void bf_logon ARGS(( bf_freereq_t func ));
#else
void bf_logon ARGS(( bf_freereq_t func, bf_checkreq_t checkfunc ));
#endif

#ifndef BUF_TRACK_ALLOC_FREE
acc_t *bf_memreq ARGS(( unsigned size));
#else
acc_t *_bf_memreq ARGS(( char *clnt_file, int clnt_line,
			unsigned size));
#endif
/* the result is an acc with linkC == 1 */

#ifndef BUF_TRACK_ALLOC_FREE
acc_t *bf_dupacc ARGS(( acc_t *acc ));
#else
acc_t *_bf_dupacc ARGS(( char *clnt_file, int clnt_line,
			acc_t *acc ));
#endif
/* the result is an acc with linkC == 1 identical to the given one */

#ifndef BUF_TRACK_ALLOC_FREE
void bf_afree ARGS(( acc_t *acc));
#else
void _bf_afree ARGS(( char *clnt_file, int clnt_line,
			acc_t *acc));
#endif
/* this reduces the linkC off the given acc with one */

#ifndef BUF_TRACK_ALLOC_FREE
acc_t *bf_pack ARGS(( acc_t *pack));
#else
acc_t *_bf_pack ARGS(( char *clnt_file, int clnt_line,
			acc_t *pack));
#endif
/* this gives a packed copy of the given acc, the linkC of the given acc is
   reduced by one, the linkC of the result == 1 */

#ifndef BUF_TRACK_ALLOC_FREE
acc_t *bf_packIffLess ARGS(( acc_t *pack, int min_len ));
#else
acc_t *_bf_packIffLess ARGS(( char *clnt_file, int clnt_line,
				acc_t *pack, int min_len ));
#endif
/* this performs a bf_pack iff pack->acc_length<min_len */

size_t bf_bufsize ARGS(( acc_t *pack));
/* this gives the length of the buffer specified by the given acc. The linkC
   of the given acc remains the same */

#ifndef BUF_TRACK_ALLOC_FREE
acc_t *bf_cut ARGS(( acc_t *data, unsigned offset, unsigned length ));
#else
acc_t *_bf_cut ARGS(( char *clnt_file, int clnt_line,
			acc_t *data, unsigned offset, unsigned length ));
#endif
/* the result is a cut of the buffer from offset with length length.
   The linkC of the result == 1, the linkC of the given acc remains the
   same. */

#ifndef BUF_TRACK_ALLOC_FREE
acc_t *bf_delhead ARGS(( acc_t *data, unsigned offset ));
#else
acc_t *_bf_delhead ARGS(( char *clnt_file, int clnt_line,
			acc_t *data, unsigned offset ));
#endif
/* the result is a cut of the buffer from offset until the end.
   The linkC of the result == 1, the linkC of the given acc is
   decremented. */

#ifndef BUF_TRACK_ALLOC_FREE
acc_t *bf_append ARGS(( acc_t *data_first, acc_t  *data_second ));
#else
acc_t *_bf_append ARGS(( char *clnt_file, int clnt_line,
			acc_t *data_first, acc_t  *data_second ));
#endif
/* data_second is appended after data_first, a link is returned to the
	result and the linkCs of data_first and data_second are reduced.
	further more, if the contents of the last part of data_first and
	the first part of data_second fit in a buffer, both parts are
	copied into a (possibly fresh) buffer
*/

#ifndef BUF_TRACK_ALLOC_FREE
acc_t *bf_align ARGS(( acc_t *acc, size_t size, size_t alignment ));
#else
acc_t *_bf_align ARGS(( char *clnt_file, int clnt_line,
			acc_t *acc, size_t size, size_t alignment ));
#endif
/* size bytes of acc (or all bytes of acc if the size buffer is smaller
	than size) are aligned on an address that is multiple of alignment.
	Size must be less than or equal to BUF_S.
*/

#define ptr2acc_data(/* acc_t * */ a) (bf_temporary_acc=(a), \
	(&bf_temporary_acc->acc_buffer->buf_data_p[bf_temporary_acc-> \
		acc_offset]))

#define bf_chkbuf(buf) ((buf)? (compare((buf)->acc_linkC,>,0), \
	compare((buf)->acc_buffer, !=, 0), \
	compare((buf)->acc_buffer->buf_linkC,>,0)) : 0)

#ifdef BUF_CONSISTENCY_CHECK
int bf_consistency_check ARGS(( void ));
void bf_check_acc ARGS(( acc_t *acc ));
void _bf_mark_acc ARGS(( char *clnt_file, int clnt_line, acc_t *acc ));
#endif

#endif /* BUF_H */

/*
 * $PchId: buf.h,v 1.8 1995/11/21 06:45:27 philip Exp $
 */
