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

#include "dmac_hi35xx.h"
#include "device_resource_if.h"
#include "dmac_core.h"
#include "hdf_device_desc.h"
#include "hdf_dlist.h"
#include "hdf_log.h"
#include "los_hw.h"
#include "los_hwi.h"
#include "los_vm_phys.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "osal_time.h"

#define HDF_LOG_TAG dmac_hi35xx

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

static struct HiDmacPeripheral g_peripheral[HIDMAC_MAX_PERIPHERALS] = {
    { 0, I2C0_RX_FIFO, HIDMAC_HOST0, 0x40000004, PERI_MODE_8BIT, 0 },
    { 1, I2C0_TX_FIFO, HIDMAC_HOST0, 0x80000004, PERI_MODE_8BIT, 1 },
    { 2, I2C1_RX_FIFO, HIDMAC_HOST0, 0x40000004, PERI_MODE_8BIT, 2 },
    { 3, I2C1_TX_FIFO, HIDMAC_HOST0, 0x80000004, PERI_MODE_8BIT, 3 },
    { 4, I2C2_RX_FIFO, HIDMAC_HOST0, 0x40000004, PERI_MODE_8BIT, 4 },
    { 5, I2C2_TX_FIFO, HIDMAC_HOST0, 0x80000004, PERI_MODE_8BIT, 5 },
    { 6, 0, HIDMAC_NOT_USE, 0, 0, 6 },
    { 7, 0, HIDMAC_NOT_USE, 0, 0, 7 },
    { 8, 0, HIDMAC_NOT_USE, 0, 0, 8 },
    { 9, 0, HIDMAC_NOT_USE, 0, 0, 9 },
    { 10, 0, HIDMAC_NOT_USE, 0, 0, 10 },
    { 11, 0, HIDMAC_NOT_USE, 0, 0, 11 },
    { 12, 0, HIDMAC_NOT_USE, 0, 0, 12 },
    { 13, 0, HIDMAC_NOT_USE, 0, 0, 13 },
    { 14, 0, HIDMAC_NOT_USE, 0, 0, 14 },
    { 15, 0, HIDMAC_NOT_USE, 0, 0, 15 },
    { 16, UART0_RX_ADDR, HIDMAC_HOST0, 0x47700004, PERI_MODE_8BIT, 16 },
    { 17, UART0_TX_ADDR, HIDMAC_HOST0, 0x87700004, PERI_MODE_8BIT, 17 },
    { 18, UART1_RX_ADDR, HIDMAC_HOST0, 0x47700004, PERI_MODE_8BIT, 18 },
    { 19, UART1_TX_ADDR, HIDMAC_HOST0, 0x87700004, PERI_MODE_8BIT, 19 },
    { 20, UART2_RX_ADDR, HIDMAC_HOST0, 0x47700004, PERI_MODE_8BIT, 20 },
    { 21, UART2_TX_ADDR, HIDMAC_HOST0, 0x87700004, PERI_MODE_8BIT, 21 },
    { 22, 0, HIDMAC_NOT_USE, 0, 0, 22 },
    { 23, 0, HIDMAC_NOT_USE, 0, 0, 23 },
    { 24, 0, HIDMAC_NOT_USE, 0, 0, 24 },
    { 25, 0, HIDMAC_NOT_USE, 0, 0, 25 },
    { 26, SPI0_RX_FIFO, HIDMAC_HOST0, 0x40000004, PERI_MODE_8BIT, 26 },
    { 27, SPI0_TX_FIFO, HIDMAC_HOST0, 0x80000004, PERI_MODE_8BIT, 27 },
    { 28, SPI1_RX_FIFO, HIDMAC_HOST0, 0x40000004, PERI_MODE_16BIT, 28 },
    { 29, SPI1_TX_FIFO, HIDMAC_HOST0, 0x80000004, PERI_MODE_16BIT, 29 },
    { 30, SPI2_RX_FIFO, HIDMAC_HOST0, 0x40000004, PERI_MODE_16BIT, 30 },
    { 31, SPI2_TX_FIFO, HIDMAC_HOST0, 0x80000004, PERI_MODE_16BIT, 31 },
}; 
#define HIDMAC_PERIPH_ID_MAX 32

#define ERROR_STATUS_NUM_0   0
#define ERROR_STATUS_NUM_1   1
#define ERROR_STATUS_NUM_2   2
#define ERROR_STATUS_MAX     3

static int HiDmacIsrErrProc(struct DmaCntlr *cntlr, uint16_t chan)
{
    unsigned int chanErrStats[ERROR_STATUS_MAX];

    chanErrStats[ERROR_STATUS_NUM_0] = OSAL_READL(cntlr->remapBase + HIDMAC_INT_ERR1_OFFSET);
    chanErrStats[ERROR_STATUS_NUM_0] = (chanErrStats[ERROR_STATUS_NUM_0] >> chan) & 0x01;
    chanErrStats[ERROR_STATUS_NUM_1] = OSAL_READL(cntlr->remapBase + HIDMAC_INT_ERR2_OFFSET);
    chanErrStats[ERROR_STATUS_NUM_1] = (chanErrStats[ERROR_STATUS_NUM_1] >> chan) & 0x01;
    chanErrStats[ERROR_STATUS_NUM_2] = OSAL_READL(cntlr->remapBase + HIDMAC_INT_ERR3_OFFSET);
    chanErrStats[ERROR_STATUS_NUM_2] = (chanErrStats[ERROR_STATUS_NUM_2] >> chan) & 0x01;
    if ((chanErrStats[ERROR_STATUS_NUM_0] | chanErrStats[ERROR_STATUS_NUM_1] |
        chanErrStats[ERROR_STATUS_NUM_2]) != 0) {
        HDF_LOGE("%s: Err in hidma %d finish, ERR1:0x%x, ERR2:0x%x, ERR3:0x%x", __func__,
            chan, chanErrStats[ERROR_STATUS_NUM_0], chanErrStats[ERROR_STATUS_NUM_1],
            chanErrStats[ERROR_STATUS_NUM_2]);
        OSAL_WRITEL(1 << chan, cntlr->remapBase + HIDMAC_INT_ERR1_RAW_OFFSET);
        HDF_LOGE("HIDMAC_INT_ERR1_RAW = 0x%x", OSAL_READL(cntlr->remapBase + HIDMAC_INT_ERR1_RAW_OFFSET));
        OSAL_WRITEL(1 << chan, cntlr->remapBase + HIDMAC_INT_ERR2_RAW_OFFSET);
        HDF_LOGE("HIDMAC_INT_ERR2_RAW = 0x%x", OSAL_READL(cntlr->remapBase + HIDMAC_INT_ERR2_RAW_OFFSET));
        OSAL_WRITEL(1 << chan, cntlr->remapBase + HIDMAC_INT_ERR3_RAW_OFFSET);
        HDF_LOGE("HIDMAC_INT_ERR3_RAW = 0x%x", OSAL_READL(cntlr->remapBase + HIDMAC_INT_ERR3_RAW_OFFSET));
        return DMAC_CHN_ERROR;
    }
    return 0;
}

static int HiDmacGetChanStat(struct DmaCntlr *cntlr, uint16_t chan)
{
    int ret;
    unsigned long chanStatus;
    unsigned long chanTcStatus;

    chanStatus = OSAL_READL(cntlr->remapBase + HIDMAC_INT_STAT_OFFSET);
    if ((chanStatus >> chan) & 0x1) {
        chanTcStatus = OSAL_READL(cntlr->remapBase + HIDMAC_INT_TC1_OFFSET);
#ifdef DMAC_HI35XX_DEBUG
        HDF_LOGD("%s: HIDMAC_INT_TC1 = 0x%lx, chan = %u", __func__, chanTcStatus, chan);
#endif
        chanTcStatus = (chanTcStatus >> chan) & 0x1;
        if (chanTcStatus != 0) {
            OSAL_WRITEL(chanTcStatus << chan, cntlr->remapBase + HIDMAC_INT_TC1_RAW_OFFSET);
            return  DMAC_CHN_SUCCESS;
        }
        ret = HiDmacIsrErrProc(cntlr, chan);
        if (ret != 0) {
            HDF_LOGE("%s: fail, ret = %d", __func__, ret);
            return DMAC_CHN_ERROR;
        }
        chanTcStatus = OSAL_READL(cntlr->remapBase + HIDMAC_INT_TC2_OFFSET);
#ifdef DMAC_HI35XX_DEBUG
        HDF_LOGD("%s: HIDMAC_INT_TC2 = 0x%lx, chan = %u", __func__, chanTcStatus, chan);
#endif
        chanTcStatus = (chanTcStatus >> chan) & 0x1;
        if (chanTcStatus != 0) {
            OSAL_WRITEL(chanTcStatus << chan, cntlr->remapBase + HIDMAC_INT_TC2_RAW_OFFSET);
        }
    }
    return DMAC_NOT_FINISHED;
}

static unsigned int HiDmacGetPriId(uintptr_t periphAddr, int transType)
{
    unsigned int i;

    /* only check p2m and m2p */
    for (i = transType - 1; i < HIDMAC_MAX_PERIPHERALS; i += 2) {
        if (g_peripheral[i].periphAddr == periphAddr) {
            return i;
        }
    }
    HDF_LOGE("%s: invalid peripheral addr", __func__);
    return HIDMAC_PERIPH_ID_MAX;
}

static uint8_t HiDmacToLocalWitdh(uint32_t transferWitdh, uint32_t defaultWidth)
{
    switch (transferWitdh) {
        case 0x1:
            return PERI_MODE_8BIT;
        case 0x2:
            return PERI_MODE_16BIT;
        case 0x3:
            return PERI_MODE_32BIT;
        case 0x4:
            return PERI_MODE_64BIT;
        default:
            break;
    }
    return defaultWidth;
}

static int32_t HiDmacGetChanInfo(struct DmaCntlr *cntlr, struct DmacChanInfo *chanInfo, struct DmacMsg *msg)
{
    unsigned int periphId;
    uint8_t localSrcWidth;
    uint8_t localDstWidth;

    if (cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    if (chanInfo == NULL || msg == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }

    if (chanInfo->transType == TRASFER_TYPE_P2M || chanInfo->transType == TRASFER_TYPE_M2P) {
        periphId = HiDmacGetPriId(DmacMsgGetPeriphAddr(msg), chanInfo->transType);
        if (periphId >= HIDMAC_PERIPH_ID_MAX) {
            HDF_LOGE("%s: invalid io address", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
        if (msg->srcWidth != msg->destWidth) {
            HDF_LOGE("%s: src width:%u != dest width:%u", __func__, msg->srcWidth, msg->destWidth);
            return HDF_ERR_INVALID_PARAM;
        }
        chanInfo->srcWidth = msg->srcWidth;
        chanInfo->destWidth = chanInfo->srcWidth;
        localSrcWidth = HiDmacToLocalWitdh(chanInfo->srcWidth, g_peripheral[periphId].transWidth);
        localDstWidth = localSrcWidth;
        chanInfo->config = g_peripheral[periphId].transCfg |
                           (localSrcWidth << HIDMAC_SRC_WIDTH_OFFSET) |
                           (localDstWidth << HIDMAC_DST_WIDTH_OFFSET) |
                           (g_peripheral[periphId].dynPeripNum << HIDMAC_PERI_ID_OFFSET);
    }
    chanInfo->lliEnFlag = HIDMAC_LLI_ENABLE;
    return HDF_SUCCESS;
}

static inline void HiDmacDisable(struct DmaCntlr *cntlr, uint16_t channel)
{
    if (cntlr != NULL) {
        OSAL_WRITEL(HIDMAC_CX_DISABLE, cntlr->remapBase + HIDMAC_CX_CFG_OFFSET(channel));
    }
}


#define HIDMAC_32BITS_MASK  0xFFFFFFFF
#define HIDMAC_32BITS_SHIFT 32

static int32_t HiDmacStartM2M(struct DmaCntlr *cntlr, struct DmacChanInfo *chanInfo,
    uintptr_t psrc, uintptr_t pdst, size_t len)
{
    if (cntlr == NULL) {
        HDF_LOGE("%s: invalid cntlr", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    if (chanInfo == NULL || len == 0) {
        HDF_LOGE("%s: chanInfo null or invalid len:%zu", __func__, len);
        return HDF_ERR_INVALID_PARAM;
    }

    OSAL_WRITEL(psrc & HIDMAC_32BITS_MASK, cntlr->remapBase + HIDMAC_CX_SRC_OFFSET_L(chanInfo->channel));
#ifdef LOSCFG_AARCH64
    OSAL_WRITEL((psrc >> HIDMAC_32BITS_SHIFT) & HIDMAC_32BITS_MASK,
        cntlr->remapBase + HIDMAC_CX_SRC_OFFSET_H(chanInfo->channel));
#else
    OSAL_WRITEL(0, cntlr->remapBase + HIDMAC_CX_SRC_OFFSET_H(chanInfo->channel));
#endif

    OSAL_WRITEL(pdst & HIDMAC_32BITS_MASK, cntlr->remapBase + HIDMAC_CX_DST_OFFSET_L(chanInfo->channel));
#ifdef LOSCFG_AARCH64
    OSAL_WRITEL((pdst >> HIDMAC_32BITS_SHIFT) & HIDMAC_32BITS_MASK,
        cntlr->remapBase + HIDMAC_CX_DST_OFFSET_H(chanInfo->channel));
#else
    OSAL_WRITEL(0, cntlr->remapBase + HIDMAC_CX_DST_OFFSET_H(chanInfo->channel));
#endif

    OSAL_WRITEL(0, cntlr->remapBase + HIDMAC_CX_LLI_OFFSET_L(chanInfo->channel));
    OSAL_WRITEL(len, cntlr->remapBase + HIDMAC_CX_CNT0_OFFSET(chanInfo->channel));
    OSAL_WRITEL(HIDMAC_CX_CFG_M2M, cntlr->remapBase + HIDMAC_CX_CFG_OFFSET(chanInfo->channel));
    return HDF_SUCCESS;
}

static int32_t HiDmacStartLli(struct DmaCntlr *cntlr, struct DmacChanInfo *chanInfo)
{
    struct DmacLli *plli = NULL;

    if (cntlr == NULL) {
        HDF_LOGE("%s: invalid cntlr", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    if (chanInfo == NULL || chanInfo->lli == NULL) {
        HDF_LOGE("%s: chanInfo or lli is null", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    plli = chanInfo->lli;

    OSAL_WRITEL(plli->srcAddr & HIDMAC_32BITS_MASK, cntlr->remapBase + HIDMAC_CX_SRC_OFFSET_L(chanInfo->channel));
#ifdef LOSCFG_AARCH64
    OSAL_WRITEL((plli->srcAddr >> HIDMAC_32BITS_SHIFT) & HIDMAC_32BITS_MASK,
        cntlr->remapBase + HIDMAC_CX_SRC_OFFSET_H(chanInfo->channel)); 
#else
    OSAL_WRITEL(0, cntlr->remapBase + HIDMAC_CX_SRC_OFFSET_H(chanInfo->channel)); 
#endif

    OSAL_WRITEL(plli->destAddr & HIDMAC_32BITS_MASK, cntlr->remapBase + HIDMAC_CX_DST_OFFSET_L(chanInfo->channel));
#ifdef LOSCFG_AARCH64
    OSAL_WRITEL((plli->destAddr >> HIDMAC_32BITS_SHIFT) & HIDMAC_32BITS_MASK,
        cntlr->remapBase + HIDMAC_CX_DST_OFFSET_H(chanInfo->channel)); 
#else
    OSAL_WRITEL(0, cntlr->remapBase + HIDMAC_CX_DST_OFFSET_H(chanInfo->channel)); 
#endif

    OSAL_WRITEL(plli->nextLli & HIDMAC_32BITS_MASK, cntlr->remapBase + HIDMAC_CX_LLI_OFFSET_L(chanInfo->channel));
#ifdef LOSCFG_AARCH64
    OSAL_WRITEL((plli->nextLli >> HIDMAC_32BITS_SHIFT) & HIDMAC_32BITS_MASK,
        cntlr->remapBase + HIDMAC_CX_LLI_OFFSET_H(chanInfo->channel));
#else
    OSAL_WRITEL(0, cntlr->remapBase + HIDMAC_CX_LLI_OFFSET_H(chanInfo->channel));
#endif

    OSAL_WRITEL(plli->count, cntlr->remapBase + HIDMAC_CX_CNT0_OFFSET(chanInfo->channel));

    OSAL_WRITEL(plli->config | HIDMAC_CX_CFG_CHN_START, cntlr->remapBase + HIDMAC_CX_CFG_OFFSET(chanInfo->channel));
    return HDF_SUCCESS;
}

static uintptr_t HiDmacGetCurrDstAddr(struct DmaCntlr *cntlr, uint16_t channel)
{
    if (cntlr == NULL || channel >= HIDMAC_CHANNEL_NUM) {
        return 0;
    }
    return OSAL_READL(cntlr->remapBase + HIDMAC_CX_CUR_DST_OFFSET(channel));
}

static void HiDmacClkEn(uint16_t index)
{
    unsigned long tmp;

    if (index == 0) {
        tmp = OSAL_READL(CRG_REG_BASE + HIDMAC_PERI_CRG101_OFFSET);
        tmp |= 1 << HIDMA0_CLK_OFFSET | 1 << HIDMA0_AXI_OFFSET;
        OSAL_WRITEL(tmp, CRG_REG_BASE + HIDMAC_PERI_CRG101_OFFSET);
    }
}

static void HiDmacUnReset(uint16_t index)
{
    unsigned long tmp;

    if (index == 0) {
        tmp = OSAL_READL(CRG_REG_BASE + HIDMAC_PERI_CRG101_OFFSET);
        tmp |= 1 << HIDMA0_RST_OFFSET;
        OSAL_WRITEL(tmp, CRG_REG_BASE + HIDMAC_PERI_CRG101_OFFSET);

        tmp = OSAL_READL(CRG_REG_BASE + HIDMAC_PERI_CRG101_OFFSET);
        tmp &= ~(1 << HIDMA0_RST_OFFSET);
        OSAL_WRITEL(tmp, CRG_REG_BASE + HIDMAC_PERI_CRG101_OFFSET);
    }
}

static int32_t Hi35xxDmacInit(struct DmaCntlr *cntlr)
{
    HDF_LOGI("%s: enter", __func__);

    HiDmacClkEn(cntlr->index);
    HiDmacUnReset(cntlr->index);

    OSAL_WRITEL(HIDMAC_ALL_CHAN_CLR, cntlr->remapBase + HIDMAC_INT_TC1_RAW_OFFSET);
    OSAL_WRITEL(HIDMAC_ALL_CHAN_CLR, cntlr->remapBase + HIDMAC_INT_TC2_RAW_OFFSET);
    OSAL_WRITEL(HIDMAC_ALL_CHAN_CLR, cntlr->remapBase + HIDMAC_INT_ERR1_RAW_OFFSET);
    OSAL_WRITEL(HIDMAC_ALL_CHAN_CLR, cntlr->remapBase + HIDMAC_INT_ERR2_RAW_OFFSET);
    OSAL_WRITEL(HIDMAC_ALL_CHAN_CLR, cntlr->remapBase + HIDMAC_INT_ERR3_RAW_OFFSET);
    OSAL_WRITEL(HIDMAC_INT_ENABLE_ALL_CHAN, cntlr->remapBase + HIDMAC_INT_TC1_MASK_OFFSET);
    OSAL_WRITEL(HIDMAC_INT_ENABLE_ALL_CHAN, cntlr->remapBase + HIDMAC_INT_TC2_MASK_OFFSET);
    OSAL_WRITEL(HIDMAC_INT_ENABLE_ALL_CHAN, cntlr->remapBase + HIDMAC_INT_ERR1_MASK_OFFSET);
    OSAL_WRITEL(HIDMAC_INT_ENABLE_ALL_CHAN, cntlr->remapBase + HIDMAC_INT_ERR2_MASK_OFFSET);
    OSAL_WRITEL(HIDMAC_INT_ENABLE_ALL_CHAN, cntlr->remapBase + HIDMAC_INT_ERR3_MASK_OFFSET);
    return HDF_SUCCESS;
}

static uintptr_t HiDmacVaddrToPaddr(void *vaddr)
{
    return (uintptr_t)LOS_PaddrQuery(vaddr);
}

static void *HiDmacPaddrToVaddr(uintptr_t paddr)
{
    return LOS_PaddrToKVaddr((paddr_t)paddr);
}

static void HiDmacCacheInv(uintptr_t vaddr, uintptr_t vend)
{
    if (vaddr == 0 || vend == 0) {
        return;
    }
    DCacheInvRange(vaddr, vend);
}

static void HiDmacCacheFlush(uintptr_t vaddr, uintptr_t vend)
{
    if (vaddr == 0 || vend == 0) {
        return;
    }
    DCacheFlushRange(vaddr, vend);
}

static int32_t HiDmacParseHcs(struct DmaCntlr *cntlr, const struct DeviceResourceNode *node)
{
    int32_t ret;
    struct DeviceResourceIface *drsOps = NULL;

    if (cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL || drsOps->GetUint16 == NULL) {
        HDF_LOGE("%s: invalid drs ops", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    ret = drsOps->GetUint32(node, "reg_pbase", &cntlr->phyBase, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read reg base fail", __func__);
        return ret;
    }
    cntlr->remapBase = OsalIoRemap((unsigned long)cntlr->phyBase, (unsigned long)cntlr->regSize);

    ret = drsOps->GetUint32(node, "reg_size", &cntlr->regSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read reg size fail", __func__);
        return ret;
    }
    ret = drsOps->GetUint16(node, "index", &cntlr->index, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read index fail", __func__);
        return ret;
    }
    ret = drsOps->GetUint32(node, "irq", &cntlr->irq, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read irq fail", __func__);
        return ret;
    }
    ret = drsOps->GetUint32(node, "max_transfer_size", &cntlr->maxTransSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read max transfer size fail", __func__);
        return ret;
    }
    ret = drsOps->GetUint16(node, "channel_num", &cntlr->channelNum, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read channel num fail", __func__);
        return ret;
    }
    HDF_LOGI("phybase:0x%x, size:0x%x, index:0x%x, irq:0x%x, max_trans_size:0x%x, chan_num:0x%x",
        cntlr->phyBase, cntlr->regSize, cntlr->index, cntlr->irq, cntlr->maxTransSize, cntlr->channelNum);
    return HDF_SUCCESS;
}

static int32_t HiDmacBind(struct HdfDeviceObject *device)
{
    struct DmaCntlr *cntlr = NULL;

    cntlr = DmaCntlrCreate(device);
    if (cntlr == NULL) {
        return HDF_ERR_MALLOC_FAIL;
    }
    cntlr->getChanInfo = HiDmacGetChanInfo;
    cntlr->dmacCacheFlush = HiDmacCacheFlush;
    cntlr->dmacCacheInv = HiDmacCacheInv;
    cntlr->dmaChanEnable = HiDmacStartLli;
    cntlr->dmaM2mChanEnable = HiDmacStartM2M;
    cntlr->dmacChanDisable = HiDmacDisable;
    cntlr->dmacVaddrToPaddr = HiDmacVaddrToPaddr;
    cntlr->dmacPaddrToVaddr = HiDmacPaddrToVaddr;
    cntlr->dmacGetChanStatus = HiDmacGetChanStat;
    cntlr->dmacGetCurrDestAddr = HiDmacGetCurrDstAddr;
    device->service = &cntlr->service;
    return HDF_SUCCESS;
}

static int32_t HiDmacInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct DmaCntlr *cntlr = NULL;

    if (device == NULL || device->property == NULL) {
        HDF_LOGE("%s: device or property null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    cntlr = CONTAINER_OF(device->service, struct DmaCntlr, service);
    ret = HiDmacParseHcs(cntlr, device->property);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    ret = Hi35xxDmacInit(cntlr);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    return DmacCntlrAdd(cntlr);
}

static void HiDmacRelease(struct HdfDeviceObject *device)
{
    struct DmaCntlr *cntlr = NULL;

    if (device == NULL) {
        return;
    }
    cntlr = CONTAINER_OF(device->service, struct DmaCntlr, service);
    DmacCntlrRemove(cntlr);
    DmaCntlrDestroy(cntlr);
}

struct HdfDriverEntry g_hiDmacEntry = {
    .moduleVersion = 1,
    .Bind = HiDmacBind,
    .Init = HiDmacInit,
    .Release = HiDmacRelease,
    .moduleName = "HDF_PLATFORM_DMAC",
};
HDF_INIT(g_hiDmacEntry);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
