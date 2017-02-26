/*
 * lance.c
 *
 * This file contains a ethernet device driver for AMD LANCE based ethernet
 * cards.
 *
 * Created: Jul 27, 2002 by Kazuya Kodama <kazuya@nii.ac.jp>
 * Adapted for Minix 3: Sep 05, 2005 by Joren l'Ami <jwlami@cs.vu.nl>
 */

#define VERBOSE 0 /* Verbose debugging output */
#define LANCE_FKEY 0 /* Use function key to dump Lance stats */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <assert.h>

#include <minix/syslib.h>
#include <minix/endpoint.h>
#include <machine/pci.h>
#include <minix/ds.h>

#include "lance.h"

static int do_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks);
static void ec_confaddr(netdriver_addr_t *addr, unsigned int instance);
static void ec_reinit(ether_card_t *ec);
static void do_intr(unsigned int mask);
static void do_set_mode(unsigned int mode, const netdriver_addr_t *mcast_list,
	unsigned int mcast_count);
static int do_send(struct netdriver_data *data, size_t size);
static ssize_t do_recv(struct netdriver_data *data, size_t max);
static void do_stop(void);
static void lance_dump(void);
static void do_other(const message *m_ptr, int ipc_status);
static void get_addressing(int devind, ether_card_t *ec);
static int lance_probe(ether_card_t *ec, unsigned int skip);
static void lance_init_hw(ether_card_t *ec, netdriver_addr_t *addr,
	unsigned int instance);

/* Accesses Lance Control and Status Registers */
static u8_t in_byte(port_t port);
static u16_t in_word(port_t port);
static void out_word(port_t port, u16_t value);
static u16_t read_csr(port_t ioaddr, u16_t csrno);
static void write_csr(port_t ioaddr, u16_t csrno, u16_t value);

static ether_card_t ec_state;

/* --- LANCE --- */
/* General */
typedef uint32_t Address;

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
static phys_bytes tx_ring_base[TX_RING_SIZE]; /* Tx-slot physical address */
static char isstored[TX_RING_SIZE]; /* Tx-slot in-use */

static const struct netdriver lance_table = {
	.ndr_name	= "le",
	.ndr_init	= do_init,
	.ndr_stop	= do_stop,
	.ndr_set_mode	= do_set_mode,
	.ndr_recv	= do_recv,
	.ndr_send	= do_send,
	.ndr_intr	= do_intr,
	.ndr_other	= do_other,
};

/*===========================================================================*
 *                              main                                         *
 *===========================================================================*/
int main(int argc, char **argv)
{

   env_setargs(argc, argv);

   netdriver_task(&lance_table);

   return 0;
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

   printf("lance driver %s:\n", netdriver_name());

   ioaddr = ec->ec_port;
   isr = read_csr(ioaddr, LANCE_CSR0);
   printf("isr = 0x%x, mode = 0x%x\n", isr, ec->ec_mode);

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
 *                              do_other                                     *
 *===========================================================================*/
static void do_other(const message *m_ptr, int ipc_status)
{

   if (is_ipc_notify(ipc_status) && m_ptr->m_source == TTY_PROC_NR)
      lance_dump();
}

/*===========================================================================*
 *                              ec_confaddr                                  *
 *===========================================================================*/
static void ec_confaddr(netdriver_addr_t *addr, unsigned int instance)
{
   int i;
   char eakey[16];
   static char eafmt[]= "x:x:x:x:x:x";
   long v;

   /* User defined ethernet address? */
   strlcpy(eakey, "LANCE0_EA", sizeof(eakey));
   eakey[5] += instance;

   for (i = 0; i < 6; i++)
   {
      v= addr->na_addr[i];
      if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
         break;
      addr->na_addr[i]= v;
   }

   if (i != 0 && i != 6)
   {
      /* It's all or nothing; force a panic. */
      panic("invalid ethernet address supplied");
   }
}

/*===========================================================================*
 *		                do_init                                      *
 *===========================================================================*/
static int do_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks __unused)
{
/* Initialize the lance driver. */
   ether_card_t *ec;
#if VERBOSE
   int i;
#endif
#if LANCE_FKEY
   int r, fkeys, sfkeys;
#endif

#if LANCE_FKEY
   fkeys = sfkeys = 0;
   bit_set( sfkeys, 7 );
   if ( (r = fkey_map(&fkeys, &sfkeys)) != OK )
      printf("Warning: lance couldn't observe Shift+F7 key: %d\n",r);
#endif

   /* Initialize the driver state. */
   ec= &ec_state;
   memset(ec, 0, sizeof(*ec));

   /* See if there is a matching card. */
   if (!lance_probe(ec, instance))
      return ENXIO;

   /* Initialize the hardware. */
   lance_init_hw(ec, addr, instance);

#if VERBOSE
   printf("%s: Ethernet address ", netdriver_name());
   for (i= 0; i < 6; i++)
      printf("%x%c", addr->na_addr[i], i < 5 ? ':' : '\n');
#endif

   *caps = NDEV_CAP_MCAST | NDEV_CAP_BCAST;
   return OK;
}

/*===========================================================================*
 *                              ec_reinit                                    *
 *===========================================================================*/
static void ec_reinit(ether_card_t *ec)
{
   spin_t spin;
   int i;
   unsigned short ioaddr = ec->ec_port;

   /* stop */
   write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_STOP);
   /* init */
   write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_INIT);
   /* poll for IDON */
   SPIN_FOR(&spin, 1000) {
      if (read_csr(ioaddr, LANCE_CSR0) & LANCE_CSR0_IDON)
         break;
   }

   /* Set 'Multicast Table' */
   for (i=0;i<4;++i)
   {
      write_csr(ioaddr, LANCE_CSR8 + i, 0xffff);
   }

   /* Set 'Receive Mode' */
   if (ec->ec_mode & NDEV_MODE_PROMISC)
   {
      write_csr(ioaddr, LANCE_CSR15, LANCE_CSR15_PROM);
   }
   else
   {
      if (ec->ec_mode &
          (NDEV_MODE_BCAST | NDEV_MODE_MCAST_LIST | NDEV_MODE_MCAST_ALL))
      {
         write_csr(ioaddr, LANCE_CSR15, 0x0000);
      }
      else
      {
         write_csr(ioaddr, LANCE_CSR15, LANCE_CSR15_DRCVBC);
      }
   }

   /* purge Tx-ring */
   tx_slot_nr = cur_tx_slot_nr = 0;
   for (i=0; i<TX_RING_SIZE; i++)
   {
      lp->tx_ring[i].u.base = 0;
      isstored[i]=0;
   }
   cur_tx_slot_nr = tx_slot_nr;

   /* re-init Rx-ring */
   rx_slot_nr = 0;
   for (i=0; i<RX_RING_SIZE; i++)
   {
      lp->rx_ring[i].buf_length = -ETH_FRAME_LEN;
      lp->rx_ring[i].u.addr[3] |= 0x80;
   }

   /* start && enable interrupt */
   write_csr(ioaddr, LANCE_CSR0,
             LANCE_CSR0_IDON|LANCE_CSR0_IENA|LANCE_CSR0_STRT);

   return;
}

/*===========================================================================*
 *                              do_set_mode                                    *
 *===========================================================================*/
static void do_set_mode(unsigned int mode,
	const netdriver_addr_t *mcast_list __unused,
	unsigned int mcast_count __unused)
{
   ether_card_t *ec;

   ec = &ec_state;

   ec->ec_mode = mode;

   ec_reinit(ec);
}

/*===========================================================================*
 *                              do_intr                                      *
 *===========================================================================*/
static void do_intr(unsigned int __unused mask)
{
   ether_card_t *ec;
   int must_restart = 0;
   int r, check, status;
   int isr = 0x0000;
   unsigned short ioaddr;

   ec = &ec_state;
   ioaddr = ec->ec_port;

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
         netdriver_stat_ierror(1);
      }
      if ((isr & LANCE_CSR0_BABL) || (isr & LANCE_CSR0_TINT))
      {
         if (isr & LANCE_CSR0_BABL)
         {
#if VERBOSE
            printf("TX Timeout\n");
#endif
            netdriver_stat_oerror(1);
         }
         if (isr & LANCE_CSR0_TINT)
         {
#if VERBOSE
            printf("TX INT\n");
#endif
            /* status check: restart if needed. */
            status = lp->tx_ring[cur_tx_slot_nr].u.base;

            /* did an error (UFLO, LCOL, LCAR, RTRY) occur? */
            if (status & 0x40000000)
            {
               status = lp->tx_ring[cur_tx_slot_nr].misc;
               netdriver_stat_oerror(1);
               if (status & 0x4000) /* UFLO */
               {
                  must_restart=1;
               }
            }
            else
            {
               if (status & 0x18000000)
                  netdriver_stat_coll(1);
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
         if (!must_restart)
            netdriver_send();
      }
      if (isr & LANCE_CSR0_RINT)
      {
#if VERBOSE
         printf("RX INT\n");
#endif
         netdriver_recv();
      }

      if (must_restart == 1)
      {
#if VERBOSE
         printf("ETH: restarting...\n");
#endif

         ec_reinit(ec);

	 /* store a buffered message on the slot if it exists */
	 netdriver_send();
      }
   }

   /* reenable interrupts */
   if ((r = sys_irqenable(&ec->ec_hook)) != OK)
      panic("couldn't enable interrupt: %d", r);
}

/*===========================================================================*
 *                              do_recv                                      *
 *===========================================================================*/
static ssize_t do_recv(struct netdriver_data *data, size_t max)
{
   ether_card_t *ec;
   vir_bytes length;
   int packet_processed;
   int status;
   unsigned short ioaddr;

   ec = &ec_state;
   ioaddr = ec->ec_port;

   /* we check all the received slots until find a properly received packet */
   packet_processed = FALSE;
   while (!packet_processed)
   {
      status = lp->rx_ring[rx_slot_nr].u.base >> 24;

      /* is the slot marked as ready? */
      if ( (status & 0x80) != 0x00 )
         return SUSPEND; /* no */

      /* did an error occur? */
      if (status != 0x03)
      {
         if (status & 0x01)
            netdriver_stat_ierror(1);
         length = 0;
      }
      else
      {
         length = lp->rx_ring[rx_slot_nr].msg_length;
      }

      /* do we now have a valid packet? */
      if (length > 0)
      {
	 if (length > max)
	    length = max;
         netdriver_copyout(data, 0, lp->rbuf[rx_slot_nr], length);
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

   /* return the length of the packet we have received */
   return length;
}

/*===========================================================================*
 *                              do_send                                      *
 *===========================================================================*/
static int do_send(struct netdriver_data *data, size_t size)
{
   int check;
   ether_card_t *ec;
   unsigned short ioaddr;

   ec = &ec_state;

   /* if all slots are used, this request must be deferred */
   if (isstored[tx_slot_nr]==1)
      return SUSPEND;

   /* copy the packet to the slot on DMA address */
   netdriver_copyin(data, 0, lp->tbuf[tx_slot_nr], size);

   /* set-up for transmitting, and transmit it if needed. */
   lp->tx_ring[tx_slot_nr].buf_length = -size;
   lp->tx_ring[tx_slot_nr].misc = 0x0;
   lp->tx_ring[tx_slot_nr].u.base = tx_ring_base[tx_slot_nr];
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

   return OK;
}

/*===========================================================================*
 *                              do_stop                                      *
 *===========================================================================*/
static void do_stop(void)
{
   ether_card_t *ec;
   unsigned short ioaddr;

   ec = &ec_state;

   ioaddr = ec->ec_port;

   /* stop */
   write_csr(ioaddr, LANCE_CSR0, LANCE_CSR0_STOP);

   /* Reset */
   in_word(ioaddr+LANCE_RESET);
}

/*===========================================================================*
 *                              get_addressing                               *
 *===========================================================================*/
static void get_addressing(int devind, ether_card_t *ec)
{
   unsigned int ioaddr;
   int reg, irq;

   for (reg = PCI_BAR; reg <= PCI_BAR_6; reg += 4)
   {
      ioaddr = pci_attr_r32(devind, reg);

      if ((ioaddr & PCI_BAR_IO_MASK) == 0 || (ioaddr & PCI_BAR_IO) == 0)
         continue;
      /* Strip the I/O address out of the returned value */
      ioaddr &= PCI_BAR_IO_MASK;
      ec->ec_port = ioaddr;
   }

   /* KK: Get the IRQ number */
   irq = pci_attr_r8(devind, PCI_IPR);
   if (irq)
      irq = pci_attr_r8(devind, PCI_ILR);
   ec->ec_irq = irq;
}

/*===========================================================================*
 *                              lance_probe                                  *
 *===========================================================================*/
static int lance_probe(ether_card_t *ec, unsigned int skip)
{
   unsigned short    pci_cmd;
   unsigned short    ioaddr;
   int               lance_version, chip_version;
   int devind, r;
   u16_t vid, did;

   pci_init();

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

   get_addressing(devind, ec);

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
      return 0;
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
	 return 0;
      }
      chip_version = (chip_version >> 12) & 0xffff;
      for (lance_version = 1; chip_table[lance_version].id_number != 0;
           ++lance_version)
         if (chip_table[lance_version].id_number == chip_version)
            break;
   }

#if VERBOSE
   printf("%s: %s at %X:%d\n",
          netdriver_name(), chip_table[lance_version].name,
          ec->ec_port, ec->ec_irq);
#endif

   return lance_version;
}

/*===========================================================================*
 *                              virt_to_bus                                  *
 *===========================================================================*/
static phys_bytes virt_to_bus(void *ptr)
{
   phys_bytes value;
   int r;

   if ((r = sys_umap(SELF, VM_D, (vir_bytes)ptr, 4, &value)) != OK)
      panic("sys_umap failed: %d", r);

   return value;
}

/*===========================================================================*
 *                              lance_init_hw                                *
 *===========================================================================*/
static void lance_init_hw(ether_card_t *ec, netdriver_addr_t *addr,
	unsigned int instance)
{
   phys_bytes lance_buf_phys;
   int i, r;
   Address l;
   unsigned short ioaddr = ec->ec_port;

   /* ============= setup init_block(cf. lance_probe1) ================ */
   /* make sure data structure is 8-byte aligned and below 16MB (for DMA) */

   /* Allocate memory */
   if ((lance_buf = alloc_contig(LANCE_BUF_SIZE, AC_ALIGN4K|AC_LOWER16M,
     &lance_buf_phys)) == NULL)
      panic("alloc_contig failed: %d", LANCE_BUF_SIZE);

   l = (vir_bytes)lance_buf;
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
      addr->na_addr[i]=in_byte(ioaddr+LANCE_ETH_ADDR+i);

   /* Allow the user to override the hardware address. */
   ec_confaddr(addr, instance);

   /* ============ (re)start init_block(cf. lance_reset) =============== */
   /* Reset the LANCE */
   (void)in_word(ioaddr+LANCE_RESET);

   /* ----- Re-initialize the LANCE ----- */
   /* Set station address */
   for (i = 0; i < 6; ++i)
      lp->init_block.phys_addr[i] = addr->na_addr[i];
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
      tx_ring_base[i] = virt_to_bus(lp->tbuf[i]) & 0xffffff;
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
   ec_reinit(ec);

   /* Set the interrupt handler */
   ec->ec_hook = ec->ec_irq;
   if ((r=sys_irqsetpolicy(ec->ec_irq, 0, &ec->ec_hook)) != OK)
      panic("couldn't set IRQ policy: %d", r);
   if ((r = sys_irqenable(&ec->ec_hook)) != OK)
      panic("couldn't enable interrupt: %d", r);

   /* start && enable interrupt */
   write_csr(ioaddr, LANCE_CSR0,
             LANCE_CSR0_IDON|LANCE_CSR0_IENA|LANCE_CSR0_STRT);
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
