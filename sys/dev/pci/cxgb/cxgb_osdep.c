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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/queue.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/cxgb/cxgb_include.h>
#include <altq/altq_conf.h>

int cxgb_initialized = FALSE;

int atomic_fetchadd_int(volatile int *p, int v)
{
    int tmp = *p;
    *p += v;
    return (tmp);
}

#if 0
int atomic_add_int(volatile int *p, int v)
{
    return (*p += v);
}
#endif

int atomic_load_acq_int(volatile int *p)
{
    return (*p);
}

void atomic_store_rel_int(volatile int *p, int v)
{
    *p = v;
}

u_short in_cksum_hdr(struct ip *ih)
{
	u_long sum = 0;
	u_short *p = (u_short *)ih;
	int i;

        i = ih->ip_hl*2;
	while (i--)
		sum += *p++;

	if (sum > 0xffff)
		sum -= 0xffff;

	return (~sum);
}

void m_cljset(struct mbuf *m, void *cl, int type)
{
    MEXTADD(m, cl, m->m_len, M_DEVBUF, NULL, NULL);
}

int
_m_explode(struct mbuf *m)
{
        int i, offset, type, first, len;
        uint8_t *cl;
        struct mbuf *m0, *head = NULL;
        struct mbuf_vec *mv;

#ifdef INVARIANTS
        len = m->m_len;
        m0 = m->m_next;
        while (m0) {
                KASSERT((m0->m_flags & M_PKTHDR) == 0,
                    ("pkthdr set on intermediate mbuf - pre"));
                len += m0->m_len;
                m0 = m0->m_next;

        }
        if (len != m->m_pkthdr.len)
                panic("at start len=%d pktlen=%d", len, m->m_pkthdr.len);
#endif
        mv = (struct mbuf_vec *)((m)->m_pktdat);
        first = mv->mv_first;
        for (i = mv->mv_count + first - 1; i > first; i--) {
                type = mbuf_vec_get_type(mv, i);
                cl = mv->mv_vec[i].mi_base;
                offset = mv->mv_vec[i].mi_offset;
                len = mv->mv_vec[i].mi_len;
#if 0
                if (__predict_false(type == EXT_MBUF)) {
                        m0 = (struct mbuf *)cl;
                        KASSERT((m0->m_flags & M_EXT) == 0);
                        m0->m_len = len;
                        m0->m_data = cl + offset;
                        goto skip_cluster;

                } else 
#endif
		if ((m0 = m_get(M_NOWAIT, MT_DATA)) == NULL) {
                        /*
                         * Check for extra memory leaks
                         */
                        m_freem(head);
                        return (ENOMEM);
                }
                m0->m_flags = 0;

                m0->m_len = mv->mv_vec[i].mi_len;
                m_cljset(m0, (uint8_t *)cl, type);
                if (offset)
                        m_adj(m0, offset);
//        skip_cluster:
                m0->m_next = head;
                m->m_len -= m0->m_len;
                head = m0;
        }
        offset = mv->mv_vec[first].mi_offset;
        cl = mv->mv_vec[first].mi_base;
        type = mbuf_vec_get_type(mv, first);
        m->m_flags &= ~(M_IOVEC);
        m_cljset(m, cl, type);
        if (offset)
                m_adj(m, offset);
        m->m_next = head;
        head = m;
        M_SANITY(m, 0);

        return (0);
}

/*
 * Allocate a chunk of memory using kmalloc or, if that fails, vmalloc.
 * The allocated memory is cleared.
 */
void *
cxgb_alloc_mem(unsigned long size)
{
    return malloc(size, M_DEVBUF, M_ZERO);
}

/*
 * Free memory allocated through t3_alloc_mem().
 */
void
cxgb_free_mem(void *addr)
{
    free(addr, M_DEVBUF);
}

void pci_enable_busmaster(device_t dev)
{
    adapter_t *sc = (adapter_t *)dev;
    uint32_t reg;

    t3_os_pci_read_config_4(sc, PCI_COMMAND_STATUS_REG, &reg);
    reg |= PCI_COMMAND_MASTER_ENABLE;
    t3_os_pci_write_config_4(sc, PCI_COMMAND_STATUS_REG, reg);
}
