/*
 * Copyright (c) 2020 HiSilicon (Shanghai) Technologies CO., LIMITED.
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

#include "device_resource_if.h"
#include "hdf_base.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "osal_time.h"
#include "hdf_log.h"
#include "spi_core.h"
#include "spi_dev.h"
#include "spi_hi35xx.h"

#define HDF_LOG_TAG spi_hi35xx

struct HiPl022 {
    struct SpiCntlr *cntlr;
    struct DListHead deviceList;
    volatile unsigned char *regBase;
    uint32_t busNum;
    uint32_t numCs;
    uint32_t curCs;
    uint32_t speed;
    uint32_t fifoSize;
    uint32_t clkRate;
    uint32_t maxSpeedHz;
    uint32_t minSpeedHz;
    uint32_t regCrg;
    uint32_t clkEnBit;
    uint32_t clkRstBit;
    uint32_t regMiscCtrl;
    uint32_t miscCtrlCsShift;
    uint32_t miscCtrlCs;
    uint16_t mode;
    uint8_t bitsPerWord;
    uint8_t transferMode;
};

static int32_t SpiCfgCs(struct HiPl022 *pl022, uint32_t cs)
{
    uint32_t value;

    if ((cs + 1) > pl022->numCs) {
        HDF_LOGE("%s: cs %d is big than pl022 csNum %d", __func__, cs, pl022->numCs);
        return HDF_FAILURE;
    }
    if (pl022->numCs == 1) {
        return HDF_SUCCESS;
    }
    value = OSAL_READL(pl022->regMiscCtrl);
    value &= ~pl022->miscCtrlCs;
    value |= (cs << pl022->miscCtrlCsShift);
    OSAL_WRITEL(value, pl022->regMiscCtrl);
    return 0;
}

static int32_t SpiHwInitCfg(struct HiPl022 *pl022)
{
    uint32_t value;

    value = OSAL_READL(pl022->regCrg);
    value &= ~pl022->clkRstBit;
    value |= pl022->clkEnBit;
    OSAL_WRITEL(value, pl022->regCrg); /* open spi clk */
    return 0;
}

static int32_t SpiHwExitCfg(struct HiPl022 *pl022)
{
    uint32_t value;

    value = OSAL_READL(pl022->regCrg);
    value |= pl022->clkRstBit;
    value &= ~pl022->clkEnBit;
    OSAL_WRITEL(value, pl022->regCrg); /* close spi clk */
    return 0;
}

static void HiPl022Enable(const struct HiPl022 *pl022)
{
    uint32_t value;

    value = OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_CR1);
    value |= SPI_CR1_SSE;
    OSAL_WRITEL(value, (UINTPTR)(pl022->regBase) + REG_SPI_CR1);
}

static void HiPl022Disable(const struct HiPl022 *pl022)
{
    uint32_t value;

    value = OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_CR1);
    value &= ~SPI_CR1_SSE;
    OSAL_WRITEL(value, (UINTPTR)(pl022->regBase) + REG_SPI_CR1);
}

static void HiPl022ConfigCPSR(const struct HiPl022 *pl022, uint32_t cpsdvsr)
{
    uint32_t value;

    value = OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_CPSR);
    value &= ~SPI_CPSR_CPSDVSR;
    value |= cpsdvsr << SPI_CPSR_CPSDVSR_SHIFT;
    OSAL_WRITEL(value, (UINTPTR)(pl022->regBase) + REG_SPI_CPSR);
}

static void HiPl022ConfigCR0(const struct HiPl022 *pl022, uint32_t scr)
{
    uint32_t tmp;
    uint32_t value;

    value = OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_CR0);
    value &= ~SPI_CR0_DSS;
    value |= (pl022->bitsPerWord - 1) << SPI_CR0_DSS_SHIFT;
    value &= ~SPI_CR0_FRF;
    value &= ~SPI_CR0_SPO;
    tmp = (!!(pl022->mode & SPI_CLK_POLARITY)) ? (1 << SPI_CR0_SPO_SHIFT) : 0;
    value |= tmp;
    value &= ~SPI_CR0_SPH;
    tmp = (!!(pl022->mode & SPI_CLK_PHASE)) ? (1 << SPI_CR0_SPH_SHIFT) : 0;
    value |= tmp;
    value &= ~SPI_CR0_SCR;
    value |= (scr << SPI_CR0_SCR_SHIFT);
    OSAL_WRITEL(value, (UINTPTR)(pl022->regBase) + REG_SPI_CR0);
}

static void HiPl022ConfigCR1(const struct HiPl022 *pl022)
{
    uint32_t tmp;
    uint32_t value;

    value = OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_CR1);
    value &= ~SPI_CR1_LBN;
    tmp = (!!(pl022->mode & SPI_MODE_LOOP)) ? (1 << SPI_CR1_LBN_SHIFT) : 0;
    value |= tmp;
    value &= ~SPI_CR1_MS;
    value &= ~SPI_CR1_BIG_END;
    tmp = (!!(pl022->mode & SPI_MODE_LSBFE)) ? (1 << SPI_CR1_BIG_END_SHIFT) : 0;
    value |= tmp;
    value &= ~SPI_CR1_ALT;
    value |= 0x1 << SPI_CR1_ALT_SHIFT;
    OSAL_WRITEL(value, (UINTPTR)(pl022->regBase) + REG_SPI_CR1);
}

static int HiPl022Config(struct HiPl022 *pl022)
{
    uint32_t tmp;
    uint32_t scr;
    uint32_t cpsdvsr;

    HiPl022Disable(pl022);
    /* Check if we can provide the requested rate */
    if (pl022->speed > pl022->maxSpeedHz) {
        pl022->speed = pl022->maxSpeedHz;
    }
    /* Min possible */
    if (pl022->speed < pl022->minSpeedHz) {
        HDF_LOGE("%s: pl022->speed is %d not support", __func__, pl022->speed);
        return HDF_FAILURE;
    }
    /* Check if we can provide the requested bits_per_word */
    if ((pl022->bitsPerWord < BITS_PER_WORD_MIN) || (pl022->bitsPerWord > BITS_PER_WORD_MAX)) {
        HDF_LOGE("%s: pl022->bitsPerWord is %d not support", __func__, pl022->bitsPerWord);
        return HDF_FAILURE;
    }
    /* compute spi speed, speed=clk/(cpsdvsr*(scr+1)) */
    tmp = (pl022->clkRate) / (pl022->speed);
    if (tmp < CPSDVSR_MIN) {
        cpsdvsr = CPSDVSR_MIN;
        scr = 0;
    } else if (tmp <= CPSDVSR_MAX) {
        cpsdvsr = tmp & (~0x1);
        scr = (tmp / cpsdvsr) - 1;
    } else {
        cpsdvsr = CPSDVSR_MAX;
        scr = (tmp / cpsdvsr) - 1;
    }
    /* config SPICPSR register */
    HiPl022ConfigCPSR(pl022, cpsdvsr);
    /* config SPICR0 register */
    HiPl022ConfigCR0(pl022, scr);
    /* config SPICR1 register */
    HiPl022ConfigCR1(pl022);
    return 0;
}

static int HiPl022CheckTimeout(const struct HiPl022 *pl022)
{
    uint32_t value;
    uint32_t tmp = 0;

    while (1) {
        value = OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_SR);
        if ((value & SPI_SR_TFE) && (!(value & SPI_SR_BSY))) {
            break;
        }
        if (tmp++ > MAX_WAIT) {
            HDF_LOGE("%s: spi transfer wait timeout", __func__);
            return HDF_ERR_TIMEOUT;
        }
        OsalUDelay(1);
    }
    return 0;
}

static int HiPl022FlushFifo(const struct HiPl022 *pl022)
{
    uint32_t value;
    uint32_t tmp;

    tmp = HiPl022CheckTimeout(pl022);
    if (tmp) {
        return tmp;
    }
    while (1) {
        value = OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_SR);
        if (!(value & SPI_SR_RNE)) {
            break;
        }
        if (tmp++ > pl022->fifoSize) {
            HDF_LOGE("%s: spi transfer check rx fifo wait timeout", __func__);
            return HDF_ERR_TIMEOUT;
        }
        OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_DR);
    }
    return 0;
}

static int HiPl022TxRx8(const struct HiPl022 *pl022, const struct SpiMsg *msg)
{
    uint32_t len = msg->len;
    uint32_t tmpLen;
    uint32_t count;
    const uint8_t *tx = (const uint8_t *)(msg->wbuf);
    uint8_t *rx = (uint8_t *)(msg->rbuf);
    uint8_t value;
    uint32_t tmp;

    if (tx == NULL && rx == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    while (len > 0) {
        if (len > pl022->fifoSize) {
            tmpLen = pl022->fifoSize;
        } else {
            tmpLen = len;
        }
        len -= tmpLen;
        /* write fifo */
        count = tmpLen;
        value = 0;
        while (count > 0) {
            if (tx != NULL) {
                value = *tx++;
            }
            OSAL_WRITEL(value, (UINTPTR)(pl022->regBase) + REG_SPI_DR);
            count -= 1;
            OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_SR);
        }
        tmp = HiPl022CheckTimeout(pl022);
        if (tmp != 0) {
            return tmp;
        }
        /* read fifo */
        count = tmpLen;
        while (count > 0) {
            value = OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_DR);
            if (rx != NULL) {
                *rx++ = value;
            }
            count -= 1;
            OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_SR);
        }
    }
    return 0;
}

static int HiPl022TxRx16(const struct HiPl022 *pl022, const struct SpiMsg *msg)
{
    uint32_t len = msg->len;
    uint32_t tmpLen;
    uint32_t count;
    const uint16_t *tx = (const uint16_t *)(msg->wbuf);
    uint16_t *rx = (uint16_t *)(msg->rbuf);
    uint16_t value;
    uint32_t tmp;

    if (tx == NULL && rx == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    while (len > 0) {
        tmp = pl022->fifoSize * TWO_BYTES;
        if (len > tmp) {
            tmpLen = tmp;
        } else {
            tmpLen = len;
        }
        len -= tmpLen;
        /* write fifo */
        count = tmpLen;
        value = 0;
        while (count >= TWO_BYTES) {
            if (tx != NULL) {
                value = *tx++;
            }
            OSAL_WRITEL(value, (UINTPTR)(pl022->regBase) + REG_SPI_DR);
            count -= TWO_BYTES;
        }
        tmp = HiPl022CheckTimeout(pl022);
        if (tmp != 0) {
            return tmp;
        }
        /* read fifo */
        count = tmpLen;
        while (count >= TWO_BYTES) {
            value = OSAL_READL((UINTPTR)(pl022->regBase) + REG_SPI_DR);
            if (rx != NULL) {
                *rx++ = value;
            }
            count -= TWO_BYTES;
        }
    }
    return 0;
}

static int HiPl022SetCs(struct HiPl022 *pl022, uint32_t cs, uint32_t flag)
{
    if (SpiCfgCs(pl022, cs)) {
        return HDF_FAILURE;
    }
    if (flag == SPI_CS_ACTIVE) {
        HiPl022Enable(pl022);
    } else {
        HiPl022Disable(pl022);
    }
    return 0;
}

static struct SpiDev *HiPl022FindDeviceByCsNum(const struct HiPl022 *pl022, uint32_t cs)
{
    struct SpiDev *dev = NULL;
    struct SpiDev *tmpDev = NULL;

    if (pl022 == NULL || pl022->numCs <= cs) {
        return NULL;
    }
    DLIST_FOR_EACH_ENTRY_SAFE(dev, tmpDev, &(pl022->deviceList), struct SpiDev, list) {
        if (dev->csNum  == cs) {
            break;
        }
    }
    return dev;
}

int32_t HiPl022SetCfg(struct SpiCntlr *cntlr, struct SpiCfg *cfg)
{
    struct HiPl022 *pl022 = NULL;
    struct SpiDev *dev = NULL;

    if (cntlr == NULL || cntlr->priv == NULL || cfg == NULL) {
        HDF_LOGE("%s: cntlr priv or cfg is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    pl022 = (struct HiPl022 *)cntlr->priv;
    dev = HiPl022FindDeviceByCsNum(pl022, cntlr->curCs);
    if (dev == NULL) {
        return HDF_FAILURE;
    }
    dev->cfg.mode = cfg->mode;
    dev->cfg.transferMode = cfg->transferMode;
    if (cfg->bitsPerWord < BITS_PER_WORD_MIN || cfg->bitsPerWord > BITS_PER_WORD_MAX) {
        HDF_LOGE("%s: bitsPerWord %d not support, use defaule bitsPerWord %d",
            __func__, cfg->bitsPerWord, BITS_PER_WORD_EIGHT);
        dev->cfg.bitsPerWord = BITS_PER_WORD_EIGHT;
    } else {
        dev->cfg.bitsPerWord = cfg->bitsPerWord;
    }
    if (cfg->maxSpeedHz != 0) {
        dev->cfg.maxSpeedHz = cfg->maxSpeedHz;
    }
    return 0;
}

int32_t HiPl022GetCfg(struct SpiCntlr *cntlr, struct SpiCfg *cfg)
{
    struct HiPl022 *pl022 = NULL;
    struct SpiDev *dev = NULL;

    if (cntlr == NULL || cntlr->priv == NULL || cfg == NULL) {
        HDF_LOGE("%s: cntlr priv or cfg is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    pl022 = (struct HiPl022 *)cntlr->priv;
    dev = HiPl022FindDeviceByCsNum(pl022, cntlr->curCs);
    if (dev == NULL) {
        return HDF_FAILURE;
    }
    *cfg = dev->cfg;
    return HDF_SUCCESS;
}

static int32_t HiPl022TransferOneMessage(struct HiPl022 *pl022, struct SpiMsg *msg)
{
    int32_t ret;

    if (msg->speed != 0) {
        pl022->speed = msg->speed;
    } else {
        pl022->speed = DEFAULT_SPEED;
    }
    ret = HiPl022Config(pl022);
    if (ret != 0) {
        return ret;
    }
    ret = HiPl022SetCs(pl022, pl022->curCs, SPI_CS_ACTIVE);
    if (ret != 0) {
        return ret;
    }
    ret = HiPl022FlushFifo(pl022);
    if (ret != 0) {
        return ret;
    }
    if (pl022->bitsPerWord <= BITS_PER_WORD_EIGHT) {
        ret = HiPl022TxRx8(pl022, msg);
    } else {
        ret = HiPl022TxRx16(pl022, msg);
    }
    if (ret || msg->csChange) {
        HiPl022SetCs(pl022, pl022->curCs, SPI_CS_INACTIVE);
    }
    return ret;
}

int32_t HiPl022Transfer(struct SpiCntlr *cntlr, struct SpiMsg *msg, uint32_t count)
{
    int ret = HDF_FAILURE;
    uint32_t i;
    struct HiPl022 *pl022 = NULL;
    struct SpiDev *dev = NULL;

    if (cntlr == NULL || cntlr->priv == NULL || msg == NULL || count == 0) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    pl022 = (struct HiPl022 *)cntlr->priv;
    dev = HiPl022FindDeviceByCsNum(pl022, cntlr->curCs);
    if (dev == NULL) {
        goto __ERR;
    }
    pl022->mode = dev->cfg.mode;
    pl022->transferMode = dev->cfg.transferMode;
    pl022->bitsPerWord = dev->cfg.bitsPerWord;
    pl022->maxSpeedHz = dev->cfg.maxSpeedHz;
    pl022->curCs = dev->csNum;
    for (i = 0; i < count; i++) {
        ret = HiPl022TransferOneMessage(pl022, &(msg[i]));
        if (ret != 0) {
            HDF_LOGE("%s: transfer error", __func__);
            goto __ERR;
        }
    }
__ERR:
    return ret;
}

int32_t HiPl022Open(struct SpiCntlr *cntlr)
{
    (void)cntlr;
    return HDF_SUCCESS;
}

int32_t HiPl022Close(struct SpiCntlr *cntlr)
{
    (void)cntlr;
    return HDF_SUCCESS;
}

static int HiPl022Probe(struct HiPl022 *pl022)
{
    int ret;

    ret = SpiHwInitCfg(pl022);
    if (ret != 0) {
        HDF_LOGE("%s: SpiHwInitCfg error", __func__);
        return HDF_FAILURE;
    }
    ret = HiPl022Config(pl022);
    if (ret != 0) {
        HDF_LOGE("%s: HiPl022Config error", __func__);
    }
    return ret;
}

struct SpiCntlrMethod g_method = {
    .Transfer = HiPl022Transfer,
    .SetCfg = HiPl022SetCfg,
    .GetCfg = HiPl022GetCfg,
    .Open = HiPl022Open,
    .Close = HiPl022Close,
};

static int32_t HiPl022CreatAndInitDevice(struct HiPl022 *pl022)
{
    uint32_t i;
    struct SpiDev *device = NULL;

    for (i = 0; i < pl022->numCs; i++) {
        device = (struct SpiDev *)OsalMemCalloc(sizeof(*device));
        if (device == NULL) {
            HDF_LOGE("%s: OsalMemCalloc error", __func__);
            return HDF_FAILURE;
        }
        device->cntlr = pl022->cntlr;
        device->csNum = i;
        device->cfg.bitsPerWord = pl022->bitsPerWord;
        device->cfg.transferMode = pl022->transferMode;
        device->cfg.maxSpeedHz = pl022->maxSpeedHz;
        device->cfg.mode = pl022->mode;
        DListHeadInit(&device->list);
        DListInsertTail(&device->list, &pl022->deviceList);
        SpiAddDev(device);
    }
    return 0;
}

static void HiPl022Release(struct HiPl022 *pl022)
{
    struct SpiDev *dev = NULL;
    struct SpiDev *tmpDev = NULL;

    DLIST_FOR_EACH_ENTRY_SAFE(dev, tmpDev, &(pl022->deviceList), struct SpiDev, list) {
        if (dev != NULL) {
            SpiRemoveDev(dev);
            DListRemove(&(dev->list));
            OsalMemFree(dev);
        }
    }
    OsalMemFree(pl022);
}

static int32_t SpiGetBaseCfgFromHcs(struct HiPl022 *pl022, const struct DeviceResourceNode *node)
{
    struct DeviceResourceIface *iface = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);

    if (iface == NULL || iface->GetUint8 == NULL || iface->GetUint16 == NULL || iface->GetUint32 == NULL) {
        HDF_LOGE("%s: face is invalid", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint32(node, "busNum", &pl022->busNum, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read busNum fail", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint32(node, "numCs", &pl022->numCs, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read numCs fail", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint32(node, "speed", &pl022->speed, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read speed fail", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint32(node, "fifoSize", &pl022->fifoSize, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read fifoSize fail", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint32(node, "clkRate", &pl022->clkRate, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read clkRate fail", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint16(node, "mode", &pl022->mode, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read mode fail", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint8(node, "bitsPerWord", &pl022->bitsPerWord, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read bitsPerWord fail", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint8(node, "transferMode", &pl022->transferMode, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read comMode fail", __func__);
        return HDF_FAILURE;
    }
    return 0;
}

static int32_t SpiGetRegCfgFromHcs(struct HiPl022 *pl022, const struct DeviceResourceNode *node)
{
    uint32_t tmp;
    struct DeviceResourceIface *iface = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);

    if (iface == NULL || iface->GetUint32 == NULL) {
        HDF_LOGE("%s: face is invalid", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint32(node, "regBase", &tmp, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read regBase fail", __func__);
        return HDF_FAILURE;
    }
    pl022->regBase = (void *)(uintptr_t)(HDF_IO_DEVICE_ADDR(tmp));
    if (iface->GetUint32(node, "REG_CRG_SPI", &pl022->regCrg, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read regCrg fail", __func__);
        return HDF_FAILURE;
    }
    pl022->regCrg = HDF_IO_DEVICE_ADDR(pl022->regCrg);
    if (iface->GetUint32(node, "CRG_SPI_CKEN", &pl022->clkEnBit, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read clkEnBit fail", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint32(node, "CRG_SPI_RST", &pl022->clkRstBit, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read clkRstBit fail", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint32(node, "REG_MISC_CTRL_SPI", &pl022->regMiscCtrl, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read regMiscCtrl fail", __func__);
        return HDF_FAILURE;
    }
    pl022->regMiscCtrl = HDF_IO_DEVICE_ADDR(pl022->regMiscCtrl);
    if (iface->GetUint32(node, "MISC_CTRL_SPI_CS", &pl022->miscCtrlCs, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read miscCtrlCs fail", __func__);
        return HDF_FAILURE;
    }
    if (iface->GetUint32(node, "MISC_CTRL_SPI_CS_SHIFT", &pl022->miscCtrlCsShift, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read miscCtrlCsShift fail", __func__);
        return HDF_FAILURE;
    }
    return 0;
}

static int HiPl022Init(struct SpiCntlr *cntlr, const struct HdfDeviceObject *device)
{
    int ret;
    struct HiPl022 *pl022 = NULL;

    if (device->property == NULL) {
        HDF_LOGE("%s: property is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    pl022 = (struct HiPl022 *)OsalMemCalloc(sizeof(*pl022));
    if (pl022 == NULL) {
        HDF_LOGE("%s: OsalMemCalloc error", __func__);
        return HDF_FAILURE;
    }
    ret = SpiGetBaseCfgFromHcs(pl022, device->property);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: SpiGetBaseCfgFromHcs error", __func__);
        OsalMemFree(pl022);
        return HDF_FAILURE;
    }
    ret = SpiGetRegCfgFromHcs(pl022, device->property);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: SpiGetRegCfgFromHcs error", __func__);
        OsalMemFree(pl022);
        return HDF_FAILURE;
    }
    pl022->maxSpeedHz = (pl022->clkRate) / ((SCR_MIN + 1) * CPSDVSR_MIN);
    pl022->minSpeedHz = (pl022->clkRate) / ((SCR_MAX + 1) * CPSDVSR_MAX);
    DListHeadInit(&pl022->deviceList);
    pl022->cntlr = cntlr;
    cntlr->priv = pl022;
    cntlr->busNum = pl022->busNum;
    cntlr->method = &g_method;
    ret = HiPl022CreatAndInitDevice(pl022);
    if (ret != 0) {
        HiPl022Release(pl022);
        return ret;
    }
    return 0;
}

static void HiPl022Remove(struct HiPl022 *pl022)
{
    if (SpiHwExitCfg(pl022) != 0) {
        HDF_LOGE("%s: SpiHwExitCfg error", __func__);
    }
    HiPl022Release(pl022);
}

static int32_t HdfSpiDeviceBind(struct HdfDeviceObject *device)
{
    HDF_LOGI("%s: entry", __func__);
    if (device == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    return (SpiCntlrCreate(device) == NULL) ? HDF_FAILURE : HDF_SUCCESS;
}

int32_t HdfSpiDeviceInit(struct HdfDeviceObject *device)
{
    int ret;
    struct SpiCntlr *cntlr = NULL;

    HDF_LOGI("%s: entry", __func__);
    if (device == NULL) {
        HDF_LOGE("%s: ptr is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = SpiCntlrFromDevice(device);
    if (cntlr == NULL) {
        HDF_LOGE("%s: cntlr is null", __func__);
        return HDF_FAILURE;
    }

    ret = HiPl022Init(cntlr, device);
    if (ret != 0) {
        HDF_LOGE("%s: error init", __func__);
        return HDF_FAILURE;
    }
    ret = HiPl022Probe(cntlr->priv);
    if (ret != 0) {
        HDF_LOGE("%s: error probe", __func__);
    }
    return ret;
}

void HdfSpiDeviceRelease(struct HdfDeviceObject *device)
{
    struct SpiCntlr *cntlr = NULL;

    HDF_LOGI("%s: entry", __func__);
    if (device == NULL) {
        HDF_LOGE("%s: device is null", __func__);
        return;
    }
    cntlr = SpiCntlrFromDevice(device);
    if (cntlr == NULL) {
        HDF_LOGE("%s: cntlr is null", __func__);
        return;
    }
    if (cntlr->priv != NULL) {
        HiPl022Remove((struct HiPl022 *)cntlr->priv);
    }
    SpiCntlrDestroy(cntlr);
}

struct HdfDriverEntry g_hdfSpiDevice = {
    .moduleVersion = 1,
    .moduleName = "HDF_PLATFORM_SPI",
    .Bind = HdfSpiDeviceBind,
    .Init = HdfSpiDeviceInit,
    .Release = HdfSpiDeviceRelease,
};

HDF_INIT(g_hdfSpiDevice);
