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

#include "ctrl.h"
#include "eth_drv.h"
#include "hieth_pri.h"
#include "hdf_netbuf.h"
#include "mdio.h"
#include <linux/delay.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/if_ether.h>

static inline int32_t FephyExpandedRead(struct HiethNetdevLocal *ld, int32_t phyAddr, int32_t regNum)
{
    HiethMdioWrite(ld, phyAddr, MII_EXPMA, regNum);
    return HiethMdioRead(ld, phyAddr, MII_EXPMD);
}

static inline int32_t FephyExpandedWrite(struct HiethNetdevLocal *ld, int32_t phyAddr, int32_t regNum, int32_t val)
{
    HiethMdioWrite(ld, phyAddr, MII_EXPMA, regNum);
    return HiethMdioWrite(ld, phyAddr, MII_EXPMD, val);
}

static void HiethFephyUseDefaultTrim(struct HiethNetdevLocal *ld, const EthPhyAccess *phyAccess)
{
    uint16_t val;
    int32_t timeout = 3;

    do {
        msleep(250);
        val = FephyExpandedRead(ld, phyAccess->phyAddr, REG_DEF_ATE);
        val &= BIT_AUTOTRIM_DONE;  /* (0x1 << 0) */
    } while (!val && --timeout);

    if (!timeout) {
        HDF_LOGE("festa PHY wait autotrim done timeout!");
    }
    mdelay(5);
}

void HiethFephyTrim(struct HiethNetdevLocal *ld, const EthPhyAccess *phyAccess)
{
    uint32_t val;
    int32_t timeout = 50;
    uint8_t ldSet, ldoSet, tuning;

    val = readl(SYS_CTRL_REG_BASE + 0x8024);
    ldSet = (val >> BIT_OFFSET_LD_SET) & BIT_MASK_LD_SET;
    ldoSet = (val >> BIT_OFFSET_LDO_SET) & BIT_MASK_LDO_SET;
    tuning = (val >> BIT_OFFSET_R_TUNING) & BIT_MASK_R_TUNING;
    if ((!ldSet) && (!ldoSet) && (!tuning)) {
        HiethFephyUseDefaultTrim(ld, phyAccess);
        return;
    }
    val = FephyExpandedRead(ld, phyAccess->phyAddr, REG_LD_AM);
    val = (val & ~BIT_MASK_LD_SET) | (ldSet & BIT_MASK_LD_SET);
    FephyExpandedWrite(ld, phyAccess->phyAddr, REG_LD_AM, val);

    val = FephyExpandedRead(ld, phyAccess->phyAddr, REG_LDO_AM);
    val = (val & ~BIT_MASK_LDO_SET) | (ldoSet & BIT_MASK_LDO_SET);
    FephyExpandedWrite(ld, phyAccess->phyAddr, REG_LDO_AM, val);

    val = FephyExpandedRead(ld, phyAccess->phyAddr, REG_R_TUNING);
    val = (val & ~BIT_MASK_R_TUNING) | (tuning & BIT_MASK_R_TUNING);
    FephyExpandedWrite(ld, phyAccess->phyAddr, REG_R_TUNING, val);

    val = FephyExpandedRead(ld, phyAccess->phyAddr, REG_WR_DONE);
    if (val & BIT_CFG_ACK) {
        HDF_LOGE("festa PHY 0x3053 bit CFG_ACK value: 1");
    }
    val = val | BIT_CFG_DONE;

    FephyExpandedWrite(ld, phyAccess->phyAddr, REG_WR_DONE, val);

    do {
        msleep(5);
        val = FephyExpandedRead(ld, phyAccess->phyAddr, REG_WR_DONE);
        val &= BIT_CFG_ACK;
    } while (!val && --timeout);

    if (!timeout) {
        HDF_LOGE("festa PHY 0x3053 wait bit CFG_ACK timeout!\n");
    }

    mdelay(5);
}

static inline void HiethEnableRxcsumDrop(struct HiethNetdevLocal *ld, bool drop)
{
    HiethWritelBits(ld, drop, UD_REG_NAME(GLB_RX_COE_CTRL), BITS_COE_IPHDR_DROP);
    HiethWritelBits(ld, false, UD_REG_NAME(GLB_RX_COE_CTRL), BITS_COE_PAYLOAD_DROP);
    HiethWritelBits(ld, drop, UD_REG_NAME(GLB_RX_COE_CTRL), BITS_COE_IPV6_UDP_ZERO_DROP);
}

void HiethHwMacCoreInit(struct HiethNetdevLocal *ld)
{
    OsalSpinInit(&(ld->tx_lock));
    OsalSpinInit(&(ld->rx_lock));

#ifdef HIETH_RXCSUM_SUPPORTED
    HiethEnableRxcsumDrop(ld, true);
#endif

#ifdef HIETH_TSO_SUPPORTED
    ld->sgHead = ld->sgTail = 0;
    ld->txqHead = ld->txqTail = 0;
#endif
    ld->txHwCnt = 0;

    /* setup hardware */
    (void)HiethSetHwqDepth(ld);
}

void HiethHwExternalPhyReset(void)
{
    uint32_t val;

    READ_UINT32(val, HIETH_CRG_IOBASE);
    val |= ETH_PHY_RESET;
    WRITE_UINT32(val, HIETH_CRG_IOBASE);

    LOS_Msleep(20);

    READ_UINT32(val, HIETH_CRG_IOBASE);
    val &= ~ETH_PHY_RESET;
    WRITE_UINT32(val, HIETH_CRG_IOBASE);

    LOS_Msleep(30);
}

static inline int32_t IrqEnable(struct HiethNetdevLocal *ld, int32_t irqs)
{
    unsigned long old;

    old = HiethRead(ld, GLB_RW_IRQ_ENA);
    HiethWrite(ld, old | (unsigned long)irqs, GLB_RW_IRQ_ENA);
    old = HiethRead(ld, GLB_RW_IRQ_ENA);
    return old;
}

static inline int32_t IrqDisable(struct HiethNetdevLocal *ld, int32_t irqs)
{
    unsigned long old;

    old = HiethRead(ld, GLB_RW_IRQ_ENA);
    HiethWrite(ld, old & (~(unsigned long)irqs), GLB_RW_IRQ_ENA);
    return old;
}

static inline int32_t ReadIrqstatus(struct HiethNetdevLocal *ld)
{
    int32_t status;

    status = HiethRead(ld, GLB_RO_IRQ_STAT);
    return status;
}

int32_t HiethHwSetMacAddress(struct HiethNetdevLocal *ld, int32_t ena, const uint8_t *mac)
{
    unsigned long reg;

    if (ld->port == DOWN_PORT) {
        HiethWritelBits(ld, 1, GLB_DN_HOSTMAC_ENA, BITS_DN_HOST_ENA);
    }

    reg = mac[1] | (mac[0] << 8);
    if (ld->port == UP_PORT) {
        HiethWrite(ld, reg, GLB_HOSTMAC_H16);
    } else {
        HiethWrite(ld, reg, GLB_DN_HOSTMAC_H16);
    }

    reg = mac[5] | (mac[4] << 8) | (mac[3] << 16) | (mac[2] << 24);
    if (ld->port == UP_PORT) {
        HiethWrite(ld, reg, GLB_HOSTMAC_L32);
    } else {
        HiethWrite(ld, reg, GLB_DN_HOSTMAC_L32);
    }
    return HDF_SUCCESS;
}

int32_t HiethHwGetMacAddress(struct HiethNetdevLocal *ld, uint8_t *mac)
{
    unsigned long reg;

    if (ld->port == UP_PORT) {
        reg = HiethRead(ld, GLB_HOSTMAC_H16);
    } else {
        reg = HiethRead(ld, GLB_DN_HOSTMAC_H16);
    }
    mac[0] = (reg >> 8) & 0xff;
    mac[1] = reg & 0xff;

    if (ld->port == UP_PORT) {
        reg = HiethRead(ld, GLB_HOSTMAC_L32);
    } else {
        reg = HiethRead(ld, GLB_DN_HOSTMAC_L32);
    }
    mac[2] = (reg >> 24) & 0xff;
    mac[3] = (reg >> 16) & 0xff;
    mac[4] = (reg >> 8) & 0xff;
    mac[5] = reg & 0xff;
    return HDF_SUCCESS;
}

int32_t TestXmitQueueReady(struct HiethNetdevLocal *ld)
{
    return HiethReadlBits(ld, UD_REG_NAME(GLB_RO_QUEUE_STAT), BITS_XMITQ_RDY);
}

int32_t HiethIrqEnable(struct HiethNetdevLocal *ld, int32_t irqs)
{
    int32_t old;

    OsalSpinLockIrq(&hiethGlbRegLock);
    old = IrqEnable(ld, irqs);
    OsalSpinUnlockIrq(&hiethGlbRegLock);
    return old;
}

int32_t HiethIrqDisable(struct HiethNetdevLocal *ld, int32_t irqs)
{
    int32_t old;

    OsalSpinLockIrq(&hiethGlbRegLock);
    old = IrqDisable(ld, irqs);
    OsalSpinUnlockIrq(&hiethGlbRegLock);
    return old;
}

int32_t HiethReadIrqstatus(struct HiethNetdevLocal *ld)
{
    return ReadIrqstatus(ld);
}

int32_t HiethClearIrqstatus(struct HiethNetdevLocal *ld, int32_t irqs)
{
    int32_t status;

    OsalSpinLockIrq(&hiethGlbRegLock);
    HiethWrite(ld, irqs, GLB_RW_IRQ_RAW);
    status = ReadIrqstatus(ld);
    OsalSpinUnlockIrq(&hiethGlbRegLock);
    return status;
}

int32_t HiethSetEndianMode(struct HiethNetdevLocal *ld, int32_t mode)
{
    int32_t old;

    old = HiethReadlBits(ld, GLB_ENDIAN_MOD, BITS_ENDIAN);
    HiethWritelBits(ld, mode, GLB_ENDIAN_MOD, BITS_ENDIAN);
    return old;
}

int32_t HiethSetHwqDepth(struct HiethNetdevLocal *ld)
{
    HiethAssert(ld->depth.hwXmitq > 0 && ld->depth.hwXmitq <= HIETH_MAX_QUEUE_DEPTH);
    if ((ld->depth.hwXmitq) > HIETH_MAX_QUEUE_DEPTH) {
        BUG();
        return HDF_FAILURE;
    }
    HiethWritelBits(ld, ld->depth.hwXmitq, UD_REG_NAME(GLB_QLEN_SET), BITS_TXQ_DEP);
    HiethWritelBits(ld, HIETH_MAX_QUEUE_DEPTH - ld->depth.hwXmitq, UD_REG_NAME(GLB_QLEN_SET), BITS_RXQ_DEP);
    return HDF_SUCCESS;
}

int32_t HiethXmitReleasePkt(struct HiethNetdevLocal *ld, const HiethPriv *priv)
{
    int32_t ret = 0;
    struct TxPktInfo *txqCur = NULL;
    int32_t txReclaimCnt = 0;
    struct PbufInfo *pbuf = NULL;

    OsalSpinLockIrq(&(ld->tx_lock));

    while (HwXmitqCntInUse(ld) < ld->txHwCnt) {
        HiethAssert(ld->txHwCnt);

        txqCur = ld->txq + ld->txqTail;
        if (txqCur->txAddr == 0) {
            HDF_LOGE("%s: txAddr is invalid.", __func__);
        }
        pbuf = priv->ram->pbufInfo + ld->txqTail;
        if (pbuf->sgLen != 1) {
            HDF_LOGE("%s: pbuf info sg len is not 1.", __func__);
        }
        pbuf->dmaInfo[0] = NULL;
        NetBufFree(pbuf->buf);

        txqCur->txAddr = 0;

        ld->txqTail++;
        if (ld->txqTail == ld->qSize) {
            ld->txqTail = 0;
        }

        txReclaimCnt++;
        ld->txHwCnt--;
    }

    if (txReclaimCnt && ld->txBusy) {
        ld->txBusy = 0;
        struct HiethPlatformData *hiethPlatformData = GetHiethPlatformData();
        LOS_EventWrite(&(hiethPlatformData[priv->index].stEvent), EVENT_NET_CAN_SEND);
    }

    OsalSpinUnlockIrq(&(ld->tx_lock));
    return ret;
}

int32_t HiethXmitGso(struct HiethNetdevLocal *ld, const HiethPriv *priv, NetBuf *netBuf)
{
    struct TxPktInfo *txqCur = NULL;
    int32_t sendPktLen, sgLen;
    struct PbufInfo *pbInfo = NULL;

    if (netBuf == NULL) {
        HDF_LOGE("%sL netBuf is NULL", __func__);
        return HDF_FAILURE;
    }

    sendPktLen = NetBufGetDataLen(netBuf);
    if (sendPktLen > HIETH_MAX_FRAME_SIZE) {
        HDF_LOGE("%s: xmit error len=%d", __func__, sendPktLen);
    }

    pbInfo = &(priv->ram->pbufInfo[ld->txqHead]);
    sgLen = 0;
    pbInfo->dmaInfo[sgLen] = (void *)NetBufGetAddress(netBuf, E_DATA_BUF);
    sgLen++;
    pbInfo->sgLen = sgLen;
    pbInfo->buf = netBuf;

    txqCur = ld->txq + ld->txqHead;
    txqCur->tx.val = 0;

    /* default config, default closed checksum offload function */
    txqCur->tx.info.tsoFlag = HIETH_CSUM_DISABLE;
    txqCur->tx.info.coeFlag = HIETH_CSUM_DISABLE;
    if (sgLen == 1) {
        txqCur->tx.info.sgFlag = 0;
        NetDmaCacheClean((void *)NetBufGetAddress(netBuf, E_DATA_BUF), sendPktLen);
        txqCur->txAddr = (uintptr_t)NetBufGetAddress(netBuf, E_DATA_BUF);
    } else {
        HDF_LOGE("sg len is not 1");
        NetBufFree(netBuf);
        return HDF_FAILURE;
    }

    txqCur->tx.info.dataLen = sendPktLen + FCS_BYTES;

    HwXmitqPkg(ld, VMM_TO_DMA_ADDR(txqCur->txAddr), txqCur->tx.val);
    ld->txqHead++;
    if (ld->txqHead == ld->qSize) {
        ld->txqHead = 0;
    }
    return HDF_SUCCESS;
}

int32_t HiethFeedHw(struct HiethNetdevLocal *ld, HiethPriv *priv)
{
    int32_t cnt = 0;
    NetBuf *netBuf = NULL;
    uint32_t rxFeedNext;

    OsalSpinLockIrq(&(ld->rx_lock));

    while (HiethReadlBits(ld, UD_REG_NAME(GLB_RO_QUEUE_STAT), BITS_RECVQ_RDY)) {
        rxFeedNext = priv->rxFeed + 1;
        if (rxFeedNext == HIETH_HWQ_RXQ_DEPTH) {
            rxFeedNext = 0;
        }
        if (rxFeedNext == priv->rxRelease) {
            break;
        }

        netBuf = NetBufAlloc(ALIGN(HIETH_MAX_FRAME_SIZE + ETH_PAD_SIZE + CACHE_ALIGNED_SIZE, CACHE_ALIGNED_SIZE));
        if (netBuf == NULL) {
            HDF_LOGE("%sL netBuf alloc fail", __func__);
            break;
        }

        /* drop some bytes for making alignment of net dma cache */
        netBuf->bufs[E_DATA_BUF].offset += (ALIGN((uintptr_t)NetBufGetAddress(netBuf, E_DATA_BUF),
            CACHE_ALIGNED_SIZE) - (uintptr_t)NetBufGetAddress(netBuf, E_DATA_BUF));

#if ETH_PAD_SIZE
        /* drop the padding word */
        netBuf->bufs[E_DATA_BUF].offset += ETH_PAD_SIZE;
#endif

        priv->ram->rxNetbuf[priv->rxFeed] = netBuf;
        NetDmaCacheInv(NetBufGetAddress(netBuf, E_DATA_BUF), HIETH_MAX_FRAME_SIZE);

        HiethWrite(ld, VMM_TO_DMA_ADDR((UINTPTR)NetBufGetAddress(netBuf, E_DATA_BUF)), UD_REG_NAME(GLB_IQ_ADDR));
        priv->rxFeed = rxFeedNext;
        cnt++;
    }

    OsalSpinUnlockIrq(&(ld->rx_lock));
    return cnt;
}
