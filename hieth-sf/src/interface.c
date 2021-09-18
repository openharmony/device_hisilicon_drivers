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

#include "hieth_phy.h"
#include "mdio.h"

#include "string.h"
#include "eth_drv.h"
#include "ctrl.h"
#include "asm/dma.h"
#include "sys/bus.h"

#define PHY_ADDR_SET     0x129B5B3
#define PHY_ADDR_NOT_SET 0x5a5aa5a5
#define INVALID_PHY_ADDR 0x12F358F
#define PHY_MODE_SET     0x12B63D0
#define PHY_MODE_NOT_SET 0x5a5aa5a5
#define INVALID_PHY_MODE 0x12F5D09

#define ETH_STACKSIZE 0x20000

#define PHY_ID_INVALID(id) (((id & 0x1fffffff) == 0x1fffffff) ||  \
                            ((id & 0xfffffff0) == 0xfffffff0) ||  \
                            (id == 0) ||  \
                            (id == 0xffff0000) ||  \
                            (id == 0x0000ffff))

static int32_t g_userSetPhyAddr = PHY_ADDR_NOT_SET;
static int32_t g_phyAddrVal = INVALID_PHY_ADDR;
static int32_t g_userSetPhyMode = PHY_MODE_NOT_SET;
static int32_t g_phyModeVal = INVALID_PHY_MODE;

struct HiethPlatformData *g_stHiethPlatformData = NULL;
OSAL_DECLARE_SPINLOCK(hiethGlbRegLock);
extern unsigned long msecs_to_jiffies(const uint32_t msecs);
#define TIME_MEDIUM 100
#define TIME_LONG 300
#define EXCESS_SIZE 4

static bool IsLinkUp(struct EthDevice *ethDevice)
{
    NetIfLinkStatus status = NETIF_LINK_DOWN;

    if (NetIfGetLinkStatus(ethDevice->netdev, &status) != 0) {
        HDF_LOGE("%s: net device is invalid", __func__);
        return false;
    }
    if (status == NETIF_LINK_UP) {
        return true;
    } else {
        return false;
    }
}

static void RestartTimer(OsalTimer *timer, uint32_t interval, OsalTimerFunc func, uintptr_t arg)
{
    (void)OsalTimerDelete(timer);
    if (OsalTimerCreate(timer, interval, func, arg) != HDF_SUCCESS) {
        HDF_LOGE("create timer failed");
    } else if (OsalTimerStartOnce(timer) != HDF_SUCCESS) {
        HDF_LOGE("%s: start timer failed", __func__);
    }
}

static void PhyStateMachine(uintptr_t arg)
{
    struct EthDevice *ethDevice = (struct EthDevice *)arg;
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;
    struct HiethNetdevLocal *ld = &(g_stHiethPlatformData[priv->index].stNetdevLocal);
    int32_t linkStatus;

    linkStatus = MiiphyLink(ld, priv->phy);
    if (IsLinkUp(ethDevice) && !linkStatus) {
        NetIfSetLinkStatus(ethDevice->netdev, NETIF_LINK_DOWN);
    } else if (!IsLinkUp(ethDevice) && linkStatus) {
        NetIfSetLinkStatus(ethDevice->netdev, NETIF_LINK_UP);
    }
}

static void HiethMonitorFunc(uintptr_t arg)
{
    struct EthDevice *ethDevice = (struct EthDevice *)arg;
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;
    struct HiethNetdevLocal *ld = &(g_stHiethPlatformData[priv->index].stNetdevLocal);
    int32_t refillCnt;

    refillCnt = HiethFeedHw(ld, priv);
    if (!refillCnt) {
        RestartTimer(&priv->monitorTimer, HIETH_MONITOR_TIME, HiethMonitorFunc, (uintptr_t)ethDevice);
    }
}

static void EthDrvRecv(struct EthDevice *ethDevice, NetBuf *netBuf)
{
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;

    priv->rxRelease++;
    if (priv->rxRelease == HIETH_HWQ_RXQ_DEPTH) {
        priv->rxRelease = 0;
    }
    NetDmaCacheInv(NetBufGetAddress(netBuf, E_DATA_BUF), netBuf->len);
    NetBufPush(netBuf, E_DATA_BUF, netBuf->len);
    NetIfRxNi(ethDevice->netdev, netBuf);
}

void UnRegisterTimerFunction(struct EthDevice *ethDevice)
{
    struct EthDrvSc *drvInfo = (struct EthDrvSc *)(ethDevice->priv);
    HiethPriv *priv = (HiethPriv *)drvInfo->driverPrivate;

    (void)OsalTimerDelete(&priv->phyTimer);
    (void)OsalTimerDelete(&priv->monitorTimer);
}

void NetDmaCacheInv(void *addr, uint32_t size)
{
    uint32_t start = (uintptr_t)addr & ~(CACHE_ALIGNED_SIZE - 1);
    uint32_t end = (uintptr_t)addr + size;

    end = ALIGN(end, CACHE_ALIGNED_SIZE);
    DCacheInvRange(start, end);
}

void NetDmaCacheClean(void *addr, uint32_t size)
{
    uint32_t start = (uintptr_t)addr & ~(CACHE_ALIGNED_SIZE - 1);
    uint32_t end = (uintptr_t)addr + size;

    end = ALIGN(end, CACHE_ALIGNED_SIZE);
    DCacheFlushRange(start, end);
}

static uint32_t HisiEthIsr(uint32_t irq, void *data)
{
    (void)irq;
    struct EthDrvSc *drvSc = (struct EthDrvSc *)data;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;

    OsalDisableIrq(priv->vector); // 禁止一个irq；
    LOS_EventWrite(&(g_stHiethPlatformData[priv->index].stEvent), EVENT_NET_TX_RX);
    return HDF_SUCCESS;
}

static int32_t HisiEthDsr(void *arg)
{
    uint32_t uwRet;
    struct EthDevice *ethDevice = (struct EthDevice *)arg;
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;

    while (true) {
        uwRet = LOS_EventRead(&(g_stHiethPlatformData[priv->index].stEvent), EVENT_NET_TX_RX,
                              LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
        if (uwRet & EVENT_NET_TX_RX) {
            (drvSc->funs->deliver)(ethDevice);
        }
    }
    return HDF_SUCCESS;
}

static int32_t CreateEthIrqThread(struct EthDevice *ethDevice)
{
    struct OsalThread thread;
    struct OsalThreadParam para = {
        .name = "eth_irq_Task",
        .stackSize = ETH_STACKSIZE,
        .priority = OSAL_THREAD_PRI_HIGHEST,
    };

    if (OsalThreadCreate(&thread, HisiEthDsr, (void *)ethDevice) != HDF_SUCCESS) {
        HDF_LOGE("create isr thread failed");
        return HDF_FAILURE;
    }
    if (OsalThreadStart(&thread, &para) != HDF_SUCCESS) {
        HDF_LOGE("isr thread start failed");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static uint32_t ScanPhyId(struct HiethNetdevLocal *ld, int32_t addr)
{
    uint32_t phyId, val;

    val = (uint32_t)HiethMdioRead(ld, addr, PHY_ID1);
    phyId = val << MAC_ADDR_OFFSET_L16;
    val = (uint32_t)HiethMdioRead(ld, addr, PHY_ID2);
    phyId |= val;
    return phyId;
}

static int32_t HiethCanSend(struct EthDevice *ethDevice)
{
    int32_t canSend;
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;
    struct HiethNetdevLocal *ld = &(g_stHiethPlatformData[priv->index].stNetdevLocal);
    uint32_t txqHeadNext;

    if (!TestXmitQueueReady(ld)) {
        HiethXmitReleasePkt(ld, priv);
    }

    txqHeadNext = ld->txqHead + 1;
    if (txqHeadNext == ld->qSize) {
        txqHeadNext = 0;
    }

    OsalSpinLockIrq(&(ld->tx_lock));
    if (!TestXmitQueueReady(ld) ||
        txqHeadNext == ld->txqTail) {
        uint32_t uwRet;
        canSend = 0;
        ld->txBusy = 1;
        OsalSpinUnlockIrq(&(ld->tx_lock));
        OsalEnableIrq(priv->vector);
        uwRet = LOS_EventRead(&(g_stHiethPlatformData[priv->index].stEvent),
                              EVENT_NET_CAN_SEND, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, msecs_to_jiffies(40));
        if (uwRet & EVENT_NET_CAN_SEND) {
            canSend = 1;
        }
        OsalDisableIrq(priv->vector);
        OsalSpinLockIrq(&(ld->tx_lock));
    } else {
        canSend = 1;
    }
    OsalSpinUnlockIrq(&(ld->tx_lock));
    return canSend;
}

static void HiethSend(struct EthDevice *ethDevice, NetBuf *netBuf)
{
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;
    struct HiethNetdevLocal *ld = &(g_stHiethPlatformData[priv->index].stNetdevLocal);

    OsalSpinLockIrq(&(ld->tx_lock));
    /* TSO supported */
    HiethXmitGso(ld, priv, netBuf);
    ld->txHwCnt++;
    OsalSpinUnlockIrq(&(ld->tx_lock));
}

static void HiethDeliver(struct EthDevice *ethDevice)
{
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;
    struct HiethNetdevLocal *ld = &(g_stHiethPlatformData[priv->index].stNetdevLocal);
    uint32_t ints;
    int32_t refillCnt;
    uint32_t rxPktInfo;
    uint32_t rlen;
    NetBuf *netBuf = NULL;

    /* mask the all interrupt */
    /* Add lock for multi-processor */
    OsalSpinLockIrq(&hiethGlbRegLock);
    HiethWritelBits(ld, 0, GLB_RW_IRQ_ENA, BITS_IRQS_ENA_ALLPORT);
    OsalSpinUnlockIrq(&hiethGlbRegLock);

    HiethXmitReleasePkt(ld, priv);
    HiethClearIrqstatus(ld, UD_BIT_NAME(HIETH_INT_TXQUE_RDY));

    ints = HiethReadIrqstatus(ld);
    if (ints & BITS_IRQS_MASK_U) {
        if ((ints & UD_BIT_NAME(HIETH_INT_MULTI_RXRDY))) {
            while (IsRecvPacket(ld)) {
                rxPktInfo = HwGetRxpkgInfo(ld);
                rlen = (rxPktInfo >> BITS_RXPKG_LEN_OFFSET) 
                       & BITS_RXPKG_LEN_MASK;
                rlen -= EXCESS_SIZE;
                if (rlen > HIETH_MAX_FRAME_SIZE) {
                    HDF_LOGE("ERROR: recv len=%d", rlen);
                }

                OsalSpinLockIrq(&hiethGlbRegLock);
                HwSetRxpkgFinish(ld);
                OsalSpinUnlockIrq(&hiethGlbRegLock);

                OsalSpinLockIrq(&(ld->rx_lock));
                netBuf = priv->ram->rxNetbuf[priv->rxRelease];
                netBuf->len = rlen;
                OsalSpinUnlockIrq(&(ld->rx_lock));
                EthDrvRecv(ethDevice, netBuf);
            }

            refillCnt = HiethFeedHw(ld, priv);
            if (!refillCnt && (priv->rxRelease == priv->rxFeed)) {
                RestartTimer(&priv->monitorTimer, HIETH_MONITOR_TIME, HiethMonitorFunc, (uintptr_t)ethDevice);
            }
        }
        HiethClearIrqstatus(ld, (ints & BITS_IRQS_MASK_U));
        ints &= ~BITS_IRQS_MASK_U;
    }

    if (ints & HIETH_INT_TX_ERR_U) {
        ints &= ~HIETH_INT_TX_ERR_U;
        HDF_LOGE("HiethDeliver ERROR: HIETH_INT_TX_ERR_U.\n");
    }

    if (ints) {
        HDF_LOGE("unknown ints=0x%.8x", ints);
        HiethClearIrqstatus(ld, ints);
    }

    /* unmask the all interrupt */
    OsalSpinLockIrq(&hiethGlbRegLock);
    HiethWritelBits(ld, 1, GLB_RW_IRQ_ENA, BITS_IRQS_ENA_ALLPORT);
    OsalSpinUnlockIrq(&hiethGlbRegLock);

    OsalEnableIrq(priv->vector);
#ifdef INT_IO_ETH_INT_SUPPORT_REQUIRED
    drv_interrupt_unmask(priv->vector);
#endif
}

static int32_t HiethIntVector(struct EthDevice *ethDevice)
{
    return NUM_HAL_INTERRUPT_ETH;
}

void EthDrvSend(struct EthDevice *ethDevice, NetBuf *netBuf)
{
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;

    OsalDisableIrq(priv->vector);
    if (!(drvSc->funs->canSend)(ethDevice)) {
        OsalEnableIrq(priv->vector);
        return;
    }
    (drvSc->funs->send)(ethDevice, netBuf);
    OsalEnableIrq(priv->vector);
}

static const char *GetPhySpeedString(int32_t speed)
{
    switch (speed) {
        case PHY_SPEED_10:
            return "10Mbps";
        case PHY_SPEED_100:
            return "100Mbps";
        case PHY_SPEED_1000:
            return "1Gbps";
        case PHY_SPEED_UNKNOWN:
            return "Unknown";
        default:
            return "Unsupported";
    }
}

void HiethLinkStatusChanged(struct EthDevice *ethDevice)
{
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;
    struct HiethNetdevLocal *ld = &(g_stHiethPlatformData[priv->index].stNetdevLocal);
    unsigned long val = 0;
    int32_t phyMode = 0;
    int32_t duplex;
    int32_t speed;

    duplex = MiiphyDuplex(ld, priv->phy);
    speed = MiiphySpeed(ld, priv->phy);

    if (IsLinkUp(ethDevice)) {
        val |= HIETH_LINKED;
    }

    if (duplex) {
        val |= HIETH_DUP_FULL;
    }

    if (speed == PHY_SPEED_100) {
        val |= HIETH_SPD_100M;
    }

    switch (priv->phy->phyMode) {
        case PHY_INTERFACE_MODE_MII:
            phyMode = HIETH_PHY_MII_MODE;
            break;
        case PHY_INTERFACE_MODE_RMII:
            phyMode = HIETH_PHY_RMII_MODE;
            break;
        default:
            HDF_LOGE("not supported mode: %d", priv->phy->phyMode);
            break;
    }

    HiethSetLinkStat(ld, val);
    HiethSetMiiMode(ld, phyMode);

    if (IsLinkUp(ethDevice)) {
        PRINTK("Link is Up - %s/%s\n", GetPhySpeedString(speed), (PHY_DUPLEX_FULL == duplex) ? "Full" : "Half");
    } else {
        PRINTK("Link is Down\n");
    }
}

uint8_t HiethSetHwaddr(struct EthDevice *ethDevice, uint8_t *addr, uint8_t len)
{
    HiethPriv *priv = (HiethPriv *)((struct EthDrvSc *)ethDevice->priv)->driverPrivate;

    if (IsMulticastEtherAddr(addr)) {
        HDF_LOGE("WARN: config a muticast mac address, please check!");
        return HDF_FAILURE;
    }

    if (len != ETHER_ADDR_LEN) {
        HDF_LOGE("WARN: config wrong mac address len=%u", len);
        return HDF_FAILURE;
    }

    HiethHwSetMacAddress(&(g_stHiethPlatformData[priv->index].stNetdevLocal), 1, addr);
    return HDF_SUCCESS;
}

int32_t HisiEthSetPhyMode(const char *phyMode)
{
    int32_t i;

    for (i = 0; i < PHY_INTERFACE_MODE_MAX; i++) {
        if (!strcasecmp(phyMode, PhyModes(i))) {
            g_userSetPhyMode = PHY_MODE_SET;
            g_phyModeVal = i;
            return HDF_SUCCESS;
        }
    }
    return HDF_FAILURE;
}

bool HiethHwInit(struct EthDevice *ethDevice)
{
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;
    struct HiethNetdevLocal *ld = &(g_stHiethPlatformData[priv->index].stNetdevLocal);
    uint32_t phyState = 0;
    uint32_t id;
    int32_t addr;

    OsalSpinInit(&hiethGlbRegLock);

    ld->txq = priv->ram->txqInfo;
    ld->iobase = (char *)((uintptr_t)priv->base);

    HiethHwMacCoreInit(ld);

    if (g_userSetPhyMode == PHY_MODE_SET) {
        priv->phy->phyMode = g_phyModeVal;
        HDF_LOGE("hisi_eth: User set phy mode=%s", PhyModes(priv->phy->phyMode));
    } else {
        priv->phy->phyMode = ld->phyMode;
        HDF_LOGE("hisi_eth: User did not set phy mode, use default=%s", PhyModes(priv->phy->phyMode));
    }

    if (!priv->phy->initDone) {
        HiethHwExternalPhyReset();
        mdelay(TIME_LONG);
        priv->phy->initDone = true;

        if (g_userSetPhyAddr == PHY_ADDR_SET) {
            priv->phy->phyAddr = g_phyAddrVal;
            HDF_LOGE("hisi_eth: User set phy addr=%d", priv->phy->phyAddr);

            id = ScanPhyId(ld, priv->phy->phyAddr);
            if (PHY_ID_INVALID(id)) {
                HDF_LOGE("Can't find PHY device - id: %x", id);
                priv->phy->initDone = false;
                goto ERR_OUT;
            }
        } else {
            HDF_LOGE("hisi_eth: User did not set phy addr, auto scan...");

            for (addr = MAX_PHY_ADDR; addr >= 0; addr--) {
                id = ScanPhyId(ld, addr);
                if (PHY_ID_INVALID(id)) {
                    continue;
                }
                break;
            }

            if (addr < 0) {
                HDF_LOGE("Can't find PHY device - id: %x", id);
                priv->phy->initDone = false;
                goto ERR_OUT;
            }

            priv->phy->phyAddr = addr;
        }
        ld->phyId = id;
    }

    HiethHwExternalPhyReset();
    mdelay(TIME_MEDIUM);
    HiethFephyTrim(ld, priv->phy);
    HDF_LOGE("Detected phy addr %d, phyid: 0x%x.", priv->phy->phyAddr, ld->phyId);

    if (!priv->phy->initDone) {
        goto ERR_OUT;
    }

    HiethGetPhyStat(ld, priv->phy, &phyState);

    if (OsalRegisterIrq(priv->vector, OSAL_IRQF_TRIGGER_NONE, HisiEthIsr, "ETH", (void *)drvSc) != HDF_SUCCESS) {
        HDF_LOGE("register irq failed");
        goto ERR_OUT;
    }
    OsalEnableIrq(priv->vector);
    return true;
ERR_OUT:
    return false;
}

void RegisterHiethData(struct EthDevice *ethDevice)
{
    int32_t ret;
    uint32_t data;

    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;
    struct HiethNetdevLocal *ld = &(g_stHiethPlatformData[priv->index].stNetdevLocal);
    ret = CreateEthIrqThread(ethDevice);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("create eth thread failed");
        return;
    }
    /* clear all interrupts */
    HiethClearIrqstatus(ld, UD_BIT_NAME(BITS_IRQS_MASK));
    priv->rxFeed = 0;
    priv->rxRelease = 0;
    HiethFeedHw(ld, priv);

    if (OsalTimerCreate(&priv->phyTimer, PHY_STATE_TIME, PhyStateMachine, (uintptr_t)ethDevice) != HDF_SUCCESS) {
        HDF_LOGE("create phy state machine timer failed");
        return;
    }
    if (OsalTimerStartLoop(&priv->phyTimer) != HDF_SUCCESS) {
        HDF_LOGE("start phy state machine timer failed");
        return;
    }

    HiethIrqEnable(ld, UD_BIT_NAME(HIETH_INT_MULTI_RXRDY) | UD_BIT_NAME(HIETH_INT_TXQUE_RDY));
    HiethWritelBits(ld, 1, GLB_RW_IRQ_ENA, UD_BIT_NAME(BITS_IRQS_ENA));
    HiethWritelBits(ld, 1, GLB_RW_IRQ_ENA, BITS_IRQS_ENA_ALLPORT);
#ifdef HIETH_TSO_SUPPORTED
    HiethIrqEnable(ld, UD_BIT_NAME(HIETH_INT_TX_ERR));
#endif

    data = readl(ld->iobase + 0x210);
    data |= 0x40000000; /* do CRC check in mac */
    writel(data, ld->iobase + 0x210);
}

static struct EthHwrFuns g_stEthnetDrvFun = {
    .canSend = HiethCanSend,
    .send = HiethSend,
    .deliver = HiethDeliver,
    .intVector = HiethIntVector,
};

void InitEthnetDrvFun(struct EthDrvSc *drvFun)
{
    if (drvFun == NULL) {
        HDF_LOGE("%s: input is NULL!", __func__);
        return;
    }
    drvFun->funs = &g_stEthnetDrvFun;
}

int32_t HiethInit(struct EthDevice *ethDevice)
{
    if (ethDevice == NULL) {
        HDF_LOGE("%s input is NULL!", __func__);
        return HDF_FAILURE;
    }

    g_stHiethPlatformData = (struct HiethPlatformData *)OsalMemCalloc(sizeof(struct HiethPlatformData));
    if (!g_stHiethPlatformData) {
        HDF_LOGE("EthDrvSc OsalMemCalloc HiethPlatformData error!");
        return HDF_FAILURE;
    }
    OsalSpinInit(&(g_stHiethPlatformData[0].stNetdevLocal.tx_lock));
    OsalSpinInit(&(g_stHiethPlatformData[0].stNetdevLocal.rx_lock));

    struct ConfigEthDevList *config = ethDevice->config;
    g_stHiethPlatformData[0].stNetdevLocal.port = config->port;
    g_stHiethPlatformData[0].stNetdevLocal.depth.hwXmitq = config->hwXmitq;
    g_stHiethPlatformData[0].stNetdevLocal.qSize = config->qSize;
    g_stHiethPlatformData[0].stNetdevLocal.mdioFrqdiv = config->ethMac.mdioFrqDiv;
    g_stHiethPlatformData[0].stNetdevLocal.txBusy = config->ethMac.txBusy;
    g_stHiethPlatformData[0].stNetdevLocal.phyMode = config->ethPhy.phyMode;
    (void)LOS_EventInit(&(g_stHiethPlatformData[0].stEvent));

    HiethHwInit(ethDevice);
    return HDF_SUCCESS;
}

struct HiethPlatformData *GetHiethPlatformData(void)
{
    return g_stHiethPlatformData;
}

struct HiethNetdevLocal *GetHiethNetDevLocal(struct EthDevice *ethDevice)
{
    struct HiethNetdevLocal *ld = NULL;
    if (ethDevice == NULL) {
        HDF_LOGE("%s input is NULL", __func__);
        return NULL;
    }
    struct EthDrvSc *drvSc = (struct EthDrvSc *)ethDevice->priv;
    HiethPriv *priv = (HiethPriv *)drvSc->driverPrivate;
    ld = &(g_stHiethPlatformData[priv->index].stNetdevLocal);
    if (ld == NULL) {
        HDF_LOGE("%s get HiethNetdevLocal fail", __func__);
        return NULL;
    }
    return ld;
}

int ethnet_hieth_init(struct EthDevice *ethDevice)
{
    return HDF_SUCCESS;
}

void get_defaultNetif(struct netif **pnetif, struct EthDrvSc *drvSc)
{
    (void)pnetif;
    (void)drvSc;
}
