/*
 * Copyright (c) 2021 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CTRL_H
#define CTRL_H

#include "eth_mac.h"
#include "osal.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define GLB_HOSTMAC_L32  0x1300
#define BITS_HOSTMAC_L32 MK_BITS(0, 32)
#define GLB_HOSTMAC_H16  0x1304
#define BITS_HOSTMAC_H16 MK_BITS(0, 16)

#define GLB_SOFT_RESET           0x1308
#define BITS_ETH_SOFT_RESET_ALL  MK_BITS(0, 1)
#define BITS_ETH_SOFT_RESET_UP   MK_BITS(2, 1)
#define BITS_ETH_SOFT_RESET_DOWN MK_BITS(3, 1)

#define GLB_FWCTRL              0x1310
#define BITS_VLAN_ENABLE        MK_BITS(0, 1)
#define BITS_FW2CPU_ENA_U       MK_BITS(5, 1)
#define BITS_FW2CPU_ENA_UP      MK_BITS(5, 1)
#define BITS_FW2CPU_ENA_D       MK_BITS(9, 1)
#define BITS_FW2CPU_ENA_DOWN    MK_BITS(9, 1)
#define BITS_FWALL2CPU_U        MK_BITS(7, 1)
#define BITS_FWALL2CPU_UP       MK_BITS(7, 1)
#define BITS_FWALL2CPU_D        MK_BITS(11, 1)
#define BITS_FWALL2CPU_DOWN     MK_BITS(11, 1)
#define BITS_FW2OTHPORT_ENA_U   MK_BITS(4, 1)
#define BITS_FW2OTHPORT_ENA_D   MK_BITS(8, 1)
#define BITS_FW2OTHPORT_FORCE_U MK_BITS(6, 1)
#define BITS_FW2OTHPORT_FORCE_D MK_BITS(10, 1)

#define GLB_MACTCTRL         0x1314
#define BITS_MACT_ENA_U      MK_BITS(7, 1)
#define BITS_MACT_ENA_D      MK_BITS(15, 1)
#define BITS_BROAD2CPU_U     MK_BITS(5, 1)
#define BITS_BROAD2CPU_UP    MK_BITS(5, 1)
#define BITS_BROAD2CPU_D     MK_BITS(13, 1)
#define BITS_BROAD2CPU_DOWN  MK_BITS(13, 1)
#define BITS_BROAD2OTHPORT_U MK_BITS(4, 1)
#define BITS_BROAD2OTHPORT_D MK_BITS(12, 1)
#define BITS_MULTI2CPU_U     MK_BITS(3, 1)
#define BITS_MULTI2CPU_D     MK_BITS(11, 1)
#define BITS_MULTI2OTHPORT_U MK_BITS(2, 1)
#define BITS_MULTI2OTHPORT_D MK_BITS(10, 1)
#define BITS_UNI2CPU_U       MK_BITS(1, 1)
#define BITS_UNI2CPU_D       MK_BITS(9, 1)
#define BITS_UNI2OTHPORT_U   MK_BITS(0, 1)
#define BITS_UNI2OTHPORT_D   MK_BITS(8, 1)

/* ENDIAN */
#define GLB_ENDIAN_MOD      0x1318
#define BITS_ENDIAN         MK_BITS(0, 2)
#define HIETH_BIG_ENDIAN    0
#define HIETH_LITTLE_ENDIAN 3

/* IRQs */
#define GLB_RO_IRQ_STAT 0x1330
#define GLB_RW_IRQ_ENA  0x1334
#define GLB_RW_IRQ_RAW  0x1338

/* IRQs mask bits */
#define BITS_IRQS_U           MK_BITS(0, 8)
#define BITS_VLAN_IRQS        MK_BITS(11, 1)
#define BITS_MDIO_IRQS        MK_BITS(13, 2)
#define BITS_IRQS_ENA_D       MK_BITS(17, 1)
#define BITS_IRQS_ENA_U       MK_BITS(18, 1)
#define BITS_IRQS_ENA_ALLPORT MK_BITS(19, 1)
#define BITS_IRQS_D           MK_BITS(20, 8)

#define BITS_IRQS_MASK_U (0xFF)
#define BITS_IRQS_MASK_D (0xFF << 20)

/* IRQs bit name */
#define HIETH_INT_RX_RDY_U      (1 << 0)
#define HIETH_INT_RX_RDY_D      (1 << 20)
#define HIETH_INT_TX_FIN_U      (1 << 1)
#define HIETH_INT_TX_FIN_D      (1 << 21)
#define HIETH_INT_LINK_CH_U     (1 << 2)
#define HIETH_INT_LINK_CH_D     (1 << 22)
#define HIETH_INT_SPEED_CH_U    (1 << 3)
#define HIETH_INT_SPEED_CH_D    (1 << 23)
#define HIETH_INT_DUPLEX_CH_U   (1 << 4)
#define HIETH_INT_DUPLEX_CH_D   (1 << 24)
#define HIETH_INT_STATE_CH_U    (1 << 5)
#define HIETH_INT_STATE_CH_D    (1 << 25)
#define HIETH_INT_TXQUE_RDY_U   (1 << 6)
#define HIETH_INT_TXQUE_RDY_D   (1 << 26)
#define HIETH_INT_MULTI_RXRDY_U (1 << 7)
#define HIETH_INT_MULTI_RXRDY_D (1 << 27)
#define HIETH_INT_TX_ERR_U      (1 << 8)
#define HIETH_INT_TX_ERR_D      (1 << 28)

#define HIETH_INT_MDIO_FINISH   (1 << 12)
#define HIETH_INT_UNKNOW_VLANID (1 << 13)
#define HIETH_INT_UNKNOW_VLANM  (1 << 14)

#define GLB_DN_HOSTMAC_L32 0x1340
#define GLB_DN_HOSTMAC_H16 0x1344
#define GLB_DN_HOSTMAC_ENA 0x1348
#define BITS_DN_HOST_ENA   MK_BITS(0, 1)

#define GLB_MAC_L32_BASE      (0x1400)
#define GLB_MAC_H16_BASE      (0x1404)
#define GLB_MAC_L32_BASE_D    (0x1400 + 16 * 0x8)
#define GLB_MAC_H16_BASE_D    (0x1404 + 16 * 0x8)
#define BITS_MACFLT_HI16      MK_BITS(0, 16)
#define BITS_MACFLT_FW2CPU_U  MK_BITS(21, 1)
#define BITS_MACFLT_FW2PORT_U MK_BITS(20, 1)
#define BITS_MACFLT_ENA_U     MK_BITS(17, 1)
#define BITS_MACFLT_FW2CPU_D  MK_BITS(19, 1)
#define BITS_MACFLT_FW2PORT_D MK_BITS(18, 1)
#define BITS_MACFLT_ENA_D     MK_BITS(16, 1)

/* Tx/Rx Queue depth */
#define U_GLB_QLEN_SET 0x0344
#define D_GLB_QLEN_SET 0x2344
#define BITS_TXQ_DEP   MK_BITS(0, 6)
#define BITS_RXQ_DEP   MK_BITS(8, 6)

#define U_GLB_FC_LEVEL       0x0348
#define D_GLB_FC_LEVEL       0x2348
#define BITS_FC_DEACTIVE_THR MK_BITS(0, 6)
#define BITS_FC_ACTIVE_THR   MK_BITS(8, 6)
#define BITS_FC_EN           MK_BITS(14, 1)

#define BITS_PAUSE_EN MK_BITS(18, 1)

/* Rx (read only) Queue-ID and LEN */
#define U_GLB_RO_IQFRM_DES 0x0354
#define D_GLB_RO_IQFRM_DES 0x2354

/* rx buffer addr. */
#define U_GLB_RXFRM_SADDR 0x0350
#define D_GLB_RXFRM_SADDR 0x2350
/* bits of U_GLB_RO_IQFRM_DES */
#define BITS_RXPKG_LEN           MK_BITS(0, 11)
#define BITS_RXPKG_ID            MK_BITS(12, 6)
#define BITS_FRM_VLAN_VID        MK_BITS(18, 1)
#define BITS_FD_VID_VID          MK_BITS(19, 1)
#define BITS_FD_VLANID           MK_BITS(20, 12)
#define BITS_RXPKG_LEN_OFFSET    0
#define BITS_RXPKG_LEN_MASK      0xFFF
#define BITS_PAYLOAD_ERR_OFFSET  20
#define BITS_PAYLOAD_ERR_MASK    0x1
#define BITS_HEADER_ERR_OFFSET   21
#define BITS_HEADER_ERR_MASK     0x1
#define BITS_PAYLOAD_DONE_OFFSET 22
#define BITS_PAYLOAD_DONE_MASK   0x1
#define BITS_HEADER_DONE_OFFSET  23
#define BITS_HEADER_DONE_MASK    0x1

/* Rx ADDR */
#define U_GLB_IQ_ADDR 0x0358
#define D_GLB_IQ_ADDR 0x2358

/* Tx ADDR and LEN */
#define U_GLB_EQ_ADDR   0x0360
#define D_GLB_EQ_ADDR   0x2360
#define U_GLB_EQFRM_LEN 0x0364
#define D_GLB_EQFRM_LEN 0x2364
/* bits of U_GLB_EQFRM_LEN */
#ifdef HIETH_TSO_SUPPORTED
#define BITS_TXINQ_LEN MK_BITS(0, 32)
#else
#define BITS_TXINQ_LEN MK_BITS(0, 11)
#endif

#ifdef HIETH_TSO_SUPPORTED
/* TSO debug enable */
#define U_GLB_TSO_DBG_EN 0x03A4
#define D_GLB_TSO_DBG_EN 0x23A4
#define BITS_TSO_DBG_EN  MK_BITS(31, 1)
/* TSO debug state */
#define U_GLB_TSO_DBG_STATE 0x03A8
#define D_GLB_TSO_DBG_STATE 0x23A8
#define BITS_TSO_DBG_STATE  MK_BITS(31, 1)
/* TSO debug addr */
#define U_GLB_TSO_DBG_ADDR 0x03AC
#define D_GLB_TSO_DBG_ADDR 0x23AC
/* TSO debug tx info */
#define U_GLB_TSO_DBG_TX_INFO 0x03B0
#define D_GLB_TSO_DBG_TX_INFO 0x23B0
/* TSO debug tx err */
#define U_GLB_TSO_DBG_TX_ERR 0x03B4
#define D_GLB_TSO_DBG_TX_ERR 0x23B4
#endif

/* Rx/Tx Queue ID */
#define U_GLB_RO_QUEUE_ID 0x0368
#define D_GLB_RO_QUEUE_ID 0x2368
/* bits of U_GLB_RO_QUEUE_ID */
#define BITS_TXOUTQ_ID MK_BITS(0, 6)
#define BITS_TXINQ_ID  MK_BITS(8, 6)
#define BITS_RXINQ_ID  MK_BITS(16, 6)

/* Rx/Tx Queue staus  */
#define U_GLB_RO_QUEUE_STAT 0x036C
#define D_GLB_RO_QUEUE_STAT 0x236C
/* bits of U_GLB_RO_QUEUE_STAT */
/* check this bit to see if we can add a Tx package */
#define BITS_XMITQ_RDY MK_BITS(24, 1)
/* check this bit to see if we can add a Rx addr */
#define BITS_RECVQ_RDY MK_BITS(25, 1)
/* counts in queue, include currently sending */
#define BITS_XMITQ_CNT_INUSE MK_BITS(0, 6)
/* counts in queue, include currently receving */
#define BITS_RECVQ_CNT_RXOK MK_BITS(8, 6)

#ifdef HIETH_TSO_SUPPORTED
#define E_MAC_TX_FAIL 2
#define E_MAC_SW_GSO  3
#endif

#define HIETH_CSUM_ENABLE  1
#define HIETH_CSUM_DISABLE 0
#if LWIP_TX_CSUM_OFFLOAD
#define HIETH_IPV4_VERSION_HW   0
#define HIETH_IPV6_VERSION_HW   1
#define HIETH_TRANS_TCP_TYPE_HW 0
#define HIETH_TRANS_UDP_TYPE_HW 1
#endif
#define FCS_BYTES 4

/* Rx COE control */
#define U_GLB_RX_COE_CTRL           0x0380
#define D_GLB_RX_COE_CTRL           0x2380
#define BITS_COE_IPV6_UDP_ZERO_DROP MK_BITS(13, 1)
#define BITS_COE_PAYLOAD_DROP       MK_BITS(14, 1)
#define BITS_COE_IPHDR_DROP         MK_BITS(15, 1)

/* fephy trim */
#define REG_LD_AM         0x3050
#define BIT_MASK_LD_SET   MK_BITS(0, 0x1f)
#define REG_LDO_AM        0x3051
#define BIT_MASK_LDO_SET  MK_BITS(0, 0x7)
#define REG_R_TUNING      0x3052
#define BIT_MASK_R_TUNING MK_BITS(0, 0x3f)

#define BIT_OFFSET_LD_SET   25
#define BIT_OFFSET_LDO_SET  22
#define BIT_OFFSET_R_TUNING 16

#define REG_DEF_ATE       0x3057
#define BIT_AUTOTRIM_DONE (0x1 << 0)

#define MII_EXPMD 0x1d
#define MII_EXPMA 0x1e

#define REG_WR_DONE  0x3053
#define BIT_CFG_DONE (0x1 << 0)
#define BIT_CFG_ACK  (0x1 << 1)

#define IsRecvPacket(ld) (HiethRead(ld, GLB_RW_IRQ_RAW) & (UD_BIT_NAME(HIETH_INT_RX_RDY)))

#define HwSetRxpkgFinish(ld) HiethWrite(ld, UD_BIT_NAME(HIETH_INT_RX_RDY), GLB_RW_IRQ_RAW)

#define HwGetRxpkgInfo(ld) HiethRead(ld, UD_REG_NAME(GLB_RO_IQFRM_DES))

#define HwXmitqCntInUse(ld) HiethReadlBits(ld, UD_REG_NAME(GLB_RO_QUEUE_STAT), BITS_XMITQ_CNT_INUSE)

#define HwXmitqPkg(ld, addr, len) \
    do { \
        HiethWrite(ld, (addr), UD_REG_NAME(GLB_EQ_ADDR)); \
        HiethWritelBits(ld, (len), UD_REG_NAME(GLB_EQFRM_LEN), BITS_TXINQ_LEN); \
    } while (0)

struct HiethPriv;

void HiethHwExternalPhyReset(void);
void HiethHwMacCoreInit(struct HiethNetdevLocal *ld);
void HiethFephyTrim(struct HiethNetdevLocal *ld, const EthPhyAccess *f);

int32_t TestXmitQueueReady(struct HiethNetdevLocal *ld);

/* return last irq_enable status */
int32_t HiethIrqEnable(struct HiethNetdevLocal *ld, int32_t irqs);

/* return last irq_enable status */
int32_t HiethIrqDisable(struct HiethNetdevLocal *ld, int32_t irqs);

/* return irqstatus */
int32_t HiethReadIrqstatus(struct HiethNetdevLocal *ld);

/* return irqstatus after clean */
int32_t HiethClearIrqstatus(struct HiethNetdevLocal *ld, int32_t irqs);

int32_t HiethSetEndianMode(struct HiethNetdevLocal *ld, int32_t mode);

/* Tx/Rx queue operation */
int32_t HiethSetHwqDepth(struct HiethNetdevLocal *ld);

int32_t HiethHwSetMacAddress(struct HiethNetdevLocal *ld, int32_t ena, const uint8_t *mac);
int32_t HiethHwGetMacAddress(struct HiethNetdevLocal *ld, uint8_t *mac);

int32_t HiethFeedHw(struct HiethNetdevLocal *ld, HiethPriv *priv);
int32_t HiethXmitGso(struct HiethNetdevLocal *ld, const HiethPriv *priv, NetBuf *netBuf);
int32_t HiethXmitReleasePkt(struct HiethNetdevLocal *ld, const HiethPriv *priv);
void RegisterHiethData(struct EthDevice *ethDevice);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* CTRL_H */
