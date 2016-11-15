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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>

#include <dev/mii/mii.h>

#ifndef _CXGB_OSDEP_H_
#define _CXGB_OSDEP_H_

typedef char *caddr_t;
#include <dev/pci/cxgb/cxgb_version.h>
#include <dev/pci/cxgb/cxgb_config.h>
#include <sys/mbuf.h>
#include <sys/bus.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <sys/kthread.h>
#include <sys/workqueue.h>

#include <sys/atomic.h>

void pci_enable_busmaster(device_t dev);

struct cxgb_task
{
    const char *name;
    void (*func)(struct work *, void *);
    struct workqueue *wq;
    struct work w;
    void *context;
};

void cxgb_make_task(void *);

void m_cljset(struct mbuf *m, void *cl, int type);

#define mtx kmutex_t
#define mtx_init(a, b, c, d) { mutex_init(a, MUTEX_DEFAULT, IPL_HIGH) }
#define mtx_destroy(a)
#define mtx_lock(a) mutex_spin_enter(a)
#define mtx_unlock(a) mutex_spin_exit(a)
#define mtx_trylock(a) mutex_tryenter(a)
#define MA_OWNED 1
#define MA_NOTOWNED 0
#define mtx_assert(a, w) 

#if 0
#define RT_LOCK_INIT(_rt) \
        mtx_init(&(_rt)->rt_mtx, "rtentry", NULL, MTX_DEF | MTX_DUPOK)
#define RT_LOCK(_rt)            mtx_lock(&(_rt)->rt_mtx)
#define RT_UNLOCK(_rt)          mtx_unlock(&(_rt)->rt_mtx)
#define RT_LOCK_DESTROY(_rt)    mtx_destroy(&(_rt)->rt_mtx)
#define RT_LOCK_ASSERT(_rt)     mtx_assert(&(_rt)->rt_mtx, MA_OWNED)
#else
#define RT_LOCK_INIT(_rt) 
#define RT_LOCK(_rt)            
#define RT_UNLOCK(_rt)         
#define RT_LOCK_DESTROY(_rt)  
#define RT_LOCK_ASSERT(_rt)  
#endif

#define RT_ADDREF(_rt)  do {                                    \
        RT_LOCK_ASSERT(_rt);                                    \
        KASSERT((_rt)->rt_refcnt >= 0);                         \
        (_rt)->rt_refcnt++;                                     \
} while (0)
#define RT_REMREF(_rt)  do {                                    \
        RT_LOCK_ASSERT(_rt);                                    \
        KASSERT((_rt)->rt_refcnt > 0);                          \
        (_rt)->rt_refcnt--;                                     \
} while (0)


#define EVL_VLID_MASK       0x0FFF

static inline void critical_enter(void)
{
}

static inline void critical_exit(void)
{
}

int atomic_fetchadd_int(volatile int *p, int v);
#if 0
int atomic_add_int(volatile int *p, int v);
#endif
int atomic_load_acq_int(volatile int *p);
void atomic_store_rel_int(volatile int *p, int v);

u_short in_cksum_hdr(struct ip *ih);

#define if_drv_flags if_flags
#define IFF_DRV_RUNNING IFF_RUNNING
#define IFF_DRV_OACTIVE IFF_OACTIVE

#define MJUM16BYTES (16*1024)
#define MJUMPAGESIZE PAGE_SIZE

#if 0
#define rw_rlock(x) rw_enter(x, RW_READER)
#define rw_runlock(x) rw_exit(x)
#define rw_wlock(x) rw_enter(x, RW_WRITER)
#define rw_wunlock(x) rw_exit(x)
#endif

#define callout_drain(x) callout_stop(x)

static inline int atomic_cmpset_ptr(volatile long *dst, long exp, long src)
{
    if (*dst == exp)
    {
        *dst = src;
        return (1);
    }
    return (0);
}
#define atomic_cmpset_int(a, b, c) atomic_cmpset_ptr((volatile long *)a, (long)b, (long)c)

static inline int atomic_set_int(volatile int *dst, int val)
{
    *dst = val;

    return (val);
}

static inline void log(int x, ...)
{
}

struct cxgb_attach_args
{
    int port;
};

#define INT3 __asm("int $3")

typedef struct adapter adapter_t;
struct sge_rspq;

struct t3_mbuf_hdr {
    struct mbuf *mh_head;
    struct mbuf *mh_tail;
};


#define PANIC_IF(exp) do {                  \
    if (exp)                            \
        panic("BUG: %s", exp);      \
} while (0)


#define m_get_priority(m) ((uintptr_t)(m)->m_pkthdr.rcvif)
#define m_set_priority(m, pri) ((m)->m_pkthdr.rcvif = (struct ifnet *)((uintptr_t)pri))

#define if_name(ifp) (ifp)->if_xname
#define M_SANITY(m, n)

/*
 * Workaround for weird Chelsio issue
 */
#define CXGB_TX_CLEANUP_THRESHOLD        32

#define LOG_WARNING                       1
#define LOG_ERR                           2

#define DPRINTF printf

#define TX_MAX_SIZE                (1 << 16)    /* 64KB                          */
#define TX_MAX_SEGS                      36     /* maximum supported by card     */
#define TX_MAX_DESC                       4     /* max descriptors per packet    */

#define TX_START_MIN_DESC  (TX_MAX_DESC << 2)

#if 0
#define TX_START_MAX_DESC (TX_ETH_Q_SIZE >> 2)  /* maximum number of descriptors */
#endif

#define TX_START_MAX_DESC (TX_MAX_DESC << 3)    /* maximum number of descriptors
                         * call to start used per    */

#define TX_CLEAN_MAX_DESC (TX_MAX_DESC << 4)    /* maximum tx descriptors
                         * to clean per iteration        */


#if defined(__i386__) || defined(__amd64__)
#define mb()    __asm volatile("mfence":::"memory")
#define rmb()   __asm volatile("lfence":::"memory")
#define wmb()   __asm volatile("sfence" ::: "memory")
#define smp_mb() mb()

#define L1_CACHE_BYTES 64
static __inline
void prefetch(void *x)
{
        __asm volatile("prefetcht0 %0" :: "m" (*(unsigned long *)x));
}

extern void kdb_backtrace(void);

#define WARN_ON(condition) do { \
        if (unlikely((condition)!=0)) { \
                log(LOG_WARNING, "BUG: warning at %s:%d/%s()\n", __FILE__, __LINE__, __func__); \
                kdb_backtrace(); \
        } \
} while (0)


#else /* !i386 && !amd64 */
#define mb()
#define rmb()
#define wmb()
#define smp_mb()
#define prefetch(x)
#define L1_CACHE_BYTES 32
#endif
#define DBG_RX          (1 << 0)
static const int debug_flags = DBG_RX;

#ifdef DEBUG_PRINT
#define DBG(flag, msg) do { \
    if ((flag & debug_flags))   \
        printf msg; \
} while (0)
#else
#define DBG(...)
#endif

#define promisc_rx_mode(rm)  ((rm)->port->ifp->if_flags & IFF_PROMISC)
#define allmulti_rx_mode(rm) ((rm)->port->ifp->if_flags & IFF_ALLMULTI)

#define CH_ERR(adap, fmt, ...) { }

#define CH_WARN(adap, fmt, ...) { }
#define CH_ALERT(adap, fmt, ...) { }

#define t3_os_sleep(x) DELAY((x) * 1000)

#define test_and_clear_bit(bit, p) atomic_cmpset_int((p), ((*(p)) | bit), ((*(p)) & ~bit))


#define max_t(type, a, b) (type)max((a), (b))
#define net_device ifnet
#define cpu_to_be32            htobe32



/* Standard PHY definitions */
#define BMCR_LOOPBACK       BMCR_LOOP
#define BMCR_ISOLATE        BMCR_ISO
#define BMCR_ANENABLE       BMCR_AUTOEN
#define BMCR_SPEED1000      BMCR_SPEED1
#define BMCR_SPEED100       BMCR_SPEED0
#define BMCR_ANRESTART      BMCR_STARTNEG
#define BMCR_FULLDPLX       BMCR_FDX
#define BMSR_LSTATUS        BMSR_LINK
#define BMSR_ANEGCOMPLETE   BMSR_ACOMP

#define MII_LPA         MII_ANLPAR
#define MII_ADVERTISE       MII_ANAR
#define MII_CTRL1000        MII_100T2CR

#define ADVERTISE_PAUSE_CAP ANAR_FC
#define ADVERTISE_PAUSE_ASYM    0x0800
#define ADVERTISE_1000HALF  ANAR_X_HD
#define ADVERTISE_1000FULL  ANAR_X_FD
#define ADVERTISE_10FULL    ANAR_10_FD
#define ADVERTISE_10HALF    ANAR_10
#define ADVERTISE_100FULL   ANAR_TX_FD
#define ADVERTISE_100HALF   ANAR_TX

/* Standard PCI Extended Capaibilities definitions */
#define PCI_CAP_ID_VPD  0x03
#define PCI_VPD_ADDR    2
#define PCI_VPD_ADDR_F  0x8000
#define PCI_VPD_DATA    4

#define PCI_CAP_ID_EXP  0x10
#define PCI_EXP_DEVCTL  8
#define PCI_EXP_DEVCTL_PAYLOAD 0x00e0
#define PCI_EXP_LNKCTL  16
#define PCI_EXP_LNKSTA  18

/*
 * Linux compatibility macros
 */

/* Some simple translations */
#define __devinit
#define udelay(x) DELAY(x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define le32_to_cpu(x) le32toh(x)
#define cpu_to_le32(x) htole32(x)
#define swab32(x) bswap32(x)
#define simple_strtoul strtoul

/* More types and endian definitions */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint8_t __be8;
typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;

#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN_BITFIELD
#elif BYTE_ORDER == LITTLE_ENDIAN
#define __LITTLE_ENDIAN_BITFIELD
#else
#error "Must set BYTE_ORDER"
#endif

/* Indicates what features are supported by the interface. */
#define SUPPORTED_10baseT_Half          (1 << 0)
#define SUPPORTED_10baseT_Full          (1 << 1)
#define SUPPORTED_100baseT_Half         (1 << 2)
#define SUPPORTED_100baseT_Full         (1 << 3)
#define SUPPORTED_1000baseT_Half        (1 << 4)
#define SUPPORTED_1000baseT_Full        (1 << 5)
#define SUPPORTED_Autoneg               (1 << 6)
#define SUPPORTED_TP                    (1 << 7)
#define SUPPORTED_AUI                   (1 << 8)
#define SUPPORTED_MII                   (1 << 9)
#define SUPPORTED_FIBRE                 (1 << 10)
#define SUPPORTED_BNC                   (1 << 11)
#define SUPPORTED_10000baseT_Full       (1 << 12)
#define SUPPORTED_Pause                 (1 << 13)
#define SUPPORTED_Asym_Pause            (1 << 14)

/* Indicates what features are advertised by the interface. */
#define ADVERTISED_10baseT_Half         (1 << 0)
#define ADVERTISED_10baseT_Full         (1 << 1)
#define ADVERTISED_100baseT_Half        (1 << 2)
#define ADVERTISED_100baseT_Full        (1 << 3)
#define ADVERTISED_1000baseT_Half       (1 << 4)
#define ADVERTISED_1000baseT_Full       (1 << 5)
#define ADVERTISED_Autoneg              (1 << 6)
#define ADVERTISED_TP                   (1 << 7)
#define ADVERTISED_AUI                  (1 << 8)
#define ADVERTISED_MII                  (1 << 9)
#define ADVERTISED_FIBRE                (1 << 10)
#define ADVERTISED_BNC                  (1 << 11)
#define ADVERTISED_10000baseT_Full      (1 << 12)
#define ADVERTISED_Pause                (1 << 13)
#define ADVERTISED_Asym_Pause           (1 << 14)

/* Enable or disable autonegotiation.  If this is set to enable,
 * the forced link modes above are completely ignored.
 */
#define AUTONEG_DISABLE         0x00
#define AUTONEG_ENABLE          0x01

#define SPEED_10        10
#define SPEED_100       100
#define SPEED_1000      1000
#define SPEED_10000     10000
#define DUPLEX_HALF     0
#define DUPLEX_FULL     1

#endif
