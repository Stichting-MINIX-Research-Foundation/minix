/* $NetBSD: isp_tpublic.h,v 1.19 2010/01/03 02:47:09 mjacob Exp $ */
/*-
 *  Copyright (c) 1997-2008 by Matthew Jacob
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 *  THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */
/*
 * Host Adapter Public Target Interface Structures && Routines
 */
/*
 * A note about terminology:
 *
 *  "Inner Layer" means this driver (isp and the isp_tpublic API).
 *
 *    This module includes the both generic and platform specific pieces.
 *
 *  "Outer Layer" means another (external) module.
 *
 *    This is an additional module that actually implements SCSI target command
 *    decode and is the recipient of incoming commands and the source of the
 *    disposition for them.
 */

#ifndef    _ISP_TPUBLIC_H
#define    _ISP_TPUBLIC_H    1
/*
 * Include general target definitions
 */
#include "isp_target.h"

/*
 * Action codes set by the Inner Layer for the outer layer to figure out what to do with.
 */
typedef enum {
    QOUT_HBA_REG=0,     /* the argument is a pointer to a hba_register_t */
    QOUT_ENABLE,        /* the argument is a pointer to a enadis_t */
    QOUT_DISABLE,       /* the argument is a pointer to a enadis_t */
    QOUT_TMD_START,     /* the argument is a pointer to a tmd_cmd_t */
    QOUT_TMD_DONE,      /* the argument is a pointer to a tmd_xact_t */
    QOUT_NOTIFY,        /* the argument is a pointer to a notify_t */
    QOUT_HBA_UNREG      /* the argument is a pointer to a hba_register_t */
} tact_e;

/*
 * Action codes set by the outer layer for the
 * inner layer to figure out what to do with.
 */
typedef enum {
    QIN_HBA_REG=99,     /* the argument is a pointer to a hba_register_t */
    QIN_GETINFO,        /* the argument is a pointer to a info_t */
    QIN_SETINFO,        /* the argument is a pointer to a info_t */
    QIN_GETDLIST,       /* the argument is a pointer to a fc_dlist_t */
    QIN_ENABLE,         /* the argument is a pointer to a enadis_t */
    QIN_DISABLE,        /* the argument is a pointer to a enadis_t */
    QIN_TMD_CONT,       /* the argument is a pointer to a tmd_xact_t */
    QIN_TMD_FIN,        /* the argument is a pointer to a tmd_cmd_t */
    QIN_NOTIFY_ACK,     /* the argument is a pointer to a notify_t */
    QIN_HBA_UNREG,      /* the argument is a pointer to a hba_register_t */
} qact_e;

/*
 * This structure is used to register to the outer layer the
 * binding of an HBA identifier, driver name and instance and the
 * lun width capapbilities of this inner layer. It's up to each
 * platform to figure out how it wants to actually implement this.
 * A typical sequence would be for the MD layer to find some external
 * module's entry point and start by sending a QOUT_HBA_REG with info
 * filled in, and the external module to call back with a QIN_HBA_REG
 * that passes back the corresponding information.
 *
 * The r_version tag defines the version of this API.
 */
#define    QR_VERSION    20
typedef struct {
    /* NB: structure tags from here to r_version must never change */
    void *                  r_identity;
    void                    (*r_action)(qact_e, void *);
    char                    r_name[8];
    int                     r_inst;
    int                     r_version;
    uint32_t                r_locator;
    uint32_t                r_nchannels;
    enum { R_FC, R_SPI }    r_type;
    void *                  r_private;
} hba_register_t;

/*
 * An information structure that is used to get or set per-channel transport layer parameters.
 */
typedef struct {
    void *                  i_identity;
    enum { I_FC, I_SPI }    i_type;
    int                     i_channel;
    int                     i_error;
    union {
        struct {
            uint64_t    wwnn_nvram;
            uint64_t    wwpn_nvram;
            uint64_t    wwnn;
            uint64_t    wwpn;
        } fc;
        struct {
            int         iid;
        } spi;
    }                       i_id;
} info_t;

/*
 * An information structure to return a list of logged in WWPNs. FC specific.
 */
typedef struct {
    void *                  d_identity;
    int                     d_channel;
    int                     d_error;
    int                     d_count;
    uint64_t *              d_wwpns;
} fc_dlist_t;

/*
 * Lun ENABLE/DISABLE
 *
 * A word about ENABLE/DISABLE: the argument is a pointer to a enadis_t
 * with en_hba, en_chan and en_lun filled out. We used to have an iid
 * and target pair, but this just gets silly so we made initiator id
 * and target id something you set, once, elsewhere.
 *
 * If an error occurs in either enabling or disabling the described lun
 * en_error is set with an appropriate non-zero value.
 */
typedef struct {
    void *          en_private;     /* for outer layer usage */
    void *          en_hba;         /* HBA tag */
    uint16_t        en_lun;         /* logical unit */
    uint16_t        en_chan;        /* channel on card */
    int             en_error;
} enadis_t;



/*
 * Data Transaction
 *
 * A tmd_xact_t is a structure used to describe a transaction within
 * an overall command. It used to be part of the overall command,
 * but it became desirable to allow for multiple simultaneous
 * transfers for a command to happen. Generally these structures
 * define data to be moved (along with the relative offset within
 * the overall command) with the last structure containing status
 * and sense (if needed) as well.
 *
 * The td_cmd tag points back to the owning command.
 *
 * The td_data tag points to the (platform specific) data descriptor.
 *
 * The td_lprivate is for use by the Inner Layer for private usage.
 *
 * The td_xfrlen says whether this transaction is moving data- if nonzero.
 *
 * The td_offset states what the relative offset within the comamnd the
 * data transfer will start at. It is undefined if td_xfrlen is zero.
 *
 * The td_error flag will note any errors that occurred during an attempt
 * to start this transaction. The inner layer is responsible for setting
 * this.
 *
 * The td_hflags tag is set by the outer layer to indicate how the inner
 * layer is supposed to treat this transaction.
 *
 * The td_lflags tag is set by the inner layer to indicate whether this
 * transaction sent status and/or sense. Note that (much as it hurts),
 * this API allows the inner layer to *fail* to send sense even if asked
 * to- that is, AUTOSENSE is not a requirement of this API and the outer
 * layer has to be prepared for this (unlikely) eventuality.
 */

typedef struct tmd_cmd tmd_cmd_t;
typedef struct tmd_xact {
    tmd_cmd_t *         td_cmd;                 /* cross-ref to tmd_cmd_t */
    void *              td_data;                /* data descriptor */
    void *              td_lprivate;            /* private for lower layer */
    uint32_t            td_xfrlen;              /* size of this data load */
    uint32_t            td_offset;              /* offset for this data load */
    int                 td_error;               /* error with this transfer */
    uint8_t             td_hflags;              /* flags set by caller */
    uint8_t             td_lflags;              /* flags set by callee */
} tmd_xact_t;

#define TDFH_STSVALID   0x01    /* status is valid - include it */
#define TDFH_SNSVALID   0x02    /* sense data (from outer layer) good - include it */
#define TDFH_DATA_IN    0x04    /* target (us) -> initiator (them) */
#define TDFH_DATA_OUT   0x08    /* initiator (them) -> target (us) */
#define TDFH_DATA_MASK  0x0C    /* mask to cover data direction */
#define TDFH_PRIVATE    0xF0    /* private outer layer usage */

#define TDFL_SENTSTATUS 0x01    /* this transaction sent status */
#define TDFL_SENTSENSE  0x02    /* this transaction sent sense data */
#define TDFL_ERROR      0x04    /* this transaction had an error */
#define TDFL_SYNCERROR  0x08    /* ... and didn't even start because of it */
#define TDFL_PRIVATE    0xF0    /* private inner layer usage */

/*
 * The command structure below the SCSI Command structure that is
 * is the whole point of this API. After a LUN is (or LUNS are)
 * enabled, initiators who send commands addressed to the port,
 * channel and lun that have been enabled cause an interrupt which
 * causes the chip to receive the command and present it to the
 * inner layer. The inner layer allocates one of this command
 * structures and copies relevant information to it and sends it
 * to the outer layer with the action QOUT_TMD_START.
 *
 * The outer layer is then responsible for command decode and is responsible
 * for sending any transactions back (via a QIN_TMD_CONT) to the inner layer
 * that (optionally) moves data and then sends closing status.
 *
 * The outer layer, when informed of the status of the final transaction
 * then releases this structure by sending it back to the inner layer
 * with the action QOUT_TMD_FIN.
 *
 * The structure tag meanings are as described below.
 *
 * The cd_hba tag is a tag that uniquely identifies the HBA this target
 * mode command is coming from. The outer layer has to pass this back
 * unchanged to avoid chaos. It is identical to the r_identity tag used
 * by the inner layer to register with the outer layer.
 *
 * The cd_iid, cd_channel, cd_tgt and cd_lun tags are used to identify the
 * the initiator who sent us a command, the channel on the this particular
 * hardware port we arrived on (for multiple channel devices), the target we
 * claim to be, and the lun on that target.
 *
 * The cd_tagval field is a tag that uniquely describes this tag. It may
 * or may not have any correspondence to an underying hardware tag. The
 * outer layer must pass this back unchanged or chaos will result.
 *
 * The tag cd_totlen is the total data amount expected to be moved
 * for this command. This will be set to non-zero for transports
 * that know this value from the transport level (e.g., Fibre Channel).
 * If it shows up in the outer layers set to zero, the total data length
 * must be inferred from the CDB.
 *
 * The tag cd_moved is the total amount of data moved so far. It is the
 * responsibilty of the inner layer to set this for every transaction and
 * to keep track of it so that transport level residuals may be correctly
 * set.
 *
 * The cd_cdb contains storage for the passed in SCSI command.
 *
 * The cd_tagtype field specifies what kind of command tag type, if
 * any, has been sent with this command.
 *
 * The tag cd_flags has some junk flags set but mostly has flags reserved for outer layer use.
 *
 * The tags cd_sense and cd_scsi_status are self-explanatory.
 *
 * The cd_xact tag is the first or only transaction structure related to this command.
 *
 * The tag cd_lreserved, cd_hreserved are scratch areas for use for the outer and inner layers respectively.
 * 
 */

#ifndef TMD_CDBLEN
#define TMD_CDBLEN       16
#endif
#ifndef TMD_SENSELEN
#define TMD_SENSELEN     18
#endif
#ifndef QCDS
#define QCDS             (sizeof (uint64_t))
#endif
#ifndef TMD_PRIV_LO
#define TMD_PRIV_LO 4
#endif
#ifndef TMD_PRIV_HI
#define TMD_PRIV_HI 4
#endif

struct tmd_cmd {
    void *              cd_hba;     /* HBA tag */
    uint64_t            cd_iid;     /* initiator ID */
    uint64_t            cd_tgt;     /* target id */
    uint64_t            cd_tagval;  /* tag value */
    uint8_t             cd_lun[8];  /* logical unit */
    uint32_t            cd_totlen;  /* total data load */
    uint32_t            cd_moved;   /* total data moved so far */
    uint16_t            cd_channel; /* channel index */
    uint16_t            cd_flags;   /* flags */
    uint16_t            cd_req_cnt; /* how many tmd_xact_t's are active */
    uint8_t             cd_cdb[TMD_CDBLEN];
    uint8_t             cd_tagtype; /* tag type */
    uint8_t             cd_scsi_status;
    uint8_t             cd_sense[TMD_SENSELEN];
    tmd_xact_t          cd_xact;    /* first or only transaction */
    union {
        void *          ptrs[QCDS / sizeof (void *)];       /* (assume) one pointer */
        uint64_t        llongs[QCDS / sizeof (uint64_t)];   /* one long long */
        uint32_t        longs[QCDS / sizeof (uint32_t)];    /* two longs */
        uint16_t        shorts[QCDS / sizeof (uint16_t)];   /* four shorts */
        uint8_t         bytes[QCDS];                        /* eight bytes */
    } cd_lreserved[TMD_PRIV_LO], cd_hreserved[TMD_PRIV_HI];
};

#define CDF_NODISC      0x0001  /* disconnects disabled */
#define CDF_DATA_IN     0x0002  /* target (us) -> initiator (them) */
#define CDF_DATA_OUT    0x0004  /* initiator (them) -> target (us) */
#define CDF_BIDIR       0x0006  /* bidirectional data */
#define CDF_SNSVALID    0x0008  /* sense is set on incoming command */
#define CDF_PRIVATE     0xff00  /* available for private use in outer layer */

/* defined tags */
#define CD_UNTAGGED     0
#define CD_SIMPLE_TAG   1
#define CD_ORDERED_TAG  2
#define CD_HEAD_TAG     3
#define CD_ACA_TAG      4

#ifndef    TMD_SIZE
#define    TMD_SIZE     (sizeof (tmd_cmd_t))
#endif

#define L0LUN_TO_FLATLUN(lptr)              ((((lptr)[0] & 0x3f) << 8) | ((lptr)[1]))
#define FLATLUN_TO_L0LUN(lptr, lun)                 \
    (lptr)[1] = lun & 0xff;                         \
    if (sizeof (lun) == 1) {                        \
        (lptr)[0] = 0;                              \
    } else {                                        \
        uint16_t nl = lun;                          \
        if (nl == LUN_ANY) {                        \
            (lptr)[0] = (nl >> 8) & 0xff;           \
        } else if (nl < 256) {                      \
            (lptr)[0] = 0;                          \
        } else {                                    \
            (lptr)[0] = 0x40 | ((nl >> 8) & 0x3f);  \
        }                                           \
    }                                               \
    memset(&(lptr)[2], 0, 6)

/*
 * Inner Layer Handler Function.
 *
 * The inner layer target handler function (the outer layer calls this)
 * should be be prototyped like so:
 *
 *    void target_action(qact_e, void *arg)
 *
 * The outer layer target handler function (the inner layer calls this)
 * should be be prototyped like:
 *
 *    void scsi_target_handler(tact_e, void *arg)
 */

#endif    /* _ISP_TPUBLIC_H */
/*
 * vim:ts=4:sw=4:expandtab
 */
