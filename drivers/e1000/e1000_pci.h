/**
 * @file e1000_pci.h
 *
 * @brief PCI Device Identity numbers of Intel Pro/1000
 *        Gigabit Ethernet cards.
 *
 * Parts of this code is based on the DragonflyBSD (FreeBSD)
 * implementation, and the fxp driver for Minix 3.
 *
 * @see http://svn.freebsd.org/viewvc/base/head/sys/dev/e1000/
 * @see fxp.c
 *
 * @author Niek Linnenbank <nieklinnenbank@gmail.com>
 * @date September 2009
 *
 */

#ifndef __E1000_PCI_H
#define __E1000_PCI_H

/**
 * @name PCI Device ID's.
 * @{
 */

#define E1000_DEV_ID_82542                    0x1000
#define E1000_DEV_ID_82543GC_FIBER            0x1001
#define E1000_DEV_ID_82543GC_COPPER           0x1004
#define E1000_DEV_ID_82544EI_COPPER           0x1008
#define E1000_DEV_ID_82544EI_FIBER            0x1009
#define E1000_DEV_ID_82544GC_COPPER           0x100C
#define E1000_DEV_ID_82544GC_LOM              0x100D
#define E1000_DEV_ID_82540EM                  0x100E
#define E1000_DEV_ID_82545EM                  0x100F
#define E1000_DEV_ID_82540EM_LOM              0x1015
#define E1000_DEV_ID_82540EP_LOM              0x1016
#define E1000_DEV_ID_82540EP                  0x1017
#define E1000_DEV_ID_82540EP_LP               0x101E
#define E1000_DEV_ID_82545EM_COPPER           0x100F
#define E1000_DEV_ID_82545EM_FIBER            0x1011
#define E1000_DEV_ID_82545GM_COPPER           0x1026
#define E1000_DEV_ID_82545GM_FIBER            0x1027
#define E1000_DEV_ID_82545GM_SERDES           0x1028
#define E1000_DEV_ID_82546EB_COPPER           0x1010
#define E1000_DEV_ID_82546EB_FIBER            0x1012
#define E1000_DEV_ID_82546EB_QUAD_COPPER      0x101D
#define E1000_DEV_ID_82546GB_COPPER           0x1079
#define E1000_DEV_ID_82546GB_FIBER            0x107A
#define E1000_DEV_ID_82546GB_SERDES           0x107B
#define E1000_DEV_ID_82546GB_PCIE             0x108A
#define E1000_DEV_ID_82546GB_QUAD_COPPER      0x1099
#define E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3 0x10B5
#define E1000_DEV_ID_82541EI                  0x1013
#define E1000_DEV_ID_82541EI_MOBILE           0x1018
#define E1000_DEV_ID_82541ER_LOM              0x1014
#define E1000_DEV_ID_82541ER                  0x1078
#define E1000_DEV_ID_82541GI                  0x1076
#define E1000_DEV_ID_82541GI_LF               0x107C
#define E1000_DEV_ID_82541GI_MOBILE           0x1077
#define E1000_DEV_ID_82547EI                  0x1019
#define E1000_DEV_ID_82547EI_MOBILE           0x101A
#define E1000_DEV_ID_82547GI                  0x1075
#define E1000_DEV_ID_82571EB_COPPER           0x105E
#define E1000_DEV_ID_82571EB_FIBER            0x105F
#define E1000_DEV_ID_82571EB_SERDES           0x1060
#define E1000_DEV_ID_82571EB_SERDES_DUAL      0x10D9
#define E1000_DEV_ID_82571EB_SERDES_QUAD      0x10DA
#define E1000_DEV_ID_82571EB_QUAD_COPPER      0x10A4
#define E1000_DEV_ID_82571PT_QUAD_COPPER      0x10D5
#define E1000_DEV_ID_82571EB_QUAD_FIBER       0x10A5
#define E1000_DEV_ID_82571EB_QUAD_COPPER_LP   0x10BC
#define E1000_DEV_ID_82572EI_COPPER           0x107D
#define E1000_DEV_ID_82572EI_FIBER            0x107E
#define E1000_DEV_ID_82572EI_SERDES           0x107F
#define E1000_DEV_ID_82572EI                  0x10B9
#define E1000_DEV_ID_82573E                   0x108B
#define E1000_DEV_ID_82573E_IAMT              0x108C
#define E1000_DEV_ID_82573L                   0x109A
#define E1000_DEV_ID_82574L                   0x10D3
#define E1000_DEV_ID_82574LA                  0x10F6
#define E1000_DEV_ID_82583V                   0x150C
#define E1000_DEV_ID_80003ES2LAN_COPPER_DPT   0x1096
#define E1000_DEV_ID_80003ES2LAN_SERDES_DPT   0x1098
#define E1000_DEV_ID_80003ES2LAN_COPPER_SPT   0x10BA
#define E1000_DEV_ID_80003ES2LAN_SERDES_SPT   0x10BB
#define E1000_DEV_ID_ICH8_IGP_M_AMT           0x1049
#define E1000_DEV_ID_ICH8_IGP_AMT             0x104A
#define E1000_DEV_ID_ICH8_IGP_C               0x104B
#define E1000_DEV_ID_ICH8_IFE                 0x104C
#define E1000_DEV_ID_ICH8_IFE_GT              0x10C4
#define E1000_DEV_ID_ICH8_IFE_G               0x10C5
#define E1000_DEV_ID_ICH8_IGP_M               0x104D
#define E1000_DEV_ID_ICH9_IGP_M               0x10BF
#define E1000_DEV_ID_ICH9_IGP_M_AMT           0x10F5
#define E1000_DEV_ID_ICH9_IGP_M_V             0x10CB
#define E1000_DEV_ID_ICH9_IGP_AMT             0x10BD
#define E1000_DEV_ID_ICH9_BM                  0x10E5
#define E1000_DEV_ID_ICH9_IGP_C               0x294C
#define E1000_DEV_ID_ICH9_IFE                 0x10C0
#define E1000_DEV_ID_ICH9_IFE_GT              0x10C3
#define E1000_DEV_ID_ICH9_IFE_G               0x10C2
#define E1000_DEV_ID_ICH10_R_BM_LM            0x10CC
#define E1000_DEV_ID_ICH10_R_BM_LF            0x10CD
#define E1000_DEV_ID_ICH10_R_BM_V             0x10CE
#define E1000_DEV_ID_ICH10_D_BM_LM            0x10DE
#define E1000_DEV_ID_ICH10_D_BM_LF            0x10DF
#define E1000_DEV_ID_PCH_M_HV_LM              0x10EA
#define E1000_DEV_ID_PCH_M_HV_LC              0x10EB
#define E1000_DEV_ID_PCH_D_HV_DM              0x10EF
#define E1000_DEV_ID_PCH_D_HV_DC              0x10F0
#define E1000_DEV_ID_82574L                   0x10D3
#define E1000_DEV_ID_82576                    0x10C9
#define E1000_DEV_ID_82576_FIBER              0x10E6
#define E1000_DEV_ID_82576_SERDES             0x10E7
#define E1000_DEV_ID_82576_QUAD_COPPER        0x10E8
#define E1000_DEV_ID_82576_NS                 0x150A
#define E1000_DEV_ID_82576_SERDES_QUAD        0x150D
#define E1000_DEV_ID_82575EB_COPPER           0x10A7
#define E1000_DEV_ID_82575EB_FIBER_SERDES     0x10A9
#define E1000_DEV_ID_82575GB_QUAD_COPPER      0x10D6
#define E1000_DEV_ID_82575GB_QUAD_COPPER_PM   0x10E2

/**
 * @}
 */

/**
 * @name Revision Numbers.
 * @{
 */

#define E1000_DEV_RID_82540	0x03
#define E1000_DEV_RID_82541	0x05

/**
 * @}
 */

#endif /* __E1000_PCI_H */
