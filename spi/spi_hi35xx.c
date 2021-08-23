/*
 * Copyright (c) 2020-2021 HiSilicon (Shanghai) Technologies CO., LIMITED.
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

#include "spi_hi35xx.h"
#include "device_resource_if.h"
#include "dmac_core.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "los_vm_phys.h"
#include "osal_io.h"
#include "osal_irq.h"
#include "osal_mem.h"
#include "osal_sem.h"
#include "osal_time.h"
#include "spi_core.h"
#include "spi_dev.h"

#define HDF_LOG_TAG spi_hi35xx

struct Pl022 {
    struct SpiCntlr *cntlr;
    struct DListHead deviceList;
    struct OsalSem sem;
    volatile unsigned char *phyBase;
    volatile unsigned char *regBase;
    uint32_t irqNum;
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

static int32_t SpiCfgCs(struct Pl022 *pl022, uint32_t cs)
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

static int32_t SpiHwInitCfg(struct Pl022 *pl022)
{
    uint32_t value;

    value = OSAL_READL(pl022->regCrg);
    value &= ~pl022->clkRstBit;
    value |= pl022->clkEnBit;
    OSAL_WRITEL(value, pl022->regCrg); /* open spi clk */
    return 0;
}

static int32_t SpiHwExitCfg(struct Pl022 *pl022)
{
    uint32_t value;

    value = OSAL_READL(pl022->regCrg);
    value |= pl022->clkRstBit;
    value &= ~pl022->clkEnBit;
    OSAL_WRITEL(value, pl022->regCrg); /* close spi clk */
    return 0;
}

static void Pl022Enable(const struct Pl022 *pl022)
{
    uint32_t value;

    value = OSAL_READL((uintptr_t)(pl022->regBase) + REG_SPI_CR1);
    value |= SPI_CR1_SSE;
    OSAL_WRITEL(value, (uintptr_t)(pl022->regBase) + REG_SPI_CR1);
}

static void Pl022Disable(const struct Pl022 *pl022)
{
    uint32_t value;

    value = OSAL_READL((uintptr_t)(pl022->regBase) + REG_SPI_CR1);
    value &= ~SPI_CR1_SSE;
    OSAL_WRITEL(value, (uintptr_t)(pl022->regBase) + REG_SPI_CR1);
}

static void Pl022ConfigCPSR(const struct Pl022 *pl022, uint32_t cpsdvsr)
{
    uint32_t value;

    value = OSAL_READL((uintptr_t)(pl022->regBase) + REG_SPI_CPSR);
    value &= ~SPI_CPSR_CPSDVSR;
    value |= cpsdvsr << SPI_CPSR_CPSDVSR_SHIFT;
    OSAL_WRITEL(value, (uintptr_t)(pl022->regBase) + REG_SPI_CPSR);
}

static void Pl022ConfigCR0(const struct Pl022 *pl022, uint32_t scr)
{
    uint32_t tmp;
    uint32_t value;

    value = OSAL_READL((uintptr_t)(pl022->regBase) + REG_SPI_CR0);
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
    OSAL_WRITEL(value, (uintptr_t)(pl022->regBase) + REG_SPI_CR0);
}

static void Pl022ConfigCR1(const struct Pl022 *pl022)
{
    uint32_t tmp;
    uint32_t value;

    value = OSAL_READL((uintptr_t)(pl022->regBase) + REG_SPI_CR1);
    value &= ~SPI_CR1_LBN;
    tmp = (!!(pl022->mode & SPI_MODE_LOOP)) ? (1 << SPI_CR1_LBN_SHIFT) : 0;
    value |= tmp;
    value &= ~SPI_CR1_MS;
    value &= ~SPI_CR1_BIG_END;
    tmp = (!!(pl022->mode & SPI_MODE_LSBFE)) ? (1 << SPI_CR1_BIG_END_SHIFT) : 0;
    value |= tmp;
    value &= ~SPI_CR1_ALT;
    value |= 0x1 << SPI_CR1_ALT_SHIFT;
    OSAL_WRITEL(value, (uintptr_t)(pl022->regBase) + REG_SPI_CR1);
}

static void Pl022ConfigDma(struct Pl022 *pl022)
{
    uint32_t value;
    if (pl022->transferMode == SPI_DMA_TRANSFER) {
        OSAL_WRITEL((0x1 << TX_DMA_EN_SHIFT) | (0x1 << RX_DMA_EN_SHIFT),
            (uintptr_t)(pl022->regBase) + SPI_DMA_CR);

        value = OSAL_READL((uintptr_t)(pl022->regBase) + SPI_TX_FIFO_CR);
        value &= ~(TX_DMA_BR_SIZE_MASK << TX_DMA_BR_SIZE_SHIFT);
        OSAL_WRITEL(value, (uintptr_t)(pl022->regBase) + SPI_TX_FIFO_CR);

        value = OSAL_READL((uintptr_t)(pl022->regBase) + SPI_RX_FIFO_CR);
        value &= ~(RX_DMA_BR_SIZE_MASK << RX_DMA_BR_SIZE_SHIFT);
        OSAL_WRITEL(value, (uintptr_t)(pl022->regBase) + SPI_RX_FIFO_CR);
    } else {
        OSAL_WRITEL(0, (uintptr_t)(pl022->regBase) + SPI_DMA_CR);
    }
}

#define RX_INT_SIZE_VALUE      0x6
#define RX_INT_FIFO_LEVEL      (256 - 128)
#define RX_INT_WAIT_TIMEOUT    1000 // ms

static void Pl022ConfigIrq(struct Pl022 *pl022)
{
    unsigned long value;

    if (pl022->transferMode == SPI_INTERRUPT_TRANSFER) {
        value = OSAL_READL((uintptr_t)(pl022->regBase) + SPI_RX_FIFO_CR);
        value &= ~(RX_INT_SIZE_MASK << RX_INT_SIZE_SHIFT);
        value |= ((RX_INT_SIZE_VALUE & RX_INT_SIZE_MASK) << RX_INT_SIZE_SHIFT);
        OSAL_WRITEL(value, (uintptr_t)(pl022->regBase) + SPI_RX_FIFO_CR);
    }

    OSAL_WRITEL(SPI_ALL_IRQ_DISABLE, (uintptr_t)(pl022->regBase) + REG_SPI_IMSC);
    OSAL_WRITEL(SPI_ALL_IRQ_CLEAR, (uintptr_t)(pl022->regBase) + REG_SPI_ICR);
}

static int32_t Pl022Config(struct Pl022 *pl022)
{
    uint32_t tmp;
    uint32_t scr;
    uint32_t cpsdvsr;

    Pl022Disable(pl022);
    /* Check if we can provide the requested rate */
    if (pl022->speed > pl022->maxSpeedHz) {
        HDF_LOGW("%s: invalid speed:%d, use max:%d instead", __func__, pl022->speed, pl022->maxSpeedHz);
        pl022->speed = pl022->maxSpeedHz;
    }
    /* Min possible */
    if (pl022->speed == 0 || pl022->speed < pl022->minSpeedHz) {
        HDF_LOGW("%s: invalid speed:%d, use min:%d instead", __func__, pl022->speed, pl022->minSpeedHz);
        pl022->speed = pl022->minSpeedHz;
    }
    /* Check if we can provide the requested bits_per_word */
    if ((pl022->bitsPerWord < BITS_PER_WORD_MIN) || (pl022->bitsPerWord > BITS_PER_WORD_MAX)) {
        HDF_LOGE("%s: pl022->bitsPerWord is %d not support", __func__, pl022->bitsPerWord);
        return HDF_FAILURE;
    }
    /* compute spi speed, speed=clk/(cpsdvsr*(scr+1)) */
    tmp = (pl022->clkRate) / (pl022->speed);
    cpsdvsr = (tmp < CPSDVSR_MIN) ? CPSDVSR_MIN : (tmp <= CPSDVSR_MAX) ? (tmp & (~0x1)) : CPSDVSR_MAX;
    scr = (tmp < CPSDVSR_MIN) ? 0 : (tmp <= CPSDVSR_MAX);

    /* config SPICPSR register */
    Pl022ConfigCPSR(pl022, cpsdvsr);
    /* config SPICR0 register */
    Pl022ConfigCR0(pl022, scr);
    /* config SPICR1 register */
    Pl022ConfigCR1(pl022);
    /* config irq */
    Pl022ConfigIrq(pl022);
    /* config dma */
    Pl022ConfigDma(pl022);
    return 0;
}

static int32_t Pl022CheckTimeout(const struct Pl022 *pl022)
{
    uint32_t tmp = 0;
    unsigned long value;

    while (1) {
        value = OSAL_READL((uintptr_t)(pl022->regBase) + REG_SPI_SR);
        if ((value & SPI_SR_TFE) && (!(value & SPI_SR_BSY))) {
            break;
        }
        if (tmp++ > MAX_WAIT) {
            HDF_LOGE("%s: spi transfer wait timeout", __func__);
            return HDF_ERR_TIMEOUT;
        }
        OsalUDelay(1);
    }
    return HDF_SUCCESS;
}

static int32_t Pl022FlushFifo(const struct Pl022 *pl022)
{
    uint32_t value;
    uint32_t tmp;

    tmp = Pl022CheckTimeout(pl022);
    if (tmp) {
        return tmp;
    }
    while (1) {
        value = OSAL_READL((uintptr_t)(pl022->regBase) + REG_SPI_SR);
        if (!(value & SPI_SR_RNE)) {
            break;
        }
        if (tmp++ > pl022->fifoSize) {
            HDF_LOGE("%s: spi transfer check rx fifo wait timeout", __func__);
            return HDF_ERR_TIMEOUT;
        }
        OSAL_READL((uintptr_t)(pl022->regBase) + REG_SPI_DR);
    }
    return 0;
}

#define PL022_ONE_BYTE 1
#define PL022_TWO_BYTE 2

static inline uint8_t Pl022ToByteWidth(uint8_t bitsPerWord)
{
    if (bitsPerWord <= BITS_PER_WORD_EIGHT) {
        return PL022_ONE_BYTE; 
    } else {
        return PL022_TWO_BYTE;
    }
}

static void Pl022WriteFifo(const struct Pl022 *pl022, const uint8_t *tx, uint32_t count)
{
    unsigned long value;
    uint8_t bytes = Pl022ToByteWidth(pl022->bitsPerWord);

    for (value = 0; count >= bytes; count -= bytes) {
        if (tx != NULL) {
            value = (bytes == 1) ? *tx : *((uint16_t *)tx);
            tx += bytes;
        }
        OSAL_WRITEL(value, (uintptr_t)(pl022->regBase) + REG_SPI_DR);
    }
}

static void Pl022ReadFifo(const struct Pl022 *pl022, uint8_t *rx, uint32_t count)
{
    unsigned long value;
    uint8_t bytes = Pl022ToByteWidth(pl022->bitsPerWord);

    for (value = 0; count >= bytes; count -= bytes) {
        value = OSAL_READL((uintptr_t)(pl022->regBase) + REG_SPI_DR);
        if (rx == NULL) {
            continue;
        }
        if (bytes == 1) {
            *rx = (uint8_t)value;
        } else {
            *((uint16_t *)rx) = (uint16_t)value;
        }
        rx += bytes;
    }
}

static int32_t Pl022TxRx(const struct Pl022 *pl022, const struct SpiMsg *msg)
{
    int32_t ret;
    uint32_t tmpLen;
    uint32_t len = msg->len;
    const uint8_t *tx = msg->wbuf;
    uint8_t *rx = msg->rbuf;
    uint8_t bytes = Pl022ToByteWidth(pl022->bitsPerWord);
    uint32_t burstSize = pl022->fifoSize * bytes;

    if (tx == NULL && rx == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }

    if (pl022->transferMode == SPI_INTERRUPT_TRANSFER && RX_INT_FIFO_LEVEL < pl022->fifoSize) {
        burstSize = RX_INT_FIFO_LEVEL * bytes;
    } 

    for (tmpLen = 0, len = msg->len; len > 0; len -= tmpLen) {
        tmpLen = (len > burstSize) ? burstSize : len;
        if (pl022->transferMode == SPI_INTERRUPT_TRANSFER && tmpLen == burstSize) {
            OSAL_WRITEL(SPI_ALL_IRQ_ENABLE, (uintptr_t)(pl022->regBase) + REG_SPI_IMSC);
        }
        Pl022WriteFifo(pl022, tx, tmpLen);
        tx = (tx == NULL) ? NULL : tx + tmpLen;
        if (pl022->transferMode == SPI_INTERRUPT_TRANSFER && tmpLen == burstSize) {
            ret = OsalSemWait((struct OsalSem *)(&pl022->sem), RX_INT_WAIT_TIMEOUT);
        } else {
            ret = Pl022CheckTimeout(pl022);
        }

        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%s: %s timeout", __func__, (pl022->transferMode == SPI_INTERRUPT_TRANSFER) ?
                "wait rx fifo int" : "wait tx fifo idle");
            return ret;
        }

        Pl022ReadFifo(pl022, rx, tmpLen);
        rx = (rx == NULL) ? NULL : (rx + tmpLen);
    }
    return 0;
}

static int32_t Pl022SetCs(struct Pl022 *pl022, uint32_t cs, uint32_t flag)
{
    if (SpiCfgCs(pl022, cs)) {
        return HDF_FAILURE;
    }
    if (flag == SPI_CS_ACTIVE) {
        Pl022Enable(pl022);
    } else {
        Pl022Disable(pl022);
    }
    return 0;
}

static struct SpiDev *Pl022FindDeviceByCsNum(const struct Pl022 *pl022, uint32_t cs)
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

static int32_t Pl022SetCfg(struct SpiCntlr *cntlr, struct SpiCfg *cfg)
{
    struct Pl022 *pl022 = NULL;
    struct SpiDev *dev = NULL;

    if (cntlr == NULL || cntlr->priv == NULL || cfg == NULL) {
        HDF_LOGE("%s: cntlr priv or cfg is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    pl022 = (struct Pl022 *)cntlr->priv;
    dev = Pl022FindDeviceByCsNum(pl022, cntlr->curCs);
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

static int32_t Pl022GetCfg(struct SpiCntlr *cntlr, struct SpiCfg *cfg)
{
    struct Pl022 *pl022 = NULL;
    struct SpiDev *dev = NULL;

    if (cntlr == NULL || cntlr->priv == NULL || cfg == NULL) {
        HDF_LOGE("%s: cntlr priv or cfg is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    pl022 = (struct Pl022 *)cntlr->priv;
    dev = Pl022FindDeviceByCsNum(pl022, cntlr->curCs);
    if (dev == NULL) {
        return HDF_FAILURE;
    }
    *cfg = dev->cfg;
    return HDF_SUCCESS;
}

struct SpiDmaEvent {
    const struct SpiMsg *spiMsg;
    struct DmacMsg *dmaMsg;
    struct OsalSem sem;
    int32_t retValue;
};

static void Pl022DmaCallBack(void *data, int status)
{
    struct SpiDmaEvent *xEvent = (struct SpiDmaEvent *)data;

    if (xEvent != NULL) {
#ifdef SPI_HI35XX_DEBUG
        HDF_LOGD("%s: dmamsg transfer %s!", __func__, status == DMAC_CHN_SUCCESS ? "Success!" : "Error!");
#endif
        if (status != DMAC_CHN_SUCCESS) {
            xEvent->retValue = HDF_FAILURE;
            HDF_LOGE("%s: dma msg transfer failed, status = %d", __func__, status);
        }
        (void)OsalSemPost(&xEvent->sem);
    }
}

static struct DmaCntlr *GetDmaCntlr(void)
{
    static struct DmaCntlr *cntlr = NULL;

    if (cntlr == NULL) {
        cntlr = (struct DmaCntlr *)DevSvcManagerClntGetService("HDF_PLATFORM_DMAC0");
        if (cntlr != NULL) {
            HDF_LOGE("%s: get dmac0 success!", __func__);
        } else {
            HDF_LOGE("%s: get dmac0 fail!", __func__);
        }
    }
    return cntlr;
}

static inline uintptr_t Pl022AllocBufPhy(size_t len)
{
    void *tmpBuf = OsalMemCalloc(len);
    return tmpBuf == NULL ? 0 : (uintptr_t)LOS_PaddrQuery(tmpBuf);
}

static inline void Pl022RleaseBufPhy(uintptr_t buf)
{
    void *tmpBuf = NULL;

    if (buf != 0) {
        tmpBuf = LOS_PaddrToKVaddr((paddr_t)buf);
        OsalMemFree(tmpBuf);
    }
}

static int32_t Pl022TxRxDma(const struct Pl022 *pl022, const struct SpiMsg *msg)
{
    int32_t ret;
    struct SpiDmaEvent rxEvent;
    struct DmacMsg dmaMsgRx;
    struct DmacMsg dmaMsgTx;
    uintptr_t tmpBuf = 0;

    rxEvent.dmaMsg = &dmaMsgRx;
    rxEvent.spiMsg = msg;
    rxEvent.retValue = HDF_SUCCESS;

    (void)OsalSemInit(&rxEvent.sem, 0);

    if (msg->rbuf == NULL || msg->wbuf == NULL) {
        if ((tmpBuf = Pl022AllocBufPhy(msg->len)) == 0) {
            return HDF_ERR_MALLOC_FAIL;
        }
    }

    dmaMsgRx.srcAddr = (uintptr_t)(pl022->phyBase + REG_SPI_DR);
    dmaMsgRx.destAddr = (msg->rbuf == NULL) ? tmpBuf : (uintptr_t)(LOS_PaddrQuery(msg->rbuf));
    dmaMsgRx.transLen = msg->len;
    dmaMsgRx.transType = TRASFER_TYPE_P2M;
    dmaMsgRx.cb = Pl022DmaCallBack;
    dmaMsgRx.para = &rxEvent;
    dmaMsgRx.srcWidth = dmaMsgRx.destWidth = Pl022ToByteWidth(pl022->bitsPerWord);

    dmaMsgTx.srcAddr = (msg->wbuf == NULL) ? tmpBuf : (uintptr_t)(LOS_PaddrQuery(msg->wbuf));
    dmaMsgTx.destAddr = (uintptr_t)(pl022->phyBase + REG_SPI_DR);
    dmaMsgTx.transLen = msg->len;
    dmaMsgTx.transType = TRASFER_TYPE_M2P;
    dmaMsgTx.cb = NULL;
    dmaMsgTx.para = NULL;
    dmaMsgTx.srcWidth = dmaMsgTx.destWidth = dmaMsgRx.srcWidth;

    if ((ret = DmaCntlrTransfer(GetDmaCntlr(), &dmaMsgRx)) != HDF_SUCCESS) { 
        HDF_LOGE("%s: rx dma trans fail : %d", __func__, ret);
        goto __OUT;
    }
    if ((ret = DmaCntlrTransfer(GetDmaCntlr(), &dmaMsgTx)) != HDF_SUCCESS) { 
        HDF_LOGE("%s: tx dma trans fail : %d", __func__, ret);
        goto __OUT;
    }
 
#ifdef SPI_HI35XX_DEBUG
    HDF_LOGD("%s: trying to wait dma callback...", __func__);
#endif
    (void)OsalSemWait(&rxEvent.sem, HDF_WAIT_FOREVER);
    ret = (rxEvent.retValue != HDF_SUCCESS) ? rxEvent.retValue : ret;

__OUT:
    (void)OsalSemDestroy(&rxEvent.sem);
    Pl022RleaseBufPhy(tmpBuf);
    return ret;
}

static int32_t Pl022TransferOneMessage(struct Pl022 *pl022, struct SpiMsg *msg)
{
    int32_t ret;

    pl022->speed = (msg->speed) == 0 ? DEFAULT_SPEED : msg->speed;

    ret = Pl022Config(pl022);
    if (ret != 0) {
        return ret;
    }
    ret = Pl022SetCs(pl022, pl022->curCs, SPI_CS_ACTIVE);
    if (ret != 0) {
        return ret;
    }
    ret = Pl022FlushFifo(pl022);
    if (ret != 0) {
        return ret;
    }
    if (pl022->transferMode == SPI_DMA_TRANSFER) {
        ret = Pl022TxRxDma(pl022, msg);
    } else {
        ret = Pl022TxRx(pl022, msg);
    }
    if (ret || msg->csChange) {
        Pl022SetCs(pl022, pl022->curCs, SPI_CS_INACTIVE);
    }
    return ret;
}

static int32_t Pl022Transfer(struct SpiCntlr *cntlr, struct SpiMsg *msg, uint32_t count)
{
    int32_t ret = HDF_FAILURE;
    uint32_t i;
    struct Pl022 *pl022 = NULL;
    struct SpiDev *dev = NULL;

    if (cntlr == NULL || cntlr->priv == NULL) {
        HDF_LOGE("%s: invalid controller", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    if (msg == NULL || (msg->rbuf == NULL && msg->wbuf == NULL) || count == 0) {
        HDF_LOGE("%s: invalid parameter", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    pl022 = (struct Pl022 *)cntlr->priv;
    dev = Pl022FindDeviceByCsNum(pl022, cntlr->curCs);
    if (dev == NULL) {
        goto __ERR;
    }
    pl022->mode = dev->cfg.mode;
    pl022->transferMode = dev->cfg.transferMode;
    pl022->bitsPerWord = dev->cfg.bitsPerWord;
    pl022->maxSpeedHz = dev->cfg.maxSpeedHz;
    pl022->curCs = dev->csNum;
    for (i = 0; i < count; i++) {
        ret = Pl022TransferOneMessage(pl022, &(msg[i]));
        if (ret != 0) {
            HDF_LOGE("%s: transfer error", __func__);
            goto __ERR;
        }
    }
__ERR:
    return ret;
}

int32_t Pl022Open(struct SpiCntlr *cntlr)
{
    (void)cntlr;
    return HDF_SUCCESS;
}

int32_t Pl022Close(struct SpiCntlr *cntlr)
{
    (void)cntlr;
    return HDF_SUCCESS;
}

static int32_t Pl022Probe(struct Pl022 *pl022)
{
    int32_t ret;

    ret = SpiHwInitCfg(pl022);
    if (ret != 0) {
        HDF_LOGE("%s: SpiHwInitCfg error", __func__);
        return HDF_FAILURE;
    }
    ret = Pl022Config(pl022);
    if (ret != 0) {
        HDF_LOGE("%s: Pl022Config error", __func__);
    }
    return ret;
}

struct SpiCntlrMethod g_method = {
    .Transfer = Pl022Transfer,
    .SetCfg = Pl022SetCfg,
    .GetCfg = Pl022GetCfg,
    .Open = Pl022Open,
    .Close = Pl022Close,
};

static int32_t Pl022CreatAndInitDevice(struct Pl022 *pl022)
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

static void Pl022Release(struct Pl022 *pl022)
{
    struct SpiDev *dev = NULL;
    struct SpiDev *tmpDev = NULL;

    DLIST_FOR_EACH_ENTRY_SAFE(dev, tmpDev, &(pl022->deviceList), struct SpiDev, list) {
        SpiRemoveDev(dev);
        DListRemove(&(dev->list));
        OsalMemFree(dev);
    }
    if (pl022->irqNum != 0) {
        (void)OsalUnregisterIrq(pl022->irqNum, pl022);
    }
    OsalMemFree(pl022);
}

static int32_t SpiGetBaseCfgFromHcs(struct Pl022 *pl022, const struct DeviceResourceNode *node)
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

static int32_t SpiGetRegCfgFromHcs(struct Pl022 *pl022, const struct DeviceResourceNode *node)
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
    pl022->phyBase = (void *)(uintptr_t)(tmp);
    pl022->regBase = HDF_IO_DEVICE_ADDR(pl022->phyBase);

    if (iface->GetUint32(node, "irqNum", &pl022->irqNum, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read irqNum fail", __func__);
        return HDF_FAILURE;
    }
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

static uint32_t Pl022IrqHandleNoShare(uint32_t irq, void *data)
{
    unsigned long value;
    struct Pl022 *pl022 = (struct Pl022 *)data;

    (void)irq;
    if (pl022 == NULL) {
        HDF_LOGE("%s: data is NULL!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    // check SPI_RX_INTR
    value = OSAL_READL((uintptr_t)(pl022->regBase) + REG_SPI_MIS);
    if ((value & SPI_RX_INTR_MASK) != 0) {
        OSAL_WRITEL(SPI_ALL_IRQ_DISABLE, (uintptr_t)(pl022->regBase) + REG_SPI_IMSC);
        OSAL_WRITEL(SPI_ALL_IRQ_CLEAR, (uintptr_t)(pl022->regBase) + REG_SPI_ICR);
        (void)OsalSemPost(&pl022->sem);
    }

    return HDF_SUCCESS;
}

static int32_t Pl022Init(struct SpiCntlr *cntlr, const struct HdfDeviceObject *device)
{
    int32_t ret;
    struct Pl022 *pl022 = NULL;

    if (device->property == NULL) {
        HDF_LOGE("%s: property is NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    pl022 = (struct Pl022 *)OsalMemCalloc(sizeof(*pl022));
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
    ret = OsalSemInit(&pl022->sem, 0);
    if (ret != HDF_SUCCESS) {
        OsalMemFree(pl022);
        return ret;
    }
    
    if (pl022->irqNum != 0) {
        OSAL_WRITEL(SPI_ALL_IRQ_DISABLE, (uintptr_t)(pl022->regBase) + REG_SPI_IMSC);
        ret = OsalRegisterIrq(pl022->irqNum, 0, Pl022IrqHandleNoShare, "SPI_HI35XX", pl022);
        if (ret != HDF_SUCCESS) {
            OsalMemFree(pl022);
            (void)OsalSemDestroy(&pl022->sem);
            return ret;
        }
    }

    ret = Pl022CreatAndInitDevice(pl022);
    if (ret != 0) {
        Pl022Release(pl022);
        return ret;
    }
    return 0;
}

static void Pl022Remove(struct Pl022 *pl022)
{
    if (SpiHwExitCfg(pl022) != 0) {
        HDF_LOGE("%s: SpiHwExitCfg error", __func__);
    }
    Pl022Release(pl022);
}

static int32_t HdfSpiDeviceBind(struct HdfDeviceObject *device)
{
    HDF_LOGI("%s: entry", __func__);
    if (device == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    return (SpiCntlrCreate(device) == NULL) ? HDF_FAILURE : HDF_SUCCESS;
}

static int32_t HdfSpiDeviceInit(struct HdfDeviceObject *device)
{
    int32_t ret;
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

    ret = Pl022Init(cntlr, device);
    if (ret != 0) {
        HDF_LOGE("%s: error init", __func__);
        return HDF_FAILURE;
    }
    ret = Pl022Probe(cntlr->priv);
    if (ret != 0) {
        HDF_LOGE("%s: error probe", __func__);
    }
    return ret;
}

static void HdfSpiDeviceRelease(struct HdfDeviceObject *device)
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
        Pl022Remove((struct Pl022 *)cntlr->priv);
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
