///* TODO: Rename to MMCH_0_REG_BASE and add the base address for the other items */
//#define MMCHS1_REG_BASE 0x48060000

//#ifdef AM_DM37x_Multimedia_Device
#define MMCHS1_REG_BASE 0x4809C000
//#define MMCHS2_REG_BASE 0x480B4000
//#define MMCHS3_REG_BASE 0x480AD000
//#endif

#define MMCHS_SD_SYSCONFIG 0x110 /* SD system configuration */
#define MMCHS_SD_SYSSTATUS 0x114 /* SD system status */
#define MMCHS_SD_CON       0x12c /* Configuration (functional mode,card initialization etc) */
#define MMCHS_SD_BLK       0x204 /* Transfer length configuration */
#define MMCHS_SD_ARG       0x208 /* Command argument bit 38-8 of command format*/
#define MMCHS_SD_CMD       0x20c /* Command and transfer mode */
#define MMCHS_SD_RSP10     0x210 /* Command response 0 and 1 */
#define MMCHS_SD_RSP32     0x214 /* Command response 2 and 3  */
#define MMCHS_SD_RSP54     0x218 /* Command response 4 and 5  */
#define MMCHS_SD_RSP76     0x21c /* Command response 6 and 7  */
#define MMCHS_SD_DATA      0x220 /* Data register */
#define MMCHS_SD_PSTATE    0x224 /* Present state */
#define MMCHS_SD_HCTL      0x228 /* Host control(power ,wake-up and transfer) */
#define MMCHS_SD_SYSCTL    0x22c /* SD System control (reset,clocks and timeout) */
#define MMCHS_SD_STAT      0x230 /* SD Interrupt status */
#define MMCHS_SD_IE        0x234 /* SD Interrupt Enable register */
#define MMCHS_SD_ISE       0x238 /* SD Interrupt Signal Enable register */
#define MMCHS_SD_CAPA      0x240 /* Capabilities of the host controller */
#define MMCHS_SD_CUR_CAPA  0x248 /* Current capabilities of the host controller */

#define MMCHS_SD_SYSCONFIG_AUTOIDLE                    (0x1 << 0)  /* Internal clock gating strategy */
#define MMCHS_SD_SYSCONFIG_AUTOIDLE_DIS                (0x0 << 0)  /* Clocks are free running */
#define MMCHS_SD_SYSCONFIG_AUTOIDLE_EN                 (0x1 << 0)  /* Automatic clock gating strategy */
#define MMCHS_SD_SYSCONFIG_SOFTRESET                   (0x1 << 1)  /* Software reset bit writing  */
#define MMCHS_SD_SYSCONFIG_ENAWAKEUP                   (0x1 << 2)  /* Wake-up feature control */
#define MMCHS_SD_SYSCONFIG_ENAWAKEUP_DIS               (0x0 << 2)  /* Disable wake-up capability */
#define MMCHS_SD_SYSCONFIG_ENAWAKEUP_EN                (0x1 << 2)  /* Enable wake-up capability */
#define MMCHS_SD_SYSCONFIG_SIDLEMODE                   (0x3 << 3)  /* Power management */
#define MMCHS_SD_SYSCONFIG_SIDLEMODE_UNCONDITIONAL     (0x0 << 3)  /* Go into idle mode unconditionally upon request */
#define MMCHS_SD_SYSCONFIG_SIDLEMODE_IGNORE            (0x1 << 3)  /* Ignore ILDE requests */
#define MMCHS_SD_SYSCONFIG_SIDLEMODE_IDLE              (0x2 << 3)  /* Acknowledge IDLE request switch to wake-up mode */
#define MMCHS_SD_SYSCONFIG_SIDLEMODE_SMART_IDLE        (0x3 << 3)  /* Smart-idle */
#define MMCHS_SD_SYSCONFIG_CLOCKACTIVITY               (0x3 << 8)  /* Clock activity during wake-up */
#define MMCHS_SD_SYSCONFIG_CLOCKACTIVITY_OFF           (0x0 << 8)  /* Interface and functional clock can be switched off */
#define MMCHS_SD_SYSCONFIG_CLOCKACTIVITY_IF            (0x1 << 8)  /* Only Interface clock (functional can be switched off*/
#define MMCHS_SD_SYSCONFIG_CLOCKACTIVITY_FUNC          (0x2 << 8)  /* Only Functional clock (interface clock can be switched off) */
#define MMCHS_SD_SYSCONFIG_CLOCKACTIVITY_BOOTH         (0x3 << 8)  /* Booth the interface and functional clock are maintained */
#define MMCHS_SD_SYSCONFIG_STANDBYMODE                 (0x3 << 12) /* Configuration for standby */
#define MMCHS_SD_SYSCONFIG_STANDBYMODE_FORCE_STANDBY   (0x0 << 12) /* Force standby mode upon idle request*/
#define MMCHS_SD_SYSCONFIG_STANDBYMODE_NO_STANDBY      (0x1 << 12) /* Never go into standby mode */
#define MMCHS_SD_SYSCONFIG_STANDBYMODE_WAKEUP_INTERNAL (0x2 << 12) /* Go into wake-up mode based on internal knowledge */
#define MMCHS_SD_SYSCONFIG_STANDBYMODE_WAKEUP_SMART    (0x3 << 12) /* Go info wake-up mode when possible */

#define MMCHS_SD_SYSSTATUS_RESETDONE 0x01

#define MMCHS_SD_CON_DW8          (0x1 << 5) /* 8-bit mode MMC select , For SD clear this bit */
#define MMCHS_SD_CON_DW8_1BIT     (0x0 << 5) /* 1 or 4 bits data width configuration(also set SD_HCTL) */
#define MMCHS_SD_CON_DW8_8BITS    (0x1 << 5) /* 8 bits data width configuration */
#define MMCHS_SD_CON_INIT         (0x1 << 1) /* Send initialization stream (all cards) */
#define MMCHS_SD_CON_INIT_NOINIT  (0x0 << 1) /* Do nothing */
#define MMCHS_SD_CON_INIT_INIT    (0x1 << 1) /* Send initialization stream */

#define MMCHS_SD_BLK_NBLK             (0xffffu << 16) /* Block count for the current transfer */
#define MMCHS_SD_BLK_BLEN             (0xfff << 0)     /* Transfer block size */
#define MMCHS_SD_BLK_BLEN_NOTRANSFER  (0x0 << 0)       /* No transfer */

#define MMCHS_SD_CMD_INDX                 (0x3f << 24) /* Command index */
#define MMCHS_SD_CMD_INDX_CMD(x)          (x << 24)    /* MMC command index binary encoded values from 0 to 63 */

#define MMCHS_SD_ARG_MASK                 (0xffffffffu)      /* Mask everything */
#define MMCHS_SD_ARG_CMD8_VHS             (0x1 << (16 - 8))  /* Voltage between 2.7 and 3.6 v*/
#define MMCHS_SD_ARG_CMD8_CHECK_PATTERN   (0xaa <<(8 - 8))   /* 10101010b pattern */

#define MMCHS_SD_CMD_TYPE                 (0x3 << 22) /* Command type. */
#define MMCHS_SD_CMD_TYPE_OTHER           (0x0 << 22) /* Other type of commands (like go idle) */
#define MMCHS_SD_CMD_TYPE_BUS_SUSPEND     (0x1 << 22) /* Upon CMD52 "Bus Suspend" operation */
#define MMCHS_SD_CMD_TYPE_FUNCTION_SELECT (0x2 << 22) /* Upon CMD52 "Function Select" operation */
#define MMCHS_SD_CMD_TYPE_IOABORT         (0x3 << 22) /* Upon CMD12 and CMD21 "I/O Abort */
#define MMCHS_SD_CMD_DP                   (0x1 << 21) /* Data present select */
#define MMCHS_SD_CMD_DP_DATA              (0x1 << 21) /* Additional data is present on the data lines */
#define MMCHS_SD_CMD_DP_NODATA            (0x0 << 21) /* No additional data is present on the data lines */
#define MMCHS_SD_CMD_CICE                 (0x1 << 20) /* Command index response check enable */
#define MMCHS_SD_CMD_CICE_ENABLE          (0x1 << 20) /* Enable index check response  */
#define MMCHS_SD_CMD_CICE_DISABLE         (0x0 << 20) /* Disable index check response */
#define MMCHS_SD_CMD_CCCE                 (0x1 << 19) /* Command CRC7 Check enable on responses*/
#define MMCHS_SD_CMD_CCCE_ENABLE          (0x1 << 19) /* Enable CRC7 Check on response */
#define MMCHS_SD_CMD_CCCE_DISABLE         (0x0 << 19) /* Disable CRC7 Check on response */
#define MMCHS_SD_CMD_RSP_TYPE             (0x3 << 16) /* Response type */
#define MMCHS_SD_CMD_RSP_TYPE_NO_RESP     (0x0 << 16) /* No response */
#define MMCHS_SD_CMD_RSP_TYPE_136B        (0x1 << 16) /* Response length 136 bits */
#define MMCHS_SD_CMD_RSP_TYPE_48B         (0x2 << 16) /* Response length 48 bits */
#define MMCHS_SD_CMD_RSP_TYPE_48B_BUSY    (0x3 << 16) /* Response length 48 bits with busy after response */
#define MMCHS_SD_CMD_MSBS                 (0x1 << 5)  /* Multi/Single block select */
#define MMCHS_SD_CMD_MSBS_SINGLE          (0x0 << 5)  /* Single block mode */
#define MMCHS_SD_CMD_MSBS_MULTI           (0x0 << 5)  /* Multi block mode */
#define MMCHS_SD_CMD_DDIR                 (0x1 << 4)  /* Data transfer direction */
#define MMCHS_SD_CMD_DDIR_READ            (0x1 << 4)  /* Data read (card to host) */
#define MMCHS_SD_CMD_DDIR_WRITE           (0x0 << 4)  /* Data write (host to card)  */
#define MMCHS_SD_CMD_ACEN                 (0x1 << 2)  /* Auto CMD12 Enable */
#define MMCHS_SD_CMD_ACEN_DIS             (0x0 << 2)  /* Auto CMD12 Disable */
#define MMCHS_SD_CMD_ACEN_EN              (0x1 << 2)  /* Auto CMD12 Enable */
#define MMCHS_SD_CMD_BCE                  (0x1 << 1)  /* Block Count Enable(for multi block transfer) */
#define MMCHS_SD_CMD_BCE_DIS              (0x0 << 1)  /* Disabled block count for infinite transfer*/
#define MMCHS_SD_CMD_BCE_EN               (0x1 << 1)  /* Enabled for multi block transfer with know amount of blocks */
#define MMCHS_SD_CMD_DE                   (0x1 << 0)  /* DMA enable */
#define MMCHS_SD_CMD_DE_DIS               (0x0 << 0)  /* Disable DMA */
#define MMCHS_SD_CMD_DE_EN                (0x1 << 0)  /* Enable DMA  */
#define MMCHS_SD_CMD_MASK  				   ~(0x1 << 30  | 0x1 << 31 | 0x1 << 18 | 0x1 <<3) /* bits 30 , 31 and 18 are reserved */

#define MMCHS_SD_PSTATE_CI           (0x1 << 16) /* Card Inserted */
#define MMCHS_SD_PSTATE_CI_INSERTED  (0x1 << 16) /* Card Inserted  is inserted*/
#define MMCHS_SD_PSTATE_BRE          (0x0 << 11) /* Buffer read enable */
#define MMCHS_SD_PSTATE_BRE_DIS      (0x0 << 11) /* Read BLEN bytes disabled*/
#define MMCHS_SD_PSTATE_BRE_EN       (0x1 << 11) /* Read BLEN bytes enabled*/
#define MMCHS_SD_PSTATE_BWE          (0x0 << 10) /* Buffer Write enable */
#define MMCHS_SD_PSTATE_BWE_DIS      (0x0 << 10) /* There is no room left in the buffer to write BLEN bytes of data */
#define MMCHS_SD_PSTATE_BWE_EN       (0x1 << 10) /* There is enough space in the buffer to write BLEN bytes of data*/

#define MMCHS_SD_HCTL_DTW            (0x1 << 1) /*Data transfer width.(must be set after a successful ACMD6) */
#define MMCHS_SD_HCTL_DTW_1BIT       (0x0 << 1) /*1 bit transfer with */
#define MMCHS_SD_HCTL_DTW_4BIT       (0x1 << 1) /*4 bit transfer with */
#define MMCHS_SD_HCTL_SDBP           (0x1 << 8) /*SD bus power */
#define MMCHS_SD_HCTL_SDBP_OFF       (0x0 << 8) /*SD Power off (start card detect?) */
#define MMCHS_SD_HCTL_SDBP_ON        (0x1 << 8) /*SD Power on (start card detect?) */
#define MMCHS_SD_HCTL_SDVS           (0x7 << 9) /*SD bus voltage select */
#define MMCHS_SD_HCTL_SDVS_VS18      (0x5 << 9) /*1.8 V */
#define MMCHS_SD_HCTL_SDVS_VS30      (0x6 << 9) /*3.0 V */
#define MMCHS_SD_HCTL_SDVS_VS33      (0x7 << 9) /*3.3 V */
#define MMCHS_SD_HCTL_IWE            (0x1 << 24)/* wake-up event on SD interrupt */
#define MMCHS_SD_HCTL_IWE_DIS        (0x0 << 24)/* Disable wake-up on SD interrupt */
#define MMCHS_SD_HCTL_IWE_EN         (0x1 << 24)/* Enable wake-up on SD interrupt */

#define MMCHS_SD_SYSCTL_CLKD (0x3ff << 6)  /* 10 bits clock frequency select */
#define MMCHS_SD_SYSCTL_SRD  (0x1   << 26)  /* Soft reset for mmc_dat line */
#define MMCHS_SD_SYSCTL_SRC  (0x1   << 25)  /* Soft reset for mmc_cmd line */
#define MMCHS_SD_SYSCTL_SRA  (0x1   << 24)  /* Soft reset all (host controller) */

#define MMCHS_SD_SYSCTL_ICE     (0x1 << 0) /* Internal clock enable register  */
#define MMCHS_SD_SYSCTL_ICE_DIS (0x0 << 0) /* Disable internal clock */
#define MMCHS_SD_SYSCTL_ICE_EN  (0x1 << 0) /* Enable internal clock */
#define MMCHS_SD_SYSCTL_ICS          (0x1 << 1) /* Internal clock stable register  */
#define MMCHS_SD_SYSCTL_ICS_UNSTABLE (0x0 << 1) /* Internal clock is unstable */
#define MMCHS_SD_SYSCTL_ICS_STABLE   (0x1 << 1) /* Internal clock is stable   */
#define MMCHS_SD_SYSCTL_CEN          (0x1 << 2) /* Card lock enable provide clock to the card */
#define MMCHS_SD_SYSCTL_CEN_DIS      (0x0 << 2) /* Internal clock is unstable */
#define MMCHS_SD_SYSCTL_CEN_EN       (0x1 << 2) /* Internal clock is stable   */

#define MMCHS_SD_SYSCTL_DTO          (0xf << 16) /* Data timeout counter  */
#define MMCHS_SD_SYSCTL_DTO_2POW13   (0x0 << 16) /* TCF x 2^13  */
#define MMCHS_SD_SYSCTL_DTO_2POW14   (0x1 << 16) /* TCF x 2^14  */
#define MMCHS_SD_SYSCTL_DTO_2POW27   (0xe << 16) /* TCF x 2^27  */

#define MMCHS_SD_STAT_ERRI            (0x01 << 15) /* Error interrupt */
#define MMCHS_SD_STAT_ERROR_MASK     (0xff << 15 | 0x3 << 24 | 0x03 << 28)
#define MMCHS_SD_STAT_CC              (0x1 << 0) /* Command complete status */
#define MMCHS_SD_STAT_CC_UNRAISED     (0x0 << 0) /* Command not completed */
#define MMCHS_SD_STAT_CC_RAISED       (0x1 << 0) /* Command completed */

#define MMCHS_SD_IE_ERROR_MASK     (0xff << 15 | 0x3 << 24 | 0x03 << 28)

#define MMCHS_SD_IE_CC_ENABLE        (0x1 << 0) /* Command complete interrupt enable */
#define MMCHS_SD_IE_CC_ENABLE_ENABLE (0x1 << 0) /* Command complete Interrupts are enabled */
#define MMCHS_SD_IE_CC_ENABLE_CLEAR  (0x1 << 0) /* Clearing is done by writing a 0x1 */

#define MMCHS_SD_IE_TC_ENABLE        (0x1 << 1) /* Transfer complete interrupt enable */
#define MMCHS_SD_IE_TC_ENABLE_ENABLE (0x1 << 1) /* Transfer complete Interrupts are enabled */
#define MMCHS_SD_IE_TC_ENABLE_CLEAR  (0x1 << 1) /* Clearing TC is done by writing a 0x1 */

#define MMCHS_SD_IE_BRR_ENABLE         (0x1 << 5) /* Buffer read ready interrupt  */
#define MMCHS_SD_IE_BRR_ENABLE_DISABLE (0x0 << 5) /* Buffer read ready interrupt disable */
#define MMCHS_SD_IE_BRR_ENABLE_ENABLE  (0x1 << 5) /* Buffer read ready interrupt enable */
#define MMCHS_SD_IE_BRR_ENABLE_CLEAR   (0x1 << 5) /* Buffer read ready interrupt clear */

#define MMCHS_SD_IE_BWR_ENABLE         (0x1 << 4) /* Buffer write ready interrupt  */
#define MMCHS_SD_IE_BWR_ENABLE_DISABLE (0x0 << 4) /* Buffer write ready interrupt disable */
#define MMCHS_SD_IE_BWR_ENABLE_ENABLE  (0x1 << 4) /* Buffer write ready interrupt enable */
#define MMCHS_SD_IE_BWR_ENABLE_CLEAR   (0x1 << 4) /* Buffer write ready interrupt clear */

#define MMCHS_SD_CAPA_VS_MASK (0x7 << 24 )  /* voltage mask */
#define MMCHS_SD_CAPA_VS18 (0x01 << 26 )  /* 1.8 volt */
#define MMCHS_SD_CAPA_VS30 (0x01 << 25 )  /* 3.0 volt */
#define MMCHS_SD_CAPA_VS33 (0x01 << 24 )  /* 3.3 volt */

