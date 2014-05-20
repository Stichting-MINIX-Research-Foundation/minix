/*
 * lance.c
 *
 * This file contains a ethernet device driver for AMD LANCE based ethernet
 * cards.
 *
 * The valid messages and their parameters are:
 *
 *   m_type        DL_COUNT   DL_MODE   DL_GRANT
 * |--------------+----------+---------+---------|
 * | DL_WRITEV_S  | count    |         | grant   |
 * |--------------|----------|---------|---------|
 * | DL_READV_S   | count    |         | grant   |
 * |--------------|----------|---------|---------|
 * | DL_CONF      |          | mode    |         |
 * |--------------|----------|---------|---------|
 * | DL_GETSTAT_S |          |         | grant   |
 * |--------------|----------|---------|---------|
 * | hardware int |          |         |         |
 * |--------------|----------|---------|---------|
 *
 * The messages sent are:
 *
 *   m_type         DL_COUNT   DL_FLAGS
 * |---------------+----------+---------|
 * | DL_TASK_REPLY | rd-count | flags   |
 * |---------------|----------|---------|
 *
 *   m_type
 * |---------------|
 * | DL_STAT_REPLY |
 * |---------------|
 *
 *   m_type         DL_STAT   DL_ADDR
 * |---------------+---------+---------------|
 * | DL_CONF_REPLY | code    | ethernet addr |
 * |---------------|---------|---------------|
 *
 * Created: Jul 27, 2002 by Kazuya Kodama <kazuya@nii.ac.jp>
 * Adapted for Minix 3: Sep 05, 2005 by Joren l'Ami <jwlami@cs.vu.nl>
 */

#define VERBOSE 0 /* Verbose debugging output */
#define LANCE_FKEY 0 /* Use function key to dump Lance stats */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <assert.h>

#include <minix/syslib.h>
#include <minix/endpoint.h>
#include <machine/pci.h>
#include <minix/ds.h>

#include "lance.h"

static ether_card_t ec_state;
static int ec_instance;

/* Configuration */
typedef struct ec_conf
{
   port_t ec_port;
   int ec_irq;
   phys_bytes ec_mem;
} ec_conf_t;

/* We hardly use these. Just "LANCE0=on/off" "LANCE1=on/off" mean. */
#define EC_CONF_NR 3
ec_conf_t ec_conf[EC_CONF_NR]=    /* Card addresses */
{
   /* I/O port, IRQ,  Buffer address. */
   {  0x1000,     9,    0x00000,     },
   {  0xD000,    15,    0x00000,     },
   {  0x0000,     0,    0x00000,     },
};

/* General */
static void do_init(message *mp);
static void ec_init(ether_card_t *ec);
static void ec_confaddr(ether_card_t *ec);
static void ec_reinit(ether_card_t *ec);
static void ec_check_ints(ether_card_t *ec);
static void conf_hw(ether_card_t *ec);
static void update_conf(ether_card_t *ec, ec_conf_t *ecp);
static void mess_reply(message *req, message *reply);
static void do_int(ether_card_t *ec);
static void reply(ether_card_t *ec);
static void ec_reset(ether_card_t *ec);
static void ec_send(ether_card_t *ec);
static void ec_recv(ether_card_t *ec);
static void do_vwrite_s(message *mp, int from_int);
static void do_vread_s(const message *mp);
static void ec_user2nic(ether_card_t *dep, iovec_dat_t *iovp, vir_bytes
	offset, int nic_addr, vir_bytes count);
static void ec_nic2user(ether_card_t *ec, int nic_addr, iovec_dat_t
	*iovp, vir_bytes offset, vir_bytes count);
static int calc_iovec_size(iovec_dat_t *iovp);
static void ec_next_iovec(iovec_dat_t *iovp);
static void do_getstat_s(message *mp);
static void lance_stop(ether_card_t *ec);

static void lance_dump(void);
static void getAddressing(int devind, ether_card_t *ec);

/* probe+init LANCE cards */
static int lance_probe(ether_card_t *ec, int skip);
static void lance_init_card(ether_card_t *ec);

/* Accesses Lance Control and Status Registers */
static u8_t in_byte(port_t port);
static u16_t in_word(port_t port);
static void out_word(port_t port, u16_t value);
static u16_t read_csr(port_t ioaddr, u16_t csrno);
static void write_csr(port_t ioaddr, u16_t csrno, u16_t value);

/* --- LANCE --- */
/* General */
typedef unsigned long Address;

#define virt_to_bus(x)          (vir2phys((unsigned long)x))
unsigned long vir2phys( unsigned long x )
{
   int r;
   unsigned long value;

   if ( (r=sys_umap( SELF, VM_D, x, 4, &value )) != OK ) {
      printf("lance: umap of 0x%lx failed\n",x );
      panic("sys_umap failed: %d", r);
   }

   return value;
}

/* DMA limitations */
#define DMA_ADDR_MASK  0xFFFFFF  /* mask to verify DMA address is 24-bit */

#define CORRECT_DMA_MEM() ( (virt_to_bus(lance_buf + sizeof(struct lance_interface)) & ~DMA_ADDR_MASK) == 0 )

#define ETH_FRAME_LEN           1518

#define LANCE_MUST_PAD          0x00000001
#define LANCE_ENABLE_AUTOSELECT 0x00000002
#define LANCE_SELECT_PHONELINE  0x00000004
#define LANCE_MUST_UNRESET      0x00000008

static const struct lance_chip_type
{
   int        id_number;
   const char *name;
   int        flags;
} chip_table[] = {
   {0x0000, "LANCE 7990",           /* Ancient lance chip.  */
    LANCE_MUST_PAD + LANCE_MUST_UNRESET},
   {0x0003, "PCnet/ISA 79C960",     /* 79C960 PCnet/ISA.  */
    LANCE_ENABLE_AUTOSELECT},
   {0x2260, "PCnet/ISA+ 79C961",    /* 79C961 PCnet/ISA+, Plug-n-Play.  */
    LANCE_ENABLE_AUTOSELECT},
   {0x2420, "PCnet/PCI 79C970",     /* 79C970 or 79C974 PCnet-SCSI, PCI. */
    LANCE_ENABLE_AUTOSELECT},
   {0x2430, "PCnet32",              /* 79C965 PCnet for VL bus. */
    LANCE_ENABLE_AUTOSELECT},
   {0x2621, "PCnet/PCI-II 79C970A", /* 79C970A PCInetPCI II. */
    LANCE_ENABLE_AUTOSELECT},
   {0x2625, "PCnet-FAST III 79C973",/* 79C973 PCInet-FAST III. */
    LANCE_ENABLE_AUTOSELECT},
   {0x2626, "PCnet/HomePNA 79C978",
    LANCE_ENABLE_AUTOSELECT|LANCE_SELECT_PHONELINE},
   {0x0, "PCnet (unknown)",
    LANCE_ENABLE_AUTOSELECT},
};

/* ############## for LANCE device ############## */
#define LANCE_ETH_ADDR          0x0
#define LANCE_DATA              0x10
#define LANCE_ADDR              0x12
#define LANCE_RESET             0x14
#define LANCE_BUS_IF            0x16
#define LANCE_TOTAL_SIZE        0x18

/* Use 2^4=16 {Rx,Tx} buffers */
#define LANCE_LOG_RX_BUFFERS    4
#define RX_RING_SIZE            (1 << (LANCE_LOG_RX_BUFFERS))
#define RX_RING_MOD_MASK        (RX_RING_SIZE - 1)
#define RX_RING_LEN_BITS        ((LANCE_LOG_RX_BUFFERS) << 29)

#define LANCE_LOG_TX_BUFFERS    4
#define TX_RING_SIZE            (1 << (LANCE_LOG_TX_BUFFERS))
#define TX_RING_MOD_MASK        (TX_RING_SIZE - 1)
#define TX_RING_LEN_BITS        ((LANCE_LOG_TX_BUFFERS) << 29)

/* for lance_interface */
struct lance_init_block
{
   unsigned short  mode;
   unsigned char   phys_addr[6];
   unsigned long   filter[2];
   Address         rx_ring;
   Address         tx_ring;
};

struct lance_rx_head
{
   union {
      Address         base;
      unsigned char   addr[4];
   } u;
   short           buf_length;     /* 2s complement */
   short           msg_length;
};

struct lance_tx_head
{
   union {
      Address         base;
      unsigned char   addr[4];
   } u;
   short           buf_length;     /* 2s complement */
   short           misc;
};

struct lance_interface
{
   struct lance_init_block init_block;
   struct lance_rx_head    rx_ring[RX_RING_SIZE];
   struct lance_tx_head    tx_ring[TX_RING_SIZE];
   unsigned char           rbuf[RX_RING_SIZE][ETH_FRAME_LEN];
   unsigned char           tbuf[TX_RING_SIZE][ETH_FRAME_LEN];
};

/* =============== global variables =============== */
/* AKA the stuff that really should have been in ether_card_t */
static struct lance_interface  *lp;
#define LANCE_BUF_SIZE (sizeof(struct lance_interface))
static char *lance_buf = NULL;
static int rx_slot_nr = 0;          /* Rx-slot number */
static int tx_slot_nr = 0;          /* Tx-slot number */
static int cur_tx_slot_nr = 0;      /* Tx-slot number */
static char isstored[TX_RING_SIZE]; /* Tx-slot in-use */

phys_bytes lance_buf_phys;

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

/*===========================================================================*
 *                              main                                         *
 *===========================================================================*/
int main( int argc, char **argv )
{
   message m;
   int ipc_status;
   int r;
   ether_card_t *ec;

   /* SEF local startup. */
   env_setargs(argc, argv);
   sef_local_startup();

   ec= &ec_state;

   while (TRUE)
   {
      if (ec->ec_irq != 0)
         sys_irqenable(&ec->ec_hook);

      if ((r= netdriver_receive(ANY, &m, &ipc_status)) != OK)
        panic("netdriver_receive failed: %d", r);
        
      if (ec->ec_irq != 0)
         sys_irqdisable(&ec->ec_hook);

      if (is_ipc_notify(ipc_status)) {
	      switch(_ENDPOINT_P(m.m_source)) {
		      case TTY_PROC_NR:
			      lance_dump();
			      break;
		      case HARDWARE:
			      if (ec->mode == EC_ENABLED)
			      {
				 ec->ec_int_pending = 0;
				 ec_check_ints(ec);
				 do_int(ec);
			      }
			      break;
		      default:
			      panic("illegal notify source: %d", m.m_source);
	      }

	      /* get next message */
	      continue;
      }
  
      switch (m.m_type)
      {
      case DL_WRITEV_S:
         do_vwrite_s(&m, FALSE);
         break;
      case DL_READV_S:
         do_vread_s(&m);
         break;
      case DL_CONF:
         do_init(&m);
         break;
      case DL_GETSTAT_S:
         do_getstat_s(&m);
         break;
      default:
         panic("illegal message: %d", m.m_type);
      }
   }

   return 0;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* Register live update callbacks. */
  sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_workfree);

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *UNUSED(info))
{
/* Initialize the lance driver. */
   long v;
#if LANCE_FKEY
   int r, fkeys, sfkeys;
#endif

#if LANCE_FKEY
   fkeys = sfkeys = 0;
   bit_set( sfkeys, 7 );
   if ( (r = fkey_map(&fkeys, &sfkeys)) != OK )
      printf("Warning: lance couldn't observe Shift+F7 key: %d\n",r);
#endif

   v = 0;
   (void) env_parse("instance", "d", 0, &v, 0, 255);
   ec_instance = (int) v;

   /* Announce we are up! */
   netdriver_announce();

   return OK;
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{

   /* Only check for termination signal, ignore anything else. */
   if (signo != SIGTERM) return;

   if (ec_state.mode == EC_ENABLED)
      lance_stop(&ec_state);

#if VERBOSE
   printf("LANCE driver stopped.\n");
#endif

   exit(0);
}

/*===========================================================================*
 *                              lance_dump                                   *
 *===========================================================================*/
static void lance_dump()
{
   ether_card_t *ec;
   int isr, csr;
   unsigned short ioaddr;
  
   printf("\n");
   ec = &ec_state;
   if (ec->mode == EC_DISABLED)
      printf("lance instance %d is disabled\n", ec_instance);
   else if (ec->mode == EC_SINK)
      printf("lance instance %d is in sink mode\n", ec_instance);

   if (ec->mode != EC_ENABLED)
      return;
      
   printf("lance statistics of instance %d:\n", ec_instance);
      
   printf("recvErr    :%8ld\t", ec->eth_stat.ets_recvErr);
   printf("sendErr    :%8ld\t", ec->eth_stat.ets_sendErr);
   printf("OVW        :%8ld\n", ec->eth_stat.ets_OVW);
      
   printf("CRCerr     :%8ld\t", ec->eth_stat.ets_CRCerr);
   printf("frameAll   :%8ld\t", ec->eth_stat.ets_frameAll);
   printf("missedP    :%8ld\n", ec->eth_stat.ets_missedP);
      
   printf("packetR    :%8ld\t", ec->eth_stat.ets_packetR);
   printf("packetT    :%8ld\t", ec->eth_stat.ets_packetT);
   printf("transDef   :%8ld\n", ec->eth_stat.ets_transDef);
      
   printf("collision  :%8ld\t", ec->eth_stat.ets_collision);
   printf("transAb    :%8ld\t", ec->eth_stat.ets_transAb);
   printf("carrSense  :%8ld\n", ec->eth_stat.ets_carrSense);
      
   printf("fifoUnder  :%8ld\t", ec->eth_stat.ets_fifoUnder);
   printf("fifoOver   :%8ld\t", ec->eth_stat.ets_fifoOver);
   printf("CDheartbeat:%8ld\n", ec->eth_stat.ets_CDheartbeat);
      
   printf("OWC        :%8ld\t", ec->eth_stat.ets_OWC);
      
   ioaddr = ec->ec_port;
   isr = read_csr(ioaddr, LANCE_CSR0);
   printf("isr = 0x%x, flags = 0x%x\n", isr,
             ec->flags);

   printf("irq = %d\tioadr = 0x%x\n", ec->ec_irq, ec->ec_port);

   csr = read_csr(ioaddr, LANCE_CSR0);
   printf("CSR0: 0x%x\n", csr);
   csr = read_csr(ioaddr, LANCE_CSR3);
   printf("CSR3: 0x%x\n", csr);
   csr = read_csr(ioaddr, LANCE_CSR4);
   printf("CSR4: 0x%x\n", csr);
   csr = read_csr(ioaddr, LANCE_CSR5);
   printf("CSR5: 0x%x\n", csr);
   csr = read_csr(ioaddr, LANCE_CSR15);
   printf("CSR15: 0x%x\n", csr);
}

/*===========================================================================*
 *                              do_init                                      *
 *===========================================================================*/
static void do_init(mp)
message *mp;
{
   ether_card_t *ec;
   message reply_mess;

   pci_init();

   if(!lance_buf && !(lance_buf = alloc_contig(LANCE_BUF_SIZE, AC_ALIGN4K|AC_LOWER16M, &lance_buf_phys))) {
      panic("alloc_contig failed: %d", LANCE_BUF_SIZE);
   }

   ec= &ec_state;
   strlcpy(ec->port_name, "lance#0", sizeof(ec->port_name));
   ec->port_name[6] += ec_instance;

   if (ec->mode == EC_DISABLED)
   {
      /* This is the default, try to (re)locate the device. */
      /* only try to enable if memory is correct for DMA */
      if ( CORRECT_DMA_MEM() )
      {
         conf_hw(ec);
      }
      else
      {
         printf("LANCE: DMA denied because address out of range\n" );
      }

      if (ec->mode == EC_DISABLED)
      {
         /* Probe failed, or the device is configured off. */
         reply_mess.m_type= DL_CONF_REPLY;
         reply_mess.m_netdrv_net_dl_conf.stat = ENXIO;
         mess_reply(mp, &reply_mess);
         return;
      }
      if (ec->mode == EC_ENABLED)
         ec_init(ec);
   }

   if (ec->mode == EC_SINK)
   {
      ec->mac_address.ea_addr[0] = 
         ec->mac_address.ea_addr[1] =
         ec->mac_address.ea_addr[2] =
         ec->mac_address.ea_addr[3] =
         ec->mac_address.ea_addr[4] =
         ec->mac_address.ea_addr[5] = 0;
      ec_confaddr(ec);
      reply_mess.m_type = DL_CONF_REPLY;
      reply_mess.m_netdrv_net_dl_conf.stat = OK;
      memcpy(reply_mess.m_netdrv_net_dl_conf.hw_addr, ec->mac_address.ea_addr,
	      sizeof(reply_mess.m_netdrv_net_dl_conf.hw_addr));
      mess_reply(mp, &reply_mess);
      return;
   }
   assert(ec->mode == EC_ENABLED);
   assert(ec->flags & ECF_ENABLED);

   ec->flags &= ~(ECF_PROMISC | ECF_MULTI | ECF_BROAD);

   if (mp->m_net_netdrv_dl_conf.mode & DL_PROMISC_REQ)
      ec->flags |= ECF_PROMISC | ECF_MULTI | ECF_BROAD;
   if (mp->m_net_netdrv_dl_conf.mode & DL_MULTI_REQ)
      ec->flags |= ECF_MULTI;
   if (mp->m_net_netdrv_dl_conf.mode & DL_BROAD_REQ)
      ec->flags |= ECF_BROAD;

   ec_reinit(ec);

   reply_mess.m_type = DL_CONF_REPLY;
   reply_mess.m_netdrv_net_dl_conf.stat = OK;
   memcpy(reply_mess.m_netdrv_net_dl_conf.hw_addr, ec->mac_address.ea_addr,
      sizeof(reply_mess.m_netdrv_net_dl_conf.hw_addr));

   mess_reply(mp, &reply_mess);
}


/*===========================================================================*
 *                              do_int                                       *
 *===========================================================================*/
static void do_int(ec)
ether_card_t *ec;
{
   if (ec->flags & (ECF_PACK_SEND | ECF_PACK_RECV))
      reply(ec);
}


/*===========================================================================*
 *                              conf_hw                                      *
 *===========================================================================*/
static void conf_hw(ec)
ether_card_t *ec;
{
   static eth_stat_t empty_stat = {0, 0, 0, 0, 0, 0        /* ,... */ };

   int confnr;
   ec_conf_t *ecp;

   ec->mode= EC_DISABLED;     /* Superfluous */

   /* Pick a default configuration. This hardly matters anymore. */
   confnr= MIN(ec_instance, EC_CONF_NR-1);

   ecp= &ec_conf[confnr];
   update_conf(ec, ecp);
   if (ec->mode != EC_ENABLED)
      return;

   if (!lance_probe(ec, ec_instance))
   {
      printf("%s: No ethernet card found on PCI-BIOS info.\n", 
             ec->port_name);
      ec->mode= EC_DISABLED;
      return;
   }

   /* XXX */ if (ec->ec_linmem == 0) ec->ec_linmem= 0xFFFF0000;

   ec->flags = ECF_EMPTY;
   ec->eth_stat = empty_stat;
}


/*===========================================================================*
 *                              update_conf                                  *
 *===========================================================================*/
static void update_conf(ec, ecp)
ether_card_t *ec;
ec_conf_t *ecp;
{
   long v;
   char eckey[16];
   static char ec_fmt[] = "x:d:x:x";

   /* Get the default settings and modify them from the environment. */
   strlcpy(eckey, "LANCE0", sizeof(eckey));
   eckey[5] += ec_instance;
   ec->mode= EC_SINK;
   v= ecp->ec_port;
   switch (env_parse(eckey, ec_fmt, 0, &v, 0x0000L, 0xFFFFL))
   {
   case EP_OFF:
      ec->mode= EC_DISABLED;
      break;
   case EP_ON:
   case EP_SET:
   default:
      ec->mode= EC_ENABLED;      /* Might become disabled if
                                  * all probes fail */
      break;
   }
  
   ec->ec_port= v;

   v= ecp->ec_irq | DEI_DEFAULT;
   (void) env_parse(eckey, ec_fmt, 1, &v, 0L, (long) NR_IRQ_VECTORS - 1);
   ec->ec_irq= v;
  
   v= ecp->ec_mem;
   (void) env_parse(eckey, ec_fmt, 2, &v, 0L, 0xFFFFFL);
   ec->ec_linmem= v;
  
   v= 0;
   (void) env_parse(eckey, ec_fmt, 3, &v, 0x2000L, 0x8000L);
   ec->ec_ramsize= v;
}


/*===========================================================================*
 *                              ec_init                                      *
 *===========================================================================*/
static void ec_init(ec)
ether_card_t *ec;
{
   int r;
#if VERBOSE
   int i;
#endif

   /* General initialization */
   ec->flags = ECF_EMPTY;
   lance_init_card(ec); /* Get mac_address, etc ...*/

   ec_confaddr(ec);

#if VERBOSE
   printf("%s: Ethernet address ", ec->port_name);
   for (i= 0; i < 6; i++)
      printf("%x%c", ec->mac_address.ea_addr[i],
             i < 5 ? ':' : '\n');
#endif

   /* Finish the initialization */
   ec->flags |= ECF_ENABLED;

   /* Set the interrupt handler */
   ec->ec_hook = ec->ec_irq;
   if ((r=sys_irqsetpolicy(ec->ec_irq, 0, &ec->ec_hook)) != OK)
      printf("lance: error, couldn't set IRQ policy: %d\n", r);

   return;
}


/*===========================================================================*
 *                              reply                                        *
 *===========================================================================*/
static void reply(ec)
ether_card_t *ec;
{
   message reply;
   int flags,r;

   flags = DL_NOFLAGS;
   if (ec->flags & ECF_PACK_SEND)
      flags |= DL_PACK_SEND;
   if (ec->flags & ECF_PACK_RECV)
      flags |= DL_PACK_RECV;

   reply.m_type   = DL_TASK_REPLY;
   reply.m_netdrv_net_dl_task.flags = flags;
   reply.m_netdrv_net_dl_task.count = ec->read_s;

   r = ipc_send(ec->client, &reply);
   if (r < 0)
      panic("ipc_send failed: %d", r);

   ec->read_s = 0;
   ec->flags &= ~(ECF_PACK_SEND | ECF_PACK_RECV);
}


/*===========================================================================*
 *                              mess_reply                                   *
 *===========================================================================*/
static void mess_reply(req, reply_mess)
message *req;
message *reply_mess;
{
   if (ipc_send(req->m_source, reply_mess) != OK)
      panic("unable to mess_reply");
}


/*===========================================================================*
 *                              ec_confaddr                                  *
 *===========================================================================*/
static void ec_confaddr(ec)
ether_card_t *ec;
{
   int i;
   char eakey[16];
   static char eafmt[]= "x:x:x:x:x:x";
   long v;

   /* User defined ethernet address? */
   strlcpy(eakey, "LANCE0_EA", sizeof(eakey));
   eakey[5] += ec_instance;

   for (i = 0; i < 6; i++)
   {
      v= ec->mac_address.ea_addr[i];
      if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
         break;
      ec->mac_address.ea_addr[i]= v;
   }
  
   if (i != 0 && i != 6)
   {
      /* It's all or nothing; force a panic. */
      (void) env_parse(eakey, "?", 0, &v, 0L, 0L);
   }
}


/*===========================================================================*
 *                              ec_reinit                                    *
 *===========================================================================*/
static void ec_reinit(ec)
ether_card_t *ec;
{
   int i;
   unsigned short ioaddr = ec->ec_port;

   /* stop */
   write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_STOP);

   /* purge Tx-ring */
   tx_slot_nr = cur_tx_slot_nr = 0;
   for (i=0; i<TX_RING_SIZE; i++)
   {
      lp->tx_ring[i].u.base = 0;
      isstored[i]=0;
   }

   /* re-init Rx-ring */
   rx_slot_nr = 0;
   for (i=0; i<RX_RING_SIZE; i++)
   {
      lp->rx_ring[i].buf_length = -ETH_FRAME_LEN;
      lp->rx_ring[i].u.addr[3] |= 0x80;
   }

   /* Set 'Receive Mode' */
   if (ec->flags & ECF_PROMISC)
   {
      write_csr(ioaddr, LANCE_CSR15, LANCE_CSR15_PROM);
   }
   else
   {
      if (ec->flags & (ECF_BROAD | ECF_MULTI))
      {
         write_csr(ioaddr, LANCE_CSR15, 0x0000);
      }
      else
      {
         write_csr(ioaddr, LANCE_CSR15, LANCE_CSR15_DRCVBC);
      }
   }

   /* start && enable interrupt */
   write_csr(ioaddr, LANCE_CSR0,
             LANCE_CSR0_IDON|LANCE_CSR0_IENA|LANCE_CSR0_STRT);

   return;
}

/*===========================================================================*
 *                              ec_check_ints                                *
 *===========================================================================*/
static void ec_check_ints(ec)
ether_card_t *ec;
{
   int must_restart = 0;
   int check,status;
   int isr = 0x0000;
   unsigned short ioaddr = ec->ec_port;

   if (!(ec->flags & ECF_ENABLED))
      panic("got premature interrupt");

   for (;;)
   {
#if VERBOSE
      printf("ETH: Reading ISR...");
#endif
      isr = read_csr(ioaddr, LANCE_CSR0);
      if (isr & (LANCE_CSR0_ERR|LANCE_CSR0_RINT|LANCE_CSR0_TINT)) {
         write_csr(ioaddr, LANCE_CSR0,
                   isr & ~(LANCE_CSR0_IENA|LANCE_CSR0_TDMD|LANCE_CSR0_STOP
                           |LANCE_CSR0_STRT|LANCE_CSR0_INIT) );
      }
      write_csr(ioaddr, LANCE_CSR0,
                LANCE_CSR0_BABL|LANCE_CSR0_CERR|LANCE_CSR0_MISS|LANCE_CSR0_MERR
                |LANCE_CSR0_IDON|LANCE_CSR0_IENA);

#define ISR_RST  0x0000

      if ((isr & (LANCE_CSR0_TINT|LANCE_CSR0_RINT|LANCE_CSR0_MISS
                  |LANCE_CSR0_BABL|LANCE_CSR0_ERR)) == 0x0000)
      {
#if VERBOSE
         printf("OK\n");
#endif
         break;
      }

      if (isr & LANCE_CSR0_MISS)
      {
#if VERBOSE
         printf("RX Missed Frame\n");
#endif
         ec->eth_stat.ets_recvErr++;
      }
      if ((isr & LANCE_CSR0_BABL) || (isr & LANCE_CSR0_TINT))
      {
         if (isr & LANCE_CSR0_BABL)
         {
#if VERBOSE
            printf("TX Timeout\n");
#endif
            ec->eth_stat.ets_sendErr++;
         }
         if (isr & LANCE_CSR0_TINT)
         {
#if VERBOSE
            printf("TX INT\n");
#endif
            /* status check: restart if needed. */
            status = lp->tx_ring[cur_tx_slot_nr].u.base;

            /* ??? */
            if (status & 0x40000000)
            {
               status = lp->tx_ring[cur_tx_slot_nr].misc;
               ec->eth_stat.ets_sendErr++;
               if (status & 0x0400)
                  ec->eth_stat.ets_transAb++;
               if (status & 0x0800)
                  ec->eth_stat.ets_carrSense++;
               if (status & 0x1000)
                  ec->eth_stat.ets_OWC++;
               if (status & 0x4000)
               {
                  ec->eth_stat.ets_fifoUnder++;
                  must_restart=1;
               }
            }
            else
            {
               if (status & 0x18000000)
                  ec->eth_stat.ets_collision++;
               ec->eth_stat.ets_packetT++;
            }
         }
         /* transmit a packet on the next slot if it exists. */
         check = 0;
         if (isstored[cur_tx_slot_nr]==1)
         {
            /* free the tx-slot just transmitted */
            isstored[cur_tx_slot_nr]=0;
            cur_tx_slot_nr = (cur_tx_slot_nr + 1) & TX_RING_MOD_MASK;

            /* next tx-slot is ready? */
            if (isstored[cur_tx_slot_nr]==1)
               check=1;
            else
               check=0;
         }
         else
         {
            panic("got premature TX INT..");
         }
         if (check==1)
         {
            lp->tx_ring[cur_tx_slot_nr].u.addr[3] = 0x83;
            write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_IENA|LANCE_CSR0_TDMD);
         }
         /* we set a buffered message in the slot if it exists. */
         /* and transmit it, if needed. */
         if (ec->flags & ECF_SEND_AVAIL)
            ec_send(ec);
      }
      if (isr & LANCE_CSR0_RINT)
      {
#if VERBOSE
         printf("RX INT\n");
#endif
         ec_recv(ec);
      }

      if (isr & ISR_RST)
      {
         ec->flags = ECF_STOPPED;
#if VERBOSE
         printf("ISR_RST\n");
#endif
         break;
      }

      /* ??? cf. lance driver on linux */
      if (must_restart == 1)
      {
#if VERBOSE
         printf("ETH: restarting...\n");
#endif
         /* stop */
         write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_STOP);
         /* start */
         write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_STRT);
      }
   }

   if ((ec->flags & (ECF_READING|ECF_STOPPED)) == (ECF_READING|ECF_STOPPED))
   {
#if VERBOSE
      printf("ETH: resetting...\n");
#endif
      ec_reset(ec);
   }
}

/*===========================================================================*
 *                              ec_reset                                     *
 *===========================================================================*/
static void ec_reset(ec)
ether_card_t *ec;
{
   /* Stop/start the chip, and clear all RX,TX-slots */
   unsigned short ioaddr = ec->ec_port;
   int i;

   /* stop */
   write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_STOP);
   /* start */
   write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_STRT);

   /* purge Tx-ring */
   tx_slot_nr = cur_tx_slot_nr = 0;
   for (i=0; i<TX_RING_SIZE; i++)
   {
      lp->tx_ring[i].u.base = 0;
      isstored[i]=0;
   }

   /* re-init Rx-ring */
   rx_slot_nr = 0;
   for (i=0; i<RX_RING_SIZE; i++)
   {
      lp->rx_ring[i].buf_length = -ETH_FRAME_LEN;
      lp->rx_ring[i].u.addr[3] |= 0x80;
   }

   /* store a buffered message on the slot if exists */
   ec_send(ec);
   ec->flags &= ~ECF_STOPPED;
}

/*===========================================================================*
 *                              ec_send                                      *
 *===========================================================================*/
static void ec_send(ec)
ether_card_t *ec;
{
   /* from ec_check_ints() or ec_reset(). */
   /* this function proccesses the buffered message. (slot/transmit) */
   if (!(ec->flags & ECF_SEND_AVAIL))
      return;
  
   ec->flags &= ~ECF_SEND_AVAIL;
   switch(ec->sendmsg.m_type)
   {
   case DL_WRITEV_S: do_vwrite_s(&ec->sendmsg, TRUE);        break;
   default:
      panic("wrong type: %d", ec->sendmsg.m_type);
      break;
   }
}

/*===========================================================================*
 *                              do_vread_s                                   *
 *===========================================================================*/
static void do_vread_s(const message *mp)
{
   int count, r;
   ether_card_t *ec;

   ec= &ec_state;

   ec->client= mp->m_source;
   count = mp->m_net_netdrv_dl_readv_s.count;

   r = sys_safecopyfrom(mp->m_source, mp->m_net_netdrv_dl_readv_s.grant, 0,
                        (vir_bytes)ec->read_iovec.iod_iovec,
                        (count > IOVEC_NR ? IOVEC_NR : count) *
                        sizeof(iovec_s_t));
   if (r != OK)
	panic("do_vread_s: sys_safecopyfrom failed: %d", r);
   ec->read_iovec.iod_iovec_s    = count;
   ec->read_iovec.iod_proc_nr    = mp->m_source;
   ec->read_iovec.iod_grant = mp->m_net_netdrv_dl_readv_s.grant;
   ec->read_iovec.iod_iovec_offset = 0;

   ec->tmp_iovec = ec->read_iovec;

   ec->flags |= ECF_READING;

   ec_recv(ec);

   if ((ec->flags & (ECF_READING|ECF_STOPPED)) == (ECF_READING|ECF_STOPPED))
      ec_reset(ec);
   reply(ec);
}

/*===========================================================================*
 *                              ec_recv                                      *
 *===========================================================================*/
static void ec_recv(ec)
ether_card_t *ec;
{
   vir_bytes length;
   int packet_processed;
   int status;
   unsigned short ioaddr = ec->ec_port;

   if ((ec->flags & ECF_READING)==0)
      return;
   if (!(ec->flags & ECF_ENABLED))
      return;

   /* we check all the received slots until find a properly received packet */
   packet_processed = FALSE;
   while (!packet_processed)
   {
      status = lp->rx_ring[rx_slot_nr].u.base >> 24;
      if ( (status & 0x80) == 0x00 )
      {
         status = lp->rx_ring[rx_slot_nr].u.base >> 24;

         /* ??? */
         if (status != 0x03)
         {
            if (status & 0x01)
               ec->eth_stat.ets_recvErr++;
            if (status & 0x04)
               ec->eth_stat.ets_fifoOver++;
            if (status & 0x08)
               ec->eth_stat.ets_CRCerr++;
            if (status & 0x10)
               ec->eth_stat.ets_OVW++;
            if (status & 0x20)
               ec->eth_stat.ets_frameAll++;
            length = 0;
         }
         else
         {
            ec->eth_stat.ets_packetR++;
            length = lp->rx_ring[rx_slot_nr].msg_length;
         }
         if (length > 0)
         {
            ec_nic2user(ec, (int)(lp->rbuf[rx_slot_nr]),
                        &ec->read_iovec, 0, length);
              
            ec->read_s = length;
            ec->flags |= ECF_PACK_RECV;
            ec->flags &= ~ECF_READING;
            packet_processed = TRUE;
         }
         /* set up this slot again, and we move to the next slot */
         lp->rx_ring[rx_slot_nr].buf_length = -ETH_FRAME_LEN;
         lp->rx_ring[rx_slot_nr].u.addr[3] |= 0x80;

         write_csr(ioaddr, LANCE_CSR0,
                   LANCE_CSR0_BABL|LANCE_CSR0_CERR|LANCE_CSR0_MISS
                   |LANCE_CSR0_MERR|LANCE_CSR0_IDON|LANCE_CSR0_IENA);

         rx_slot_nr = (rx_slot_nr + 1) & RX_RING_MOD_MASK;
      }
      else
         break;
   }
}

/*===========================================================================*
 *                              do_vwrite_s                                  *
 *===========================================================================*/
static void do_vwrite_s(mp, from_int)
message *mp;
int from_int;
{
   int count, check, r;
   ether_card_t *ec;
   unsigned short ioaddr;

   ec = &ec_state;

   ec->client= mp->m_source;
   count = mp->m_net_netdrv_dl_writev_s.count;

   if (isstored[tx_slot_nr]==1)
   {
      /* all slots are used, so this message is buffered */
      ec->sendmsg= *mp;
      ec->flags |= ECF_SEND_AVAIL;
      reply(ec);
      return;
   }

   /* convert the message to write_iovec */
   r = sys_safecopyfrom(mp->m_source, mp->m_net_netdrv_dl_writev_s.grant, 0,
                        (vir_bytes)ec->write_iovec.iod_iovec,
                        (count > IOVEC_NR ? IOVEC_NR : count) *
                        sizeof(iovec_s_t));
   if (r != OK)
	panic("do_vwrite_s: sys_safecopyfrom failed: %d", r);
   ec->write_iovec.iod_iovec_s    = count;
   ec->write_iovec.iod_proc_nr    = mp->m_source;
   ec->write_iovec.iod_grant      = mp->m_net_netdrv_dl_writev_s.grant;
   ec->write_iovec.iod_iovec_offset = 0;

   ec->tmp_iovec = ec->write_iovec;
   ec->write_s = calc_iovec_size(&ec->tmp_iovec);

   /* copy write_iovec to the slot on DMA address */
   ec_user2nic(ec, &ec->write_iovec, 0,
               (int)(lp->tbuf[tx_slot_nr]), ec->write_s);
   /* set-up for transmitting, and transmit it if needed. */
   lp->tx_ring[tx_slot_nr].buf_length = -ec->write_s;
   lp->tx_ring[tx_slot_nr].misc = 0x0;
   lp->tx_ring[tx_slot_nr].u.base
      = virt_to_bus(lp->tbuf[tx_slot_nr]) & 0xffffff;
   isstored[tx_slot_nr]=1;
   if (cur_tx_slot_nr == tx_slot_nr)
      check=1;
   else
      check=0;
   tx_slot_nr = (tx_slot_nr + 1) & TX_RING_MOD_MASK;

   if (check == 1)
   {
      ioaddr = ec->ec_port;
      lp->tx_ring[cur_tx_slot_nr].u.addr[3] = 0x83;
      write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_IENA|LANCE_CSR0_TDMD);
   }
        
   ec->flags |= ECF_PACK_SEND;

   /* reply by calling do_int() if this function is called from interrupt. */
   if (from_int)
      return;
   reply(ec);
}


/*===========================================================================*
 *                              ec_user2nic                                  *
 *===========================================================================*/
static void ec_user2nic(ec, iovp, offset, nic_addr, count)
ether_card_t *ec;
iovec_dat_t *iovp;
vir_bytes offset;
int nic_addr;
vir_bytes count;
{
   int bytes, i, r;

   i= 0;
   while (count > 0)
   {
      if (i >= IOVEC_NR)
      {
         ec_next_iovec(iovp);
         i= 0;
         continue;
      }
      if (offset >= iovp->iod_iovec[i].iov_size)
      {
         offset -= iovp->iod_iovec[i].iov_size;
         i++;
         continue;
      }
      bytes = iovp->iod_iovec[i].iov_size - offset;
      if (bytes > count)
         bytes = count;
      
      if ( (r=sys_safecopyfrom(iovp->iod_proc_nr,
                               iovp->iod_iovec[i].iov_grant, offset,
                               nic_addr, bytes )) != OK )
         panic("ec_user2nic: sys_safecopyfrom failed: %d", r);

      count -= bytes;
      nic_addr += bytes;
      offset += bytes;
   }
}

/*===========================================================================*
 *                              ec_nic2user                                  *
 *===========================================================================*/
static void ec_nic2user(ec, nic_addr, iovp, offset, count)
ether_card_t *ec;
int nic_addr;
iovec_dat_t *iovp;
vir_bytes offset;
vir_bytes count;
{
   int bytes, i, r;

   i= 0;
   while (count > 0)
   {
      if (i >= IOVEC_NR)
      {
         ec_next_iovec(iovp);
         i= 0;
         continue;
      }
      if (offset >= iovp->iod_iovec[i].iov_size)
      {
         offset -= iovp->iod_iovec[i].iov_size;
         i++;
         continue;
      }
      bytes = iovp->iod_iovec[i].iov_size - offset;
      if (bytes > count)
         bytes = count;
      if ( (r=sys_safecopyto( iovp->iod_proc_nr, iovp->iod_iovec[i].iov_grant,
                              offset, nic_addr, bytes )) != OK )
         panic("ec_nic2user: sys_safecopyto failed: %d", r);
      
      count -= bytes;
      nic_addr += bytes;
      offset += bytes;
   }
}


/*===========================================================================*
 *                              calc_iovec_size                              *
 *===========================================================================*/
static int calc_iovec_size(iovec_dat_t *iovp)
{
   int size,i;

   size = i = 0;
        
   while (i < iovp->iod_iovec_s)
   {
      if (i >= IOVEC_NR)
      {
         ec_next_iovec(iovp);
         i= 0;
         continue;
      }
      size += iovp->iod_iovec[i].iov_size;
      i++;
   }

   return size;
}

/*===========================================================================*
 *                              ec_next_iovec                                *
 *===========================================================================*/
static void ec_next_iovec(iovp)
iovec_dat_t *iovp;
{
   int r;

   iovp->iod_iovec_s -= IOVEC_NR;
   iovp->iod_iovec_offset += IOVEC_NR * sizeof(iovec_s_t);

   r = sys_safecopyfrom(iovp->iod_proc_nr, iovp->iod_grant,
                        iovp->iod_iovec_offset,
                        (vir_bytes)iovp->iod_iovec,
                        (iovp->iod_iovec_s > IOVEC_NR ?
                         IOVEC_NR : iovp->iod_iovec_s) *
                        sizeof(iovec_s_t));
   if (r != OK)
	panic("ec_next_iovec: sys_safecopyfrom failed: %d", r);
}


/*===========================================================================*
 *                              do_getstat_s                                 *
 *===========================================================================*/
static void do_getstat_s(mp)
message *mp;
{
   int r;
   ether_card_t *ec;

   ec= &ec_state;

   r = sys_safecopyto(mp->m_source, mp->m_net_netdrv_dl_getstat_s.grant, 0,
                      (vir_bytes)&ec->eth_stat, sizeof(ec->eth_stat));

   if (r != OK)
	panic("do_getstat_s: sys_safecopyto failed: %d", r);

   mp->m_type= DL_STAT_REPLY;
   r= ipc_send(mp->m_source, mp);
   if (r != OK)
      panic("do_getstat_s: send failed: %d", r);
}

/*===========================================================================*
 *                              lance_stop                                   *
 *===========================================================================*/
static void lance_stop(ec)
ether_card_t *ec;
{
   unsigned short ioaddr;

   if (!(ec->flags & ECF_ENABLED))
      return;

   ioaddr = ec->ec_port;
  
   /* stop */
   write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_STOP);

   /* Reset */
   in_word(ioaddr+LANCE_RESET);

   ec->flags = ECF_EMPTY;
}

/*===========================================================================*
 *                              getAddressing                                *
 *===========================================================================*/
static void getAddressing(devind, ec)
int devind;
ether_card_t *ec;
{
   unsigned int      membase, ioaddr;
   int reg, irq;

   for (reg = PCI_BAR; reg <= PCI_BAR_6; reg += 4)
   {
      ioaddr = pci_attr_r32(devind, reg);

      if ((ioaddr & PCI_BAR_IO_MASK) == 0 || (ioaddr & PCI_BAR_IO) == 0)
         continue;
      /* Strip the I/O address out of the returned value */
      ioaddr &= PCI_BAR_IO_MASK;
      /* Get the memory base address */
      membase = pci_attr_r32(devind, PCI_BAR_2);
      /* KK: Get the IRQ number */
      irq = pci_attr_r8(devind, PCI_IPR);
      if (irq)
         irq = pci_attr_r8(devind, PCI_ILR);

      ec->ec_linmem = membase;
      ec->ec_port = ioaddr;
      ec->ec_irq = irq;
   }
}

/*===========================================================================*
 *                              lance_probe                                  *
 *===========================================================================*/
static int lance_probe(ec, skip)
ether_card_t *ec;
int skip;
{
   unsigned short    pci_cmd;
   unsigned short    ioaddr;
   int               lance_version, chip_version;
   int devind, r;
   u16_t vid, did;

   r= pci_first_dev(&devind, &vid, &did);
   if (r == 0)
      return 0;

   while (skip--)
   {
      r= pci_next_dev(&devind, &vid, &did);
      if (!r)
         return 0;
   }

   pci_reserve(devind);

   getAddressing(devind, ec);


   /* ===== Bus Master ? ===== */
   pci_cmd = pci_attr_r32(devind, PCI_CR);
   if (!(pci_cmd & PCI_CR_MAST_EN)) {
      pci_cmd |= PCI_CR_MAST_EN;
      pci_attr_w32(devind, PCI_CR, pci_cmd);
   }

   /* ===== Probe Details ===== */
   ioaddr = ec->ec_port;

   /* Reset */
   in_word(ioaddr+LANCE_RESET);

   if (read_csr(ioaddr, LANCE_CSR0) != LANCE_CSR0_STOP)
   {
      ec->mode=EC_DISABLED;
   }

   /* Probe Chip Version */
   out_word(ioaddr+LANCE_ADDR, 88);     /* Get the version of the chip */
   if (in_word(ioaddr+LANCE_ADDR) != 88)
      lance_version = 0;
   else
   {
      chip_version = read_csr(ioaddr, LANCE_CSR88);
      chip_version |= read_csr(ioaddr, LANCE_CSR89) << 16;

      if ((chip_version & 0xfff) != 0x3)
      {
         ec->mode=EC_DISABLED;
      }
      chip_version = (chip_version >> 12) & 0xffff;
      for (lance_version = 1; chip_table[lance_version].id_number != 0;
           ++lance_version)
         if (chip_table[lance_version].id_number == chip_version)
            break;
   }

#if VERBOSE
   printf("%s: %s at %X:%d\n",
          ec->port_name, chip_table[lance_version].name,
          ec->ec_port, ec->ec_irq);
#endif

   return lance_version;
}

/*===========================================================================*
 *                              lance_init_card                              *
 *===========================================================================*/
static void lance_init_card(ec)
ether_card_t *ec;
{
   int i;
   Address l = (vir_bytes)lance_buf;
   unsigned short ioaddr = ec->ec_port;

   /* ============= setup init_block(cf. lance_probe1) ================ */
   /* make sure data structure is 8-byte aligned and below 16MB (for DMA) */

   lp = (struct lance_interface *)l;

   /* disable Tx and Rx */
   lp->init_block.mode = LANCE_CSR15_DTX|LANCE_CSR15_DRX;
   lp->init_block.filter[0] = lp->init_block.filter[1] = 0x0;
   /* using multiple Rx/Tx buffer */
   lp->init_block.rx_ring
      = (virt_to_bus(&lp->rx_ring) & 0xffffff) | RX_RING_LEN_BITS;
   lp->init_block.tx_ring
      = (virt_to_bus(&lp->tx_ring) & 0xffffff) | TX_RING_LEN_BITS;

   l = virt_to_bus(&lp->init_block);
   write_csr(ioaddr, LANCE_CSR1, (unsigned short)l);
   write_csr(ioaddr, LANCE_CSR2, (unsigned short)(l >> 16));
   write_csr(ioaddr, LANCE_CSR4,
             LANCE_CSR4_APAD_XMT|LANCE_CSR4_MFCOM|LANCE_CSR4_RCVCCOM
             |LANCE_CSR4_TXSTRTM|LANCE_CSR4_JABM);

   /* ============= Get MAC address (cf. lance_probe1) ================ */
   for (i = 0; i < 6; ++i)
      ec->mac_address.ea_addr[i]=in_byte(ioaddr+LANCE_ETH_ADDR+i);

   /* ============ (re)start init_block(cf. lance_reset) =============== */
   /* Reset the LANCE */
   (void)in_word(ioaddr+LANCE_RESET);

   /* ----- Re-initialize the LANCE ----- */
   /* Set station address */
   for (i = 0; i < 6; ++i)
      lp->init_block.phys_addr[i] = ec->mac_address.ea_addr[i];
   /* Preset the receive ring headers */
   for (i=0; i<RX_RING_SIZE; i++)
   {
      lp->rx_ring[i].buf_length = -ETH_FRAME_LEN;
      /* OWN */
      lp->rx_ring[i].u.base = virt_to_bus(lp->rbuf[i]) & 0xffffff;
      /* we set the top byte as the very last thing */
      lp->rx_ring[i].u.addr[3] = 0x80;
   }
   /* Preset the transmitting ring headers */
   for (i=0; i<TX_RING_SIZE; i++)
   {
      lp->tx_ring[i].u.base = 0;
      isstored[i] = 0;
   }
   /* enable Rx and Tx */
   lp->init_block.mode = 0x0;

   l = (Address)virt_to_bus(&lp->init_block);
   write_csr(ioaddr, LANCE_CSR1, (short)l);
   write_csr(ioaddr, LANCE_CSR2, (short)(l >> 16));
   write_csr(ioaddr, LANCE_CSR4,
             LANCE_CSR4_APAD_XMT|LANCE_CSR4_MFCOM|LANCE_CSR4_RCVCCOM
             |LANCE_CSR4_TXSTRTM|LANCE_CSR4_JABM);

   /* ----- start when init done. ----- */
   /* stop */
   write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_STOP);
   /* init */
   write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_INIT);
   /* poll for IDON */
   for (i = 10000; i > 0; --i)
      if (read_csr(ioaddr, LANCE_CSR0) & LANCE_CSR0_IDON)
         break;

   /* Set 'Multicast Table' */
   for (i=0;i<4;++i)
   {
      write_csr(ioaddr, LANCE_CSR8 + i, 0xffff);
   }

   /* Set 'Receive Mode' */
   if (ec->flags & ECF_PROMISC)
   {
      write_csr(ioaddr, LANCE_CSR15, LANCE_CSR15_PROM);
   }
   else
   {
      if (ec->flags & (ECF_BROAD | ECF_MULTI))
      {
         write_csr(ioaddr, LANCE_CSR15, 0x0000);
      }
      else
      {
         write_csr(ioaddr, LANCE_CSR15, LANCE_CSR15_DRCVBC);
      }
   }

   /* start && enable interrupt */
   write_csr(ioaddr, LANCE_CSR0,
             LANCE_CSR0_IDON|LANCE_CSR0_IENA|LANCE_CSR0_STRT);

   return;
}

/*===========================================================================*
 *                              in_byte                                      *
 *===========================================================================*/
static u8_t in_byte(port_t port)
{
	int r;
	u32_t value;

	r= sys_inb(port, &value);
	if (r != OK)
		panic("sys_inb failed: %d", r);
	return value;
}

/*===========================================================================*
 *                              in_word                                      *
 *===========================================================================*/
static u16_t in_word(port_t port)
{
	int r;
	u32_t value;

	r= sys_inw(port, &value);
	if (r != OK)
		panic("sys_inw failed: %d", r);
	return value;
}


/*===========================================================================*
 *                              out_word                                     *
 *===========================================================================*/
static void out_word(port_t port, u16_t value)
{
	int r;

	r= sys_outw(port, value);
	if (r != OK)
		panic("sys_outw failed: %d", r);
}

/*===========================================================================*
 *                              read_csr                                     *
 *===========================================================================*/
static u16_t read_csr(port_t ioaddr, u16_t csrno)
{
   out_word(ioaddr+LANCE_ADDR, csrno);
   return in_word(ioaddr+LANCE_DATA);
}

/*===========================================================================*
 *                              write_csr                                    *
 *===========================================================================*/
static void write_csr(port_t ioaddr, u16_t csrno, u16_t value)
{
   out_word(ioaddr+LANCE_ADDR, csrno);
   out_word(ioaddr+LANCE_DATA, value);
}
