/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cxgb_l2t.c,v 1.3 2014/03/25 16:19:14 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_inarp.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>

#ifdef CONFIG_DEFINED
#include <cxgb_include.h>
#else
#include "cxgb_include.h"
#endif

#define VLAN_NONE 0xfff
#define SDL(s) ((struct sockaddr_dl *)s)
#define RT_ENADDR(rt)  ((u_char *)LLADDR(SDL((rt))))
#define rt_expire rt_rmx.rmx_expire

/*
 * Module locking notes:  There is a RW lock protecting the L2 table as a
 * whole plus a spinlock per L2T entry.  Entry lookups and allocations happen
 * under the protection of the table lock, individual entry changes happen
 * while holding that entry's spinlock.  The table lock nests outside the
 * entry locks.  Allocations of new entries take the table lock as writers so
 * no other lookups can happen while allocating new entries.  Entry updates
 * take the table lock as readers so multiple entries can be updated in
 * parallel.  An L2T entry can be dropped by decrementing its reference count
 * and therefore can happen in parallel with entry allocation but no entry
 * can change state or increment its ref count during allocation as both of
 * these perform lookups.
 */

static inline unsigned int
vlan_prio(const struct l2t_entry *e)
{
    return e->vlan >> 13;
}

static inline unsigned int
arp_hash(u32 key, int ifindex, const struct l2t_data *d)
{
    return jhash_2words(key, ifindex, 0) & (d->nentries - 1);
}

static inline void
neigh_replace(struct l2t_entry *e, struct rtentry *rt)
{
    RT_LOCK(rt);
    RT_ADDREF(rt);
    RT_UNLOCK(rt);

    if (e->neigh) {
        RT_LOCK(e->neigh);
        RT_REMREF(e->neigh);
        RT_UNLOCK(e->neigh);
    }
    e->neigh = rt;
}

/*
 * Set up an L2T entry and send any packets waiting in the arp queue.  The
 * supplied mbuf is used for the CPL_L2T_WRITE_REQ.  Must be called with the
 * entry locked.
 */
static int
setup_l2e_send_pending(struct toedev *dev, struct mbuf *m,
            struct l2t_entry *e)
{
    struct cpl_l2t_write_req *req;

    if (!m) {
        if ((m = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
            return (ENOMEM);
    }
    /*
     * XXX MH_ALIGN
     */
    req = mtod(m, struct cpl_l2t_write_req *);
    req->wr.wr_hi = htonl(V_WR_OP(FW_WROPCODE_FORWARD));
    OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_L2T_WRITE_REQ, e->idx));
    req->params = htonl(V_L2T_W_IDX(e->idx) | V_L2T_W_IFF(e->smt_idx) |
                V_L2T_W_VLAN(e->vlan & EVL_VLID_MASK) |
                V_L2T_W_PRIO(vlan_prio(e)));

    memcpy(e->dmac, RT_ENADDR(e->neigh), sizeof(e->dmac));
    memcpy(req->dst_mac, e->dmac, sizeof(req->dst_mac));
    m_set_priority(m, CPL_PRIORITY_CONTROL);
    while (e->arpq_head) {
        m = e->arpq_head;
        e->arpq_head = m->m_next;
        m->m_next = NULL;
    }
    e->arpq_tail = NULL;
    e->state = L2T_STATE_VALID;

    return 0;
}

/*
 * Add a packet to the an L2T entry's queue of packets awaiting resolution.
 * Must be called with the entry's lock held.
 */
static inline void
arpq_enqueue(struct l2t_entry *e, struct mbuf *m)
{
    m->m_next = NULL;
    if (e->arpq_head)
        e->arpq_tail->m_next = m;
    else
        e->arpq_head = m;
    e->arpq_tail = m;
}

int
t3_l2t_send_slow(struct toedev *dev, struct mbuf *m,
             struct l2t_entry *e)
{
    struct rtentry *rt;
    struct mbuf *m0;

    if ((m0 = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
        return (ENOMEM);

    rt = e->neigh;

again:
    switch (e->state) {
    case L2T_STATE_STALE:     /* entry is stale, kick off revalidation */
        arpresolve(rt->rt_ifp, rt, m0, rt->rt_gateway, RT_ENADDR(rt));
        mtx_lock(&e->lock);
        if (e->state == L2T_STATE_STALE)
            e->state = L2T_STATE_VALID;
        mtx_unlock(&e->lock);
    case L2T_STATE_VALID:     /* fast-path, send the packet on */
    case L2T_STATE_RESOLVING:
        mtx_lock(&e->lock);
        if (e->state != L2T_STATE_RESOLVING) { // ARP already completed
            mtx_unlock(&e->lock);
            goto again;
        }
        arpq_enqueue(e, m);
        mtx_unlock(&e->lock);

        if ((m0 = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
            return (ENOMEM);
        /*
         * Only the first packet added to the arpq should kick off
         * resolution.  However, because the m_gethdr below can fail,
         * we allow each packet added to the arpq to retry resolution
         * as a way of recovering from transient memory exhaustion.
         * A better way would be to use a work request to retry L2T
         * entries when there's no memory.
         */
        if (arpresolve(rt->rt_ifp, rt, m0, rt->rt_gateway, RT_ENADDR(rt)) == 0) {

            mtx_lock(&e->lock);
            if (e->arpq_head)
                setup_l2e_send_pending(dev, m, e);
            else
                m_freem(m);
            mtx_unlock(&e->lock);
        }
    }
    return 0;
}

void
t3_l2t_send_event(struct toedev *dev, struct l2t_entry *e)
{
    struct rtentry *rt;
    struct mbuf *m0;

    if ((m0 = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
        return;

    rt = e->neigh;
again:
    switch (e->state) {
    case L2T_STATE_STALE:     /* entry is stale, kick off revalidation */
        arpresolve(rt->rt_ifp, rt, m0, rt->rt_gateway, RT_ENADDR(rt));
        mtx_lock(&e->lock);
        if (e->state == L2T_STATE_STALE) {
            e->state = L2T_STATE_VALID;
        }
        mtx_unlock(&e->lock);
        return;
    case L2T_STATE_VALID:     /* fast-path, send the packet on */
        return;
    case L2T_STATE_RESOLVING:
        mtx_lock(&e->lock);
        if (e->state != L2T_STATE_RESOLVING) { // ARP already completed
            mtx_unlock(&e->lock);
            goto again;
        }
        mtx_unlock(&e->lock);

        if ((m0 = m_gethdr(M_NOWAIT, MT_DATA)) == NULL)
            return;
        /*
         * Only the first packet added to the arpq should kick off
         * resolution.  However, because the alloc_skb below can fail,
         * we allow each packet added to the arpq to retry resolution
         * as a way of recovering from transient memory exhaustion.
         * A better way would be to use a work request to retry L2T
         * entries when there's no memory.
         */
        arpresolve(rt->rt_ifp, rt, m0, rt->rt_gateway, RT_ENADDR(rt));

    }
    return;
}
/*
 * Allocate a free L2T entry.  Must be called with l2t_data.lock held.
 */
static struct l2t_entry *
alloc_l2e(struct l2t_data *d)
{
    struct l2t_entry *end, *e, **p;

    if (!atomic_load_acq_int(&d->nfree))
        return NULL;

    /* there's definitely a free entry */
    for (e = d->rover, end = &d->l2tab[d->nentries]; e != end; ++e)
        if (atomic_load_acq_int(&e->refcnt) == 0)
            goto found;

    for (e = &d->l2tab[1]; atomic_load_acq_int(&e->refcnt); ++e) ;
found:
    d->rover = e + 1;
    atomic_add_int(&d->nfree, -1);

    /*
     * The entry we found may be an inactive entry that is
     * presently in the hash table.  We need to remove it.
     */
    if (e->state != L2T_STATE_UNUSED) {
        int hash = arp_hash(e->addr, e->ifindex, d);

        for (p = &d->l2tab[hash].first; *p; p = &(*p)->next)
            if (*p == e) {
                *p = e->next;
                break;
            }
        e->state = L2T_STATE_UNUSED;
    }
    return e;
}

/*
 * Called when an L2T entry has no more users.  The entry is left in the hash
 * table since it is likely to be reused but we also bump nfree to indicate
 * that the entry can be reallocated for a different neighbor.  We also drop
 * the existing neighbor reference in case the neighbor is going away and is
 * waiting on our reference.
 *
 * Because entries can be reallocated to other neighbors once their ref count
 * drops to 0 we need to take the entry's lock to avoid races with a new
 * incarnation.
 */
void
t3_l2e_free(struct l2t_data *d, struct l2t_entry *e)
{
    mtx_lock(&e->lock);
    if (atomic_load_acq_int(&e->refcnt) == 0) {  /* hasn't been recycled */
        if (e->neigh) {
            RT_LOCK(e->neigh);
            RT_REMREF(e->neigh);
            RT_UNLOCK(e->neigh);
            e->neigh = NULL;
        }
    }
    mtx_unlock(&e->lock);
    atomic_add_int(&d->nfree, 1);
}

/*
 * Update an L2T entry that was previously used for the same next hop as neigh.
 * Must be called with softirqs disabled.
 */
static inline void
reuse_entry(struct l2t_entry *e, struct rtentry *neigh)
{
    struct llinfo_arp *la;

    la = (struct llinfo_arp *)neigh->rt_llinfo;

    mtx_lock(&e->lock);                /* avoid race with t3_l2t_free */
    if (neigh != e->neigh)
        neigh_replace(e, neigh);

    if (memcmp(e->dmac, RT_ENADDR(neigh), sizeof(e->dmac)) ||
        (neigh->rt_expire > time_uptime))
        e->state = L2T_STATE_RESOLVING;
    else if (la->la_hold == NULL)
        e->state = L2T_STATE_VALID;
    else
        e->state = L2T_STATE_STALE;
    mtx_unlock(&e->lock);
}

struct l2t_entry *
t3_l2t_get(struct toedev *dev, struct rtentry *neigh,
                 unsigned int smt_idx)
{
    struct l2t_entry *e;
    struct l2t_data *d = L2DATA(dev);
    u32 addr = ((struct sockaddr_in *)rt_getkey(neigh))->sin_addr.s_addr;
    int ifidx = neigh->rt_ifp->if_index;
    int hash = arp_hash(addr, ifidx, d);

    rw_wlock(&d->lock);
    for (e = d->l2tab[hash].first; e; e = e->next)
        if (e->addr == addr && e->ifindex == ifidx &&
            e->smt_idx == smt_idx) {
            l2t_hold(d, e);
            if (atomic_load_acq_int(&e->refcnt) == 1)
                reuse_entry(e, neigh);
            goto done;
        }

    /* Need to allocate a new entry */
    e = alloc_l2e(d);
    if (e) {
        mtx_lock(&e->lock);          /* avoid race with t3_l2t_free */
        e->next = d->l2tab[hash].first;
        d->l2tab[hash].first = e;
        e->state = L2T_STATE_RESOLVING;
        e->addr = addr;
        e->ifindex = ifidx;
        e->smt_idx = smt_idx;
        atomic_store_rel_int(&e->refcnt, 1);
        neigh_replace(e, neigh);
#ifdef notyet
        /*
         * XXX need to add accessor function for vlan tag
         */
        if (neigh->rt_ifp->if_vlantrunk)
            e->vlan = VLAN_DEV_INFO(neigh->dev)->vlan_id;
        else
#endif
            e->vlan = VLAN_NONE;
        mtx_unlock(&e->lock);
    }
done:
    rw_wunlock(&d->lock);
    return e;
}

/*
 * Called when address resolution fails for an L2T entry to handle packets
 * on the arpq head.  If a packet specifies a failure handler it is invoked,
 * otherwise the packets is sent to the TOE.
 *
 * XXX: maybe we should abandon the latter behavior and just require a failure
 * handler.
 */
static void
handle_failed_resolution(struct toedev *dev, struct mbuf *arpq)
{

    while (arpq) {
        struct mbuf *m = arpq;
#ifdef notyet
        struct l2t_mbuf_cb *cb = L2T_MBUF_CB(m);
#endif
        arpq = m->m_next;
        m->m_next = NULL;
#ifdef notyet
        if (cb->arp_failure_handler)
            cb->arp_failure_handler(dev, m);
        else
#endif
    }

}

#if defined(NETEVENT) || !defined(CONFIG_CHELSIO_T3_MODULE)
/*
 * Called when the host's ARP layer makes a change to some entry that is
 * loaded into the HW L2 table.
 */
void
t3_l2t_update(struct toedev *dev, struct rtentry *neigh)
{
    struct l2t_entry *e;
    struct mbuf *arpq = NULL;
    struct l2t_data *d = L2DATA(dev);
    u32 addr = ((struct sockaddr_in *)rt_getkey(neigh))->sin_addr.s_addr;
    int ifidx = neigh->rt_ifp->if_index;
    int hash = arp_hash(addr, ifidx, d);
    struct llinfo_arp *la;

    rw_rlock(&d->lock);
    for (e = d->l2tab[hash].first; e; e = e->next)
        if (e->addr == addr && e->ifindex == ifidx) {
            mtx_lock(&e->lock);
            goto found;
        }
    rw_runlock(&d->lock);
    return;

found:
    rw_runlock(&d->lock);
    if (atomic_load_acq_int(&e->refcnt)) {
        if (neigh != e->neigh)
            neigh_replace(e, neigh);

        la = (struct llinfo_arp *)neigh->rt_llinfo;
        if (e->state == L2T_STATE_RESOLVING) {

            if (la->la_asked >= 5 /* arp_maxtries */) {
                arpq = e->arpq_head;
                e->arpq_head = e->arpq_tail = NULL;
            } else if (la->la_hold == NULL)
                setup_l2e_send_pending(dev, NULL, e);
        } else {
            e->state = (la->la_hold == NULL) ?
                L2T_STATE_VALID : L2T_STATE_STALE;
            if (memcmp(e->dmac, RT_ENADDR(neigh), 6))
                setup_l2e_send_pending(dev, NULL, e);
        }
    }
    mtx_unlock(&e->lock);

    if (arpq)
        handle_failed_resolution(dev, arpq);
}
#else
/*
 * Called from a kprobe, interrupts are off.
 */
void
t3_l2t_update(struct toedev *dev, struct rtentry *neigh)
{
    struct l2t_entry *e;
    struct l2t_data *d = L2DATA(dev);
    u32 addr = *(u32 *) rt_key(neigh);
    int ifidx = neigh->dev->ifindex;
    int hash = arp_hash(addr, ifidx, d);

    rw_rlock(&d->lock);
    for (e = d->l2tab[hash].first; e; e = e->next)
        if (e->addr == addr && e->ifindex == ifidx) {
            mtx_lock(&e->lock);
            if (atomic_load_acq_int(&e->refcnt)) {
                if (neigh != e->neigh)
                    neigh_replace(e, neigh);
                e->tdev = dev;
                mod_timer(&e->update_timer, jiffies + 1);
            }
            mtx_unlock(&e->lock);
            break;
        }
    rw_runlock(&d->lock);
}

static void
update_timer_cb(unsigned long data)
{
    struct mbuf *arpq = NULL;
    struct l2t_entry *e = (struct l2t_entry *)data;
    struct rtentry *neigh = e->neigh;
    struct toedev *dev = e->tdev;

    barrier();
    if (!atomic_load_acq_int(&e->refcnt))
        return;

    rw_rlock(&neigh->lock);
    mtx_lock(&e->lock);

    if (atomic_load_acq_int(&e->refcnt)) {
        if (e->state == L2T_STATE_RESOLVING) {
            if (neigh->nud_state & NUD_FAILED) {
                arpq = e->arpq_head;
                e->arpq_head = e->arpq_tail = NULL;
            } else if (neigh_is_connected(neigh) && e->arpq_head)
                setup_l2e_send_pending(dev, NULL, e);
        } else {
            e->state = neigh_is_connected(neigh) ?
                L2T_STATE_VALID : L2T_STATE_STALE;
            if (memcmp(e->dmac, RT_ENADDR(neigh), sizeof(e->dmac)))
                setup_l2e_send_pending(dev, NULL, e);
        }
    }
    mtx_unlock(&e->lock);
    rw_runlock(&neigh->lock);

    if (arpq)
        handle_failed_resolution(dev, arpq);
}
#endif

struct l2t_data *
t3_init_l2t(unsigned int l2t_capacity)
{
    struct l2t_data *d;
    int i, size = sizeof(*d) + l2t_capacity * sizeof(struct l2t_entry);

    d = cxgb_alloc_mem(size);
    if (!d)
        return NULL;

    d->nentries = l2t_capacity;
    d->rover = &d->l2tab[1];    /* entry 0 is not used */
    atomic_store_rel_int(&d->nfree, l2t_capacity - 1);
    rw_init(&d->lock, "L2T");

    for (i = 0; i < l2t_capacity; ++i) {
        d->l2tab[i].idx = i;
        d->l2tab[i].state = L2T_STATE_UNUSED;
        mtx_init(&d->l2tab[i].lock, "L2TAB", NULL, MTX_DEF);
        atomic_store_rel_int(&d->l2tab[i].refcnt, 0);
#ifndef NETEVENT
#ifdef CONFIG_CHELSIO_T3_MODULE
        setup_timer(&d->l2tab[i].update_timer, update_timer_cb,
                (unsigned long)&d->l2tab[i]);
#endif
#endif
    }
    return d;
}

void
t3_free_l2t(struct l2t_data *d)
{
#ifndef NETEVENT
#ifdef CONFIG_CHELSIO_T3_MODULE
    int i;

    /* Stop all L2T timers */
    for (i = 0; i < d->nentries; ++i)
        del_timer_sync(&d->l2tab[i].update_timer);
#endif
#endif
    cxgb_free_mem(d);
}

#ifdef CONFIG_PROC_FS
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static inline void *
l2t_get_idx(struct seq_file *seq, loff_t pos)
{
    struct l2t_data *d = seq->private;

    return pos >= d->nentries ? NULL : &d->l2tab[pos];
}

static void *
l2t_seq_start(struct seq_file *seq, loff_t *pos)
{
    return *pos ? l2t_get_idx(seq, *pos) : SEQ_START_TOKEN;
}

static void *
l2t_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
    v = l2t_get_idx(seq, *pos + 1);
    if (v)
        ++*pos;
    return v;
}

static void
l2t_seq_stop(struct seq_file *seq, void *v)
{
}

static char
l2e_state(const struct l2t_entry *e)
{
    switch (e->state) {
    case L2T_STATE_VALID: return 'V';  /* valid, fast-path entry */
    case L2T_STATE_STALE: return 'S';  /* needs revalidation, but usable */
    case L2T_STATE_RESOLVING:
        return e->arpq_head ? 'A' : 'R';
    default:
        return 'U';
    }
}

static int
l2t_seq_show(struct seq_file *seq, void *v)
{
    if (v == SEQ_START_TOKEN)
        seq_puts(seq, "Index IP address      Ethernet address   VLAN  "
             "Prio  State   Users SMTIDX  Port\n");
    else {
        char ip[20];
        struct l2t_entry *e = v;

        mtx_lock(&e->lock);
        snprintf(ip, sizeof(ip), "%u.%u.%u.%u", NIPQUAD(e->addr));
        seq_printf(seq, "%-5u %-15s %02x:%02x:%02x:%02x:%02x:%02x  %4d"
               "  %3u     %c   %7u   %4u %s\n",
               e->idx, ip, e->dmac[0], e->dmac[1], e->dmac[2],
               e->dmac[3], e->dmac[4], e->dmac[5],
               e->vlan & EVL_VLID_MASK, vlan_prio(e),
               l2e_state(e), atomic_load_acq_int(&e->refcnt), e->smt_idx,
               e->neigh ? e->neigh->dev->name : "");
        mtx_unlock(&e->lock);
    }
    return 0;
}

#endif
