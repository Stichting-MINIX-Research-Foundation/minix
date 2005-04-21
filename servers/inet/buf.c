/*
This file contains routines for buffer management.

Copyright 1995 Philip Homburg
*/

#define BUF_IMPLEMENTATION	1	/* Avoid some macros */

#include "inet.h"

#include <stdlib.h>
#include <string.h>

#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/type.h"

THIS_FILE

#ifndef BUF_USEMALLOC
#define BUF_USEMALLOC	0
#endif

#ifndef BUF512_NR
#if CRAMPED
#define BUF512_NR	32
#else
#define BUF512_NR	128
#endif
#endif
#ifndef BUF2K_NR
#define BUF2K_NR	0
#endif
#ifndef BUF32K_NR
#define BUF32K_NR	0
#endif

#define ACC_NR		((BUF512_NR+BUF2K_NR+BUF32K_NR)*3/2)
#define CLIENT_NR	6

#define DECLARE_TYPE(Tag, Type, Size)					\
	typedef struct Tag						\
	{								\
		buf_t buf_header;					\
		char buf_data[Size];					\
	} Type

#if BUF_USEMALLOC
#define DECLARE_STORAGE(Type, Ident, Nitems)				\
	PRIVATE Type *Ident

#define ALLOC_STORAGE(Ident, Nitems, Label)				\
	do								\
	{								\
		printf("buf.c: malloc %d %s\n", Nitems, Label);		\
		Ident= malloc(sizeof(*Ident) * Nitems);			\
		if (!Ident)						\
			ip_panic(( "unable to alloc %s", Label ));	\
	} while(0)
#else
#define DECLARE_STORAGE(Type, Ident, Nitems)				\
	PRIVATE Type Ident[Nitems]

#define ALLOC_STORAGE(Ident, Nitems, Label)				\
	(void)0
#endif

#if BUF512_NR
DECLARE_TYPE(buf512, buf512_t, 512);
PRIVATE acc_t *buf512_freelist;
DECLARE_STORAGE(buf512_t, buffers512, BUF512_NR);
FORWARD void bf_512free ARGS(( acc_t *acc ));
#endif
#if BUF2K_NR
DECLARE_TYPE(buf2K, buf2K_t, (2*1024));
PRIVATE acc_t *buf2K_freelist;
DECLARE_STORAGE(buf2K_t, buffers2K, BUF2K_NR);
FORWARD void bf_2Kfree ARGS(( acc_t *acc ));
#endif
#if BUF32K_NR
DECLARE_TYPE(buf32K, buf32K_t, (32*1024));
PRIVATE acc_t *buf32K_freelist;
DECLARE_STORAGE(buf32K_t, buffers32K, BUF32K_NR);
FORWARD void bf_32Kfree ARGS(( acc_t *acc ));
#endif

PRIVATE acc_t *acc_freelist;
DECLARE_STORAGE(acc_t, accessors, ACC_NR);

PRIVATE bf_freereq_t freereq[CLIENT_NR];
PRIVATE size_t bf_buf_gran;

PUBLIC size_t bf_free_bufsize;
PUBLIC acc_t *bf_temporary_acc;

#ifdef BUF_CONSISTENCY_CHECK
int inet_buf_debug;
unsigned buf_generation; 
PRIVATE bf_checkreq_t checkreq[CLIENT_NR];
#endif

#ifndef BUF_TRACK_ALLOC_FREE
FORWARD acc_t *bf_small_memreq ARGS(( size_t size ));
#else
FORWARD acc_t *_bf_small_memreq ARGS(( char *clnt_file, int clnt_line,
								size_t size ));
#define bf_small_memreq(a) _bf_small_memreq(clnt_file, clnt_line, a)
#endif
FORWARD void free_accs ARGS(( void ));
#ifdef BUF_CONSISTENCY_CHECK
FORWARD void count_free_bufs ARGS(( acc_t *list ));
FORWARD int report_buffer ARGS(( buf_t *buf, char *label, int i ));
#endif

PUBLIC void bf_init()
{
	int i;
	size_t size;
	size_t buf_s;
	acc_t *acc;

	bf_buf_gran= BUF_S;
	buf_s= 0;

	for (i=0;i<CLIENT_NR;i++)
		freereq[i]=0;
#ifdef BUF_CONSISTENCY_CHECK
	for (i=0;i<CLIENT_NR;i++)
		checkreq[i]=0;
#endif

#if BUF512_NR
	ALLOC_STORAGE(buffers512, BUF512_NR, "512B-buffers");
#endif
#if BUF2K_NR
	ALLOC_STORAGE(buffers2K, BUF2K_NR, "2K-buffers");
#endif
#if BUF32K_NR
	ALLOC_STORAGE(buffers32K, BUF32K_NR, "32K-buffers");
#endif
	ALLOC_STORAGE(accessors, ACC_NR, "accs");

	acc_freelist= NULL;
	for (i=0;i<ACC_NR;i++)
	{
		memset(&accessors[i], '\0', sizeof(accessors[i]));

		accessors[i].acc_linkC= 0;
		accessors[i].acc_next= acc_freelist;
		acc_freelist= &accessors[i];
	}

#define INIT_BUFFERS(Ident, Nitems, Freelist, Freefunc)			\
	do								\
	{								\
		Freelist= NULL;						\
		for (i=0;i<Nitems;i++)					\
		{							\
			acc= acc_freelist;				\
			if (!acc)					\
				ip_panic(( "fewer accessors than buffers")); \
			acc_freelist= acc->acc_next;			\
			acc->acc_linkC= 0;				\
									\
			memset(&Ident[i], '\0', sizeof(Ident[i]));	\
			Ident[i].buf_header.buf_linkC= 0;		\
			Ident[i].buf_header.buf_free= Freefunc;		\
			Ident[i].buf_header.buf_size=			\
				sizeof(Ident[i].buf_data);		\
			Ident[i].buf_header.buf_data_p=			\
				Ident[i].buf_data;			\
									\
			acc->acc_buffer= &Ident[i].buf_header;		\
			acc->acc_next= Freelist;			\
			Freelist= acc;					\
		}							\
		if (sizeof(Ident[0].buf_data) < bf_buf_gran)		\
			bf_buf_gran= sizeof(Ident[0].buf_data);		\
		if (sizeof(Ident[0].buf_data) > buf_s)			\
			buf_s= sizeof(Ident[0].buf_data);		\
	} while(0)

#if BUF512_NR
	INIT_BUFFERS(buffers512, BUF512_NR, buf512_freelist, bf_512free);
#endif
#if BUF2K_NR
	INIT_BUFFERS(buffers2K, BUF2K_NR, buf2K_freelist, bf_2Kfree);
#endif
#if BUF32K_NR
	INIT_BUFFERS(buffers32K, BUF32K_NR, buf32K_freelist, bf_32Kfree);
#endif

#undef INIT_BUFFERS

	assert (buf_s == BUF_S);
}

#ifndef BUF_CONSISTENCY_CHECK
PUBLIC void bf_logon(func)
bf_freereq_t func;
#else
PUBLIC void bf_logon(func, checkfunc)
bf_freereq_t func;
bf_checkreq_t checkfunc;
#endif
{
	int i;

	for (i=0;i<CLIENT_NR;i++)
		if (!freereq[i])
		{
			freereq[i]=func;
#ifdef BUF_CONSISTENCY_CHECK
			checkreq[i]= checkfunc;
#endif
			return;
		}

	ip_panic(( "buf.c: to many clients" ));
}

/*
bf_memreq
*/

#ifndef BUF_TRACK_ALLOC_FREE
PUBLIC acc_t *bf_memreq(size)
#else
PUBLIC acc_t *_bf_memreq(clnt_file, clnt_line, size)
char *clnt_file;
int clnt_line;
#endif
size_t size;
{
	acc_t *head, *tail, *new_acc;
	buf_t *buf;
	int i,j;
	size_t count;

	assert (size>0);

	head= NULL;
	while (size)
	{
		new_acc= NULL;

		/* Note the tricky dangling else... */
#define ALLOC_BUF(Freelist, Bufsize)					\
	if (Freelist && (Bufsize == BUF_S || size <= Bufsize))		\
	{								\
		new_acc= Freelist;					\
		Freelist= new_acc->acc_next;				\
									\
		assert(new_acc->acc_linkC == 0);			\
		new_acc->acc_linkC= 1;					\
		buf= new_acc->acc_buffer;				\
		assert(buf->buf_linkC == 0);				\
		buf->buf_linkC= 1;					\
	}								\
	else

		/* Sort attempts by buffer size */
#if BUF512_NR
		ALLOC_BUF(buf512_freelist, 512)
#endif
#if BUF2K_NR
		ALLOC_BUF(buf2K_freelist, 2*1024)
#endif
#if BUF32K_NR
		ALLOC_BUF(buf32K_freelist, 32*1024)
#endif
#undef ALLOC_BUF
		{
			DBLOCK(1, printf("freeing buffers\n"));

			bf_free_bufsize= 0;
			for (i=0; bf_free_bufsize<size && i<MAX_BUFREQ_PRI;
				i++)
			{
				for (j=0; j<CLIENT_NR; j++)
				{
					if (freereq[j])
						(*freereq[j])(i);
				}
#if DEBUG
 { acc_t *acc;
   j= 0; for(acc= buf512_freelist; acc; acc= acc->acc_next) j++;
   printf("# of free 512-bytes buffer is now %d\n", j); }
#endif
			}
#if DEBUG
 { printf("last level was level %d\n", i-1); }
#endif
			if (bf_free_bufsize<size)
				ip_panic(( "not enough buffers freed" ));

			continue;
		}

#ifdef BUF_TRACK_ALLOC_FREE
		new_acc->acc_alloc_file= clnt_file;
		new_acc->acc_alloc_line= clnt_line;
		buf->buf_alloc_file= clnt_file;
		buf->buf_alloc_line= clnt_line;
#endif

		if (!head)
			head= new_acc;
		else
			tail->acc_next= new_acc;
		tail= new_acc;

		count= tail->acc_buffer->buf_size;
		if (count > size)
			count= size;

		tail->acc_offset= 0;
		tail->acc_length=  count;
		size -= count;
	}
	tail->acc_next= 0;

#if DEBUG
	bf_chkbuf(head);
#endif

	return head;
}

/*
bf_small_memreq
*/

#ifndef BUF_TRACK_ALLOC_FREE
PRIVATE acc_t *bf_small_memreq(size)
#else
PRIVATE acc_t *_bf_small_memreq(clnt_file, clnt_line, size)
char *clnt_file;
int clnt_line;
#endif
size_t size;
{
	return bf_memreq(size);
}

#ifndef BUF_TRACK_ALLOC_FREE
PUBLIC void bf_afree(acc)
#else
PUBLIC void _bf_afree(clnt_file, clnt_line, acc)
char *clnt_file;
int clnt_line;
#endif
acc_t *acc;
{
	acc_t *next_acc;
	buf_t *buf;

	while (acc)
	{
#if defined(bf_afree)
		DIFBLOCK(1, (acc->acc_linkC <= 0),
			printf("clnt_file= %s, clnt_line= %d\n", 
			clnt_file, clnt_line));
#endif
		assert (acc->acc_linkC>0);
		if (--acc->acc_linkC > 0)
			break;

#ifdef BUF_TRACK_ALLOC_FREE
		acc->acc_free_file= clnt_file;
		acc->acc_free_line= clnt_line;
#endif
		buf= acc->acc_buffer;
		assert (buf);

#if defined(bf_afree)
		DIFBLOCK(1, (buf->buf_linkC == 0),
			printf("clnt_file= %s, clnt_line= %d\n", 
			clnt_file, clnt_line));
#endif
		assert (buf->buf_linkC>0);
		if (--buf->buf_linkC > 0)
		{
			acc->acc_buffer= NULL;
			next_acc= acc->acc_next;
			acc->acc_next= acc_freelist;
			acc_freelist= acc;
#ifdef BUF_CONSISTENCY_CHECK
			if (inet_buf_debug)
			{
				acc->acc_offset= 0xdeadbeaf;
				acc->acc_length= 0xdeadbeaf;
				acc->acc_buffer= (buf_t *)0xdeadbeaf;
				acc->acc_ext_link= (acc_t *)0xdeadbeaf;
			}
#endif
			acc= next_acc;
			continue;
		}

		bf_free_bufsize += buf->buf_size;
#ifdef BUF_TRACK_ALLOC_FREE
		buf->buf_free_file= clnt_file;
		buf->buf_free_line= clnt_line;
#endif
		next_acc= acc->acc_next;
		buf->buf_free(acc);
		acc= next_acc;
		continue;
	}
}

#ifndef BUF_TRACK_ALLOC_FREE
PUBLIC acc_t *bf_dupacc(acc_ptr)
#else
PUBLIC acc_t *_bf_dupacc(clnt_file, clnt_line, acc_ptr)
char *clnt_file;
int clnt_line;
#endif
register acc_t *acc_ptr;
{
	register acc_t *new_acc;
	int i, j;

	if (!acc_freelist)
	{
		free_accs();
		if (!acc_freelist)
			ip_panic(( "buf.c: out of accessors" ));
	}
	new_acc= acc_freelist;
	acc_freelist= new_acc->acc_next;

	*new_acc= *acc_ptr;
	if (acc_ptr->acc_next)
		acc_ptr->acc_next->acc_linkC++;
	if (acc_ptr->acc_buffer)
		acc_ptr->acc_buffer->buf_linkC++;
	new_acc->acc_linkC= 1;
#ifdef BUF_TRACK_ALLOC_FREE
	new_acc->acc_alloc_file= clnt_file;
	new_acc->acc_alloc_line= clnt_line;
#endif
	return new_acc;
}

PUBLIC size_t bf_bufsize(acc_ptr)
register acc_t *acc_ptr;
{
	register size_t size;

assert(acc_ptr);

	size=0;

	while (acc_ptr)
	{
assert(acc_ptr >= accessors && acc_ptr <= &accessors[ACC_NR-1]);
		size += acc_ptr->acc_length;
		acc_ptr= acc_ptr->acc_next;
	}
	return size;
}

#ifndef BUF_TRACK_ALLOC_FREE
PUBLIC acc_t *bf_packIffLess(pack, min_len)
#else
PUBLIC acc_t *_bf_packIffLess(clnt_file, clnt_line, pack, min_len)
char *clnt_file;
int clnt_line;
#endif
acc_t *pack;
int min_len;
{
	if (!pack || pack->acc_length >= min_len)
		return pack;

#if DEBUG
#ifdef bf_packIffLess
 { where(); printf("calling bf_pack because of %s %d: %d\n", bf_pack_file,
	bf_pack_line, min_len); }
#endif
#endif
	return bf_pack(pack);
}

#ifndef BUF_TRACK_ALLOC_FREE
PUBLIC acc_t *bf_pack(old_acc)
#else
PUBLIC acc_t *_bf_pack(clnt_file, clnt_line, old_acc)
char *clnt_file;
int clnt_line;
#endif
acc_t *old_acc;
{
	acc_t *new_acc, *acc_ptr_old, *acc_ptr_new;
	size_t size, offset_old, offset_new, block_size, block_size_old;

	/* Check if old acc is good enough. */
	if (!old_acc || !old_acc->acc_next && old_acc->acc_linkC == 1 && 
		old_acc->acc_buffer->buf_linkC == 1)
	{
		return old_acc;
	}

	size= bf_bufsize(old_acc);
	assert(size > 0);
	new_acc= bf_memreq(size);
	acc_ptr_old= old_acc;
	acc_ptr_new= new_acc;
	offset_old= 0;
	offset_new= 0;
	while (size)
	{
		assert (acc_ptr_old);
		if (offset_old == acc_ptr_old->acc_length)
		{
			offset_old= 0;
			acc_ptr_old= acc_ptr_old->acc_next;
			continue;
		}
		assert (offset_old < acc_ptr_old->acc_length);
		block_size_old= acc_ptr_old->acc_length - offset_old;
		assert (acc_ptr_new);
		if (offset_new == acc_ptr_new->acc_length)
		{
			offset_new= 0;
			acc_ptr_new= acc_ptr_new->acc_next;
			continue;
		}
		assert (offset_new < acc_ptr_new->acc_length);
		block_size= acc_ptr_new->acc_length - offset_new;
		if (block_size > block_size_old)
			block_size= block_size_old;
		memcpy(ptr2acc_data(acc_ptr_new)+offset_new,
			ptr2acc_data(acc_ptr_old)+offset_old, block_size);
		offset_new += block_size;
		offset_old += block_size;
		size -= block_size;
	}
	bf_afree(old_acc);
	return new_acc;
}

#ifndef BUF_TRACK_ALLOC_FREE
PUBLIC acc_t *bf_cut (data, offset, length)
#else
PUBLIC acc_t *_bf_cut (clnt_file, clnt_line, data, offset, length)
char *clnt_file;
int clnt_line;
#endif
register acc_t *data;
register unsigned offset;
register unsigned length;
{
	register acc_t *head, *tail;

	if (!data && !offset && !length)
		return 0;
#ifdef BUF_TRACK_ALLOC_FREE
	assert(data ||
		(printf("from %s, %d: %u, %u\n",
		clnt_file, clnt_line, offset, length), 0));
#else
	assert(data);
#endif

	assert(data);
#if DEBUG
	bf_chkbuf(data);
#endif

	if (!length)
	{
		head= bf_dupacc(data);
		bf_afree(head->acc_next);
		head->acc_next= 0;
		head->acc_length= 0;
#if DEBUG
		bf_chkbuf(data);
#endif
		return head;
	}
	while (data && offset>=data->acc_length)
	{
		offset -= data->acc_length;
		data= data->acc_next;
	}

	assert (data);

	head= bf_dupacc(data);
	bf_afree(head->acc_next);
	head->acc_next= 0;
	head->acc_offset += offset;
	head->acc_length -= offset;
	if (length >= head->acc_length)
		length -= head->acc_length;
	else
	{
		head->acc_length= length;
		length= 0;
	}
	tail= head;
	data= data->acc_next;
	while (data && length && length>=data->acc_length)
	{
		tail->acc_next= bf_dupacc(data);
		tail= tail->acc_next;
		bf_afree(tail->acc_next);
		tail->acc_next= 0;
		data= data->acc_next;
		length -= tail->acc_length;
	}
	if (length)
	{
#ifdef bf_cut
		assert (data ||
			(printf("bf_cut called from %s:%d\n",
			clnt_file, clnt_line), 0));
#else
		assert (data);
#endif
		tail->acc_next= bf_dupacc(data);
		tail= tail->acc_next;
		bf_afree(tail->acc_next);
		tail->acc_next= 0;
		tail->acc_length= length;
	}
#if DEBUG
	bf_chkbuf(data);
#endif
	return head;
}

#ifndef BUF_TRACK_ALLOC_FREE
PUBLIC acc_t *bf_delhead (data, offset)
#else
PUBLIC acc_t *_bf_delhead (clnt_file, clnt_line, data, offset)
char *clnt_file;
int clnt_line;
#endif
register acc_t *data;
register unsigned offset;
{
	acc_t *new_acc;

	assert(data);

	/* Find the acc we need to modify. */
	new_acc= data;
	while(offset >= new_acc->acc_length)
	{
		offset -= new_acc->acc_length;
		new_acc= new_acc->acc_next;
#ifdef BUF_TRACK_ALLOC_FREE
		assert(new_acc || (printf("called from %s, %d\n",
			clnt_file, clnt_line),0));
#else
		assert(new_acc);
#endif
	}

	/* Discard the old acc(s) */
	if (new_acc != data)
	{
		new_acc->acc_linkC++;
		bf_afree(data);
		data= new_acc;
	}

	/* Make sure that acc_linkC == 1 */
	if (data->acc_linkC != 1)
	{
		new_acc= bf_dupacc(data);
		bf_afree(data);
		data= new_acc;
	}

	/* Delete the last bit by modifying acc_offset and acc_length */
	data->acc_offset += offset;
	data->acc_length -= offset;
	return data;
}

/*
bf_append
*/

#ifndef BUF_TRACK_ALLOC_FREE
PUBLIC acc_t *bf_append(data_first, data_second)
#else
PUBLIC acc_t *_bf_append(clnt_file, clnt_line, data_first, data_second)
char *clnt_file;
int clnt_line;
#endif
acc_t *data_first;
acc_t  *data_second;
{
	acc_t *head, *tail, *new_acc, *acc_ptr_new, tmp_acc, *curr;
	char *src_ptr, *dst_ptr;
	size_t size, offset_old, offset_new, block_size_old, block_size;

	if (!data_first)
		return data_second;
	if (!data_second)
		return data_first;

	head= 0;
	while (data_first)
	{
		if (data_first->acc_linkC == 1)
			curr= data_first;
		else
		{
			curr= bf_dupacc(data_first);
			assert (curr->acc_linkC == 1);
			bf_afree(data_first);
		}
		data_first= curr->acc_next;
		if (!curr->acc_length)
		{
			curr->acc_next= 0;
			bf_afree(curr);
			continue;
		}
		if (!head)
			head= curr;
		else
			tail->acc_next= curr;
		tail= curr;
	}
	if (!head)
		return data_second;
	tail->acc_next= 0;

	while (data_second && !data_second->acc_length)
	{
		curr= data_second;
		data_second= data_second->acc_next;
		if (data_second)
			data_second->acc_linkC++;
		bf_afree(curr);
	}
	if (!data_second)
		return head;

	if (tail->acc_length + data_second->acc_length >
		tail->acc_buffer->buf_size)
	{
		tail->acc_next= data_second;
		return head;
	}

	if (tail->acc_buffer->buf_size == bf_buf_gran && 
		tail->acc_buffer->buf_linkC == 1)
	{
		if (tail->acc_offset)
		{
			memmove(tail->acc_buffer->buf_data_p,
				ptr2acc_data(tail), tail->acc_length);
			tail->acc_offset= 0;
		}
		dst_ptr= ptr2acc_data(tail) + tail->acc_length;
		src_ptr= ptr2acc_data(data_second);
		memcpy(dst_ptr, src_ptr, data_second->acc_length);
		tail->acc_length += data_second->acc_length;
		tail->acc_next= data_second->acc_next;
		if (data_second->acc_next)
			data_second->acc_next->acc_linkC++;
		bf_afree(data_second);
		return head;
	}

	new_acc= bf_small_memreq(tail->acc_length+data_second->acc_length);
	acc_ptr_new= new_acc;
	offset_old= 0;
	offset_new= 0;
	size= tail->acc_length;
	while (size)
	{
assert (acc_ptr_new);
		if (offset_new == acc_ptr_new->acc_length)
		{
			offset_new= 0;
			acc_ptr_new= acc_ptr_new->acc_next;
			continue;
		}
assert (offset_new < acc_ptr_new->acc_length);
assert (offset_old < tail->acc_length);
		block_size_old= tail->acc_length - offset_old;
		block_size= acc_ptr_new->acc_length - offset_new;
		if (block_size > block_size_old)
			block_size= block_size_old;
		memcpy(ptr2acc_data(acc_ptr_new)+offset_new,
			ptr2acc_data(tail)+offset_old, block_size);
		offset_new += block_size;
		offset_old += block_size;
		size -= block_size;
	}
	offset_old= 0;
	size= data_second->acc_length;
	while (size)
	{
assert (acc_ptr_new);
		if (offset_new == acc_ptr_new->acc_length)
		{
			offset_new= 0;
			acc_ptr_new= acc_ptr_new->acc_next;
			continue;
		}
assert (offset_new < acc_ptr_new->acc_length);
assert (offset_old < data_second->acc_length);
		block_size_old= data_second->acc_length - offset_old;
		block_size= acc_ptr_new->acc_length - offset_new;
		if (block_size > block_size_old)
			block_size= block_size_old;
		memcpy(ptr2acc_data(acc_ptr_new)+offset_new,
			ptr2acc_data(data_second)+offset_old, block_size);
		offset_new += block_size;
		offset_old += block_size;
		size -= block_size;
	}
	tmp_acc= *tail;
	*tail= *new_acc;
	*new_acc= tmp_acc;

	bf_afree(new_acc);
	while (tail->acc_next)
		tail= tail->acc_next;

	tail->acc_next= data_second->acc_next;
	if (data_second->acc_next)
		data_second->acc_next->acc_linkC++;
	bf_afree(data_second);
	return head;
}

#if BUF512_NR
PRIVATE void bf_512free(acc)
acc_t *acc;
{
#ifdef BUF_CONSISTENCY_CHECK 
	if (inet_buf_debug)
		memset(acc->acc_buffer->buf_data_p, 0xa5, 512);
#endif
	acc->acc_next= buf512_freelist;
	buf512_freelist= acc;
}
#endif
#if BUF2K_NR
PRIVATE void bf_2Kfree(acc)
acc_t *acc;
{
#ifdef BUF_CONSISTENCY_CHECK 
	if (inet_buf_debug)
		memset(acc->acc_buffer->buf_data_p, 0xa5, 2*1024);
#endif
	acc->acc_next= buf2K_freelist;
	buf2K_freelist= acc;
}
#endif
#if BUF32K_NR
PRIVATE void bf_32Kfree(acc)
acc_t *acc;
{
#ifdef BUF_CONSISTENCY_CHECK 
	if (inet_buf_debug)
		memset(acc->acc_buffer->buf_data_p, 0xa5, 32*1024);
#endif
	acc->acc_next= buf32K_freelist;
	buf32K_freelist= acc;
}
#endif

#ifdef BUF_CONSISTENCY_CHECK
PUBLIC int bf_consistency_check()
{
	acc_t *acc;
	buf_t *buf;
	int silent;
	int error;
	int i;

	buf_generation++;

	for (i=0; i<CLIENT_NR; i++)
	{
		if (checkreq[i])
			(*checkreq[i])();
	}

	/* Add information about free accessors */
	for(acc= acc_freelist; acc; acc= acc->acc_next)
	{
		if (acc->acc_generation == buf_generation-1)
		{
			acc->acc_generation= buf_generation;
			acc->acc_check_linkC= 0;
		}
		else
		{
			assert(acc->acc_generation == buf_generation &&
				acc->acc_check_linkC > 0);
			acc->acc_check_linkC= -acc->acc_check_linkC;
		}
	}

#if BUF512_NR
	count_free_bufs(buf512_freelist);
#endif
#if BUF2K_NR
	count_free_bufs(buf2K_freelist);
#endif
#if BUF32K_NR
	count_free_bufs(buf32K_freelist);
#endif

	error= 0;

	/* Report about accessors */
	silent= 0;
	for (i=0, acc= accessors; i<ACC_NR; i++, acc++)
	{
		if (acc->acc_generation != buf_generation)
		{
			error++;
			assert(acc->acc_generation == buf_generation-1);
			acc->acc_generation= buf_generation;
			if (!silent)
			{
				printf(
"acc[%d] (0x%x) has been lost with count %d, last allocated at %s, %d\n",
	i, acc, acc->acc_linkC, acc->acc_alloc_file, acc->acc_alloc_line);
#if 0
				silent= 1;
#endif
			}
			continue;
		}
		if (acc->acc_check_linkC == acc->acc_linkC)
			continue;
		error++;
		if (acc->acc_check_linkC < 0)
		{
			if (!silent)
			{
				printf(
"acc[%d] is freed but still in use, allocated at %s, %d, freed at %s, %d\n",
				i, acc->acc_alloc_file, acc->acc_alloc_line, 
				acc->acc_free_file, acc->acc_free_line);
			}
			acc->acc_check_linkC= -acc->acc_check_linkC;
			if (acc->acc_check_linkC == acc->acc_linkC)
			{
				silent= 1;
				continue;
			}
		}
		if (!silent)
		{
			printf(
"# of tracked links (%d) for acc[%d] don't match with stored link count %d\n",
				acc->acc_check_linkC, i, acc->acc_linkC);
			printf("acc[%d] was allocated at %s, %d\n",
				i, acc->acc_alloc_file, acc->acc_alloc_line);
			silent=1;
		}
	}

	/* Report about buffers */
#if BUF512_NR
	{
		for (i= 0; i<BUF512_NR; i++)
		{
			error |= report_buffer(&buffers512[i].buf_header,
				"512-buffer", i);
		}
	}
#endif
#if BUF2K_NR
	{
		for (i= 0; i<BUF2K_NR; i++)
		{
			error |= report_buffer(&buffers2K[i].buf_header,
				"2K-buffer", i);
		}
	}
#endif
#if BUF32K_NR
	{
		for (i= 0; i<BUF32K_NR; i++)
		{
			error |= report_buffer(&buffers32K[i].buf_header,
				"32K-buffer", i);
		}
	}
#endif

	return !error;
}

PRIVATE void count_free_bufs(list)
acc_t *list;
{
	acc_t *acc;
	buf_t *buf;

	for(acc= list; acc; acc= acc->acc_next)
	{
		if (acc->acc_generation != buf_generation-1)
		{
			assert(acc->acc_generation == buf_generation &&
				acc->acc_check_linkC > 0);
			acc->acc_check_linkC= -acc->acc_check_linkC;
			continue;
		}
		acc->acc_generation= buf_generation;
		acc->acc_check_linkC= 0;

		buf= acc->acc_buffer;
		if (buf->buf_generation == buf_generation-1)
		{
			buf->buf_generation= buf_generation;
			buf->buf_check_linkC= 0;
			continue;
		}
		assert(buf->buf_generation == buf_generation &&
			buf->buf_check_linkC > 0);
		buf->buf_check_linkC= -buf->buf_check_linkC;
	}
}

PRIVATE int report_buffer(buf, label, i)
buf_t *buf;
char *label;
int i;
{
	if (buf->buf_generation != buf_generation)
	{
		assert(buf->buf_generation == buf_generation-1);
		buf->buf_generation= buf_generation;
		printf(
"%s[%d] (0x%x) has been lost with count %d, last allocated at %s, %d\n",
			label, i, buf,
			buf->buf_linkC, buf->buf_alloc_file,
			buf->buf_alloc_line);
		return 1;
	}
	if (buf->buf_check_linkC == buf->buf_linkC)
		return 0;
	if (buf->buf_check_linkC < 0)
	{
		printf(
"%s[%d] is freed but still in use, allocated at %s, %d, freed at %s, %d\n",
			label, i, buf->buf_alloc_file, buf->buf_alloc_line, 
			buf->buf_free_file, buf->buf_free_line);
		buf->buf_check_linkC= -buf->buf_check_linkC;
		if (buf->buf_check_linkC == buf->buf_linkC)
			return 1;
	}
	printf(
"# of tracked links (%d) for %s[%d] don't match with stored link count %d\n",
			buf->buf_check_linkC, label, i, buf->buf_linkC);
	printf("%s[%d] was allocated at %s, %d\n",
		label, i, buf->buf_alloc_file, buf->buf_alloc_line);
	return 1;
}

PUBLIC void bf_check_acc(acc)
acc_t *acc;
{
	buf_t *buf;

	while(acc != NULL)
	{
		if (acc->acc_generation == buf_generation)
		{
			assert(acc->acc_check_linkC > 0);
			acc->acc_check_linkC++;
			return;
		}
		assert(acc->acc_generation == buf_generation-1);
		acc->acc_generation= buf_generation;
		acc->acc_check_linkC= 1;

		buf= acc->acc_buffer;
		if (buf->buf_generation == buf_generation)
		{
			assert(buf->buf_check_linkC > 0);
			buf->buf_check_linkC++;
		}
		else
		{
			assert(buf->buf_generation == buf_generation-1);
			buf->buf_generation= buf_generation;
			buf->buf_check_linkC= 1;
		}

		acc= acc->acc_next;
	}
}

PUBLIC void _bf_mark_acc(clnt_file, clnt_line, acc)
char *clnt_file;
int clnt_line;
acc_t *acc;
{
	buf_t *buf;

	for (; acc; acc= acc->acc_next)
	{
		acc->acc_alloc_file= clnt_file;
		acc->acc_alloc_line= clnt_line;
		buf= acc->acc_buffer;
		buf->buf_alloc_file= clnt_file;
		buf->buf_alloc_line= clnt_line;
	}
}
#endif

PRIVATE void free_accs()
{
	int i, j;

	DBLOCK(1, printf("free_accs\n"));

	for (i=0; !acc_freelist && i<MAX_BUFREQ_PRI; i++)
	{
		for (j=0; j<CLIENT_NR; j++)
		{
			bf_free_bufsize= 0;
			if (freereq[j])
			{
				(*freereq[j])(i);
			}
		}
	}
#if DEBUG
	printf("last level was level %d\n", i-1);
#endif
}

#ifndef BUF_TRACK_ALLOC_FREE
PUBLIC acc_t *bf_align(acc, size, alignment)
#else
PUBLIC acc_t *_bf_align(clnt_file, clnt_line, acc, size, alignment)
char *clnt_file;
int clnt_line;
#endif
acc_t *acc;
size_t size;
size_t alignment;
{
	char *ptr;
	size_t buf_size;
	acc_t *head, *tail;

	/* Fast check if the buffer is aligned already. */
	if (acc->acc_length >= size)
	{
		ptr= ptr2acc_data(acc);
		if (((unsigned)ptr & (alignment-1)) == 0)
			return acc;
	}
	buf_size= bf_bufsize(acc);
#ifdef bf_align
	assert(size != 0 && buf_size != 0 ||
		(printf("bf_align(..., %d, %d) from %s, %d\n",
			size, alignment, clnt_file, clnt_line),0));
#else
	assert(size != 0 && buf_size != 0);
#endif
	if (buf_size <= size)
	{
		acc= bf_pack(acc);
		return acc;
	}
	head= bf_cut(acc, 0, size);
	tail= bf_cut(acc, size, buf_size-size);
	bf_afree(acc);
	head= bf_pack(head);
	assert(head->acc_next == NULL);
	head->acc_next= tail;
	return head;
}

#if 0
int chk_acc(acc)
acc_t *acc;
{
	int acc_nr;

	if (!acc)
		return 1;
	if (acc < accessors || acc >= &accessors[ACC_NR])
		return 0;
	acc_nr= acc-accessors;
	return acc == &accessors[acc_nr];
}
#endif

/*
 * $PchId: buf.c,v 1.10 1995/11/23 11:25:25 philip Exp $
 */
