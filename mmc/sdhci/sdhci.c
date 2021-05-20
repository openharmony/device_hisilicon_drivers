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

#include "sdhci.h"
#include "sdhci_proc.h"
#include "securec.h"

#define HDF_LOG_TAG sdhci_adapter

#define SDHCI_DMA_MAX_BUFF_SIZE 0x1000
#define SDHCI_MAX_BLK_NUM 65535
#define SDHCI_DETECET_RETRY 5
#define SDHCI_RESET_RETRY_TIMES 100
#define SDHCI_OFF_CLK2CARD_ON_DELAY 25
#define SDHCI_MAX_HOST_NUM 2
#define SDHCI_CLK_CTRL_RETRY_TIMES 20
#define SDHCI_PHASE_SCALE_GAP 16
#define SDHCI_DIV_MIDDLE 2
#define SDHCI_CART_PLUG_STATE (SDHCI_INTERRUPT_CARD_INSERT | SDHCI_INTERRUPT_CARD_REMOVE)
#define SDHCI_PLUG_STATE(h) (SdhciReadl(h, PSTATE_R) & SDHCI_CARD_PRESENT)

static void SdhciDumpregs(struct SdhciHost *host)
{
    HDF_LOGE(": =========== DUMP (host%d) REGISTER===========", host->hostId);
    HDF_LOGE(": Sys addr: 0x%08x | Version:  0x%04x",
        SdhciReadl(host, SDMASA_R), SdhciReadw(host, HOST_VERSION_R));
    HDF_LOGE(": Blk size: 0x%04x | Blk cnt:  0x%04x",
        SdhciReadw(host, BLOCKSIZE_R), SdhciReadw(host, BLOCKCOUNT_R));
    HDF_LOGE(": Argument: 0x%08x | Trn mode: 0x%04x",
        SdhciReadl(host, ARGUMENT_R), SdhciReadw(host, XFER_MODE_R));
    HDF_LOGE(": Present:  0x%08x | Host ctl: 0x%08x",
        SdhciReadl(host, PSTATE_R), SdhciReadb(host, HOST_CTRL1_R));
    HDF_LOGE(": Power:    0x%08x | Blk gap:  0x%08x",
        SdhciReadb(host, PWR_CTRL_R), SdhciReadb(host, BLOCK_GAP_CTRL_R));
    HDF_LOGE(": Wake-up:  0x%08x | Clock:    0x%04x",
        SdhciReadb(host, WUP_CTRL_R), SdhciReadw(host, CLK_CTRL_R));
    HDF_LOGE(": Timeout:  0x%08x | Int stat: 0x%08x",
        SdhciReadb(host, TOUT_CTRL_R), SdhciReadl(host, NORMAL_INT_STAT_R));
    HDF_LOGE(": Int enab: 0x%08x | Sig enab: 0x%08x",
        SdhciReadl(host, NORMAL_INT_STAT_EN_R), SdhciReadl(host, NORMAL_INT_SIGNAL_EN_R));
    HDF_LOGE(": ACMD err: 0x%04x | Slot int: 0x%04x",
        SdhciReadw(host, AUTO_CMD_STAT_R), SdhciReadw(host, SLOT_INT_STATUS_R));
    HDF_LOGE(": Caps_1:   0x%08x | Caps_2:   0x%08x",
        SdhciReadl(host, CAPABILITIES1_R), SdhciReadl(host, CAPABILITIES2_R));
    HDF_LOGE(": Cmd:      0x%04x | Max curr: 0x%08x | opcode = %d",
        SdhciReadw(host, CMD_R), SdhciReadl(host, CURR_CAPBILITIES1_R), (SdhciReadw(host, CMD_R) >> 8) & 0x1f);
    HDF_LOGE(": Resp 1:   0x%08x | Resp 0:   0x%08x",
        SdhciReadl(host, RESP01_R + 0x4), SdhciReadl(host, RESP01_R));
    HDF_LOGE(": Resp 3:   0x%08x | Resp 2:   0x%08x",
        SdhciReadl(host, RESP01_R + 0xC), SdhciReadl(host, RESP01_R + 0x8));
    HDF_LOGE(": Host ctl2: 0x%04x", SdhciReadw(host, HOST_CTRL2_R));
    HDF_LOGE(": Emmc ctrl: 0x%04x | Multi cycle 0x%08x",
        SdhciReadw(host, EMMC_CTRL_R), SdhciReadl(host, MULTI_CYCLE_R));
    if (host->flags & SDHCI_USE_64BIT_ADMA) {
        HDF_LOGE(": ADMA Err: 0x%08x\n", SdhciReadl(host, ADMA_ERR_STAT_R));
        HDF_LOGE(": ADMA Addr(0:31): 0x%08x | ADMA Addr(32:63): 0x%08x",
            SdhciReadl(host, ADMA_SA_LOW_R), SdhciReadl(host, ADMA_SA_HIGH_R));
    } else if (host->flags & SDHCI_USE_ADMA) {
        HDF_LOGE(": ADMA Err: 0x%08x | ADMA Ptr: 0x%08x",
            SdhciReadl(host, ADMA_ERR_STAT_R), SdhciReadl(host, ADMA_SA_LOW_R));
    }
    HDF_LOGE(": ===========================================");
}

static void SdhciDmaCacheClean(void *addr, uint32_t size)
{
    addr = (void *)(uintptr_t)DMA_TO_VMM_ADDR((paddr_t)(uintptr_t)addr);
    uint32_t start = (uintptr_t)addr & ~(CACHE_ALIGNED_SIZE - 1);
    uint32_t end = (uintptr_t)addr + size;

    end = ALIGN(end, CACHE_ALIGNED_SIZE);
    DCacheFlushRange(start, end);
}

static void SdhciDmaCacheInv(void *addr, uint32_t size)
{
    addr = (void *)(uintptr_t)DMA_TO_VMM_ADDR((paddr_t)(uintptr_t)addr);
    uint32_t start = (uintptr_t)addr & ~(CACHE_ALIGNED_SIZE - 1);
    uint32_t end = (uintptr_t)addr + size;

    end = ALIGN(end, CACHE_ALIGNED_SIZE);
    DCacheInvRange(start, end);
}

static void SdhciEnablePlugIrq(struct SdhciHost *host, uint32_t irq)
{
    SdhciWritel(host, irq, NORMAL_INT_STAT_EN_R);
    SdhciWritel(host, irq, NORMAL_INT_SIGNAL_EN_R);
}

static void SdhciSetCardDetection(struct SdhciHost *host, bool enable)
{
    uint32_t present;

    if (host->mmc->caps.bits.nonremovable > 0 ||
        host->quirks.bits.forceSWDetect > 0 ||
        host->quirks.bits.brokenCardDetection > 0) {
        return;
    }

    if (enable == true) {
        present = SdhciReadl(host, PSTATE_R) & SDHCI_CARD_PRESENT;
        host->irqEnable |= (present ? SDHCI_INTERRUPT_CARD_REMOVE : SDHCI_INTERRUPT_CARD_INSERT);
    } else {
        host->irqEnable &= (~(SDHCI_INTERRUPT_CARD_REMOVE | SDHCI_INTERRUPT_CARD_INSERT));
    }
}

static void SdhciEnableCardDetection(struct SdhciHost *host)
{
    SdhciSetCardDetection(host, true);
    SdhciWritel(host, host->irqEnable, NORMAL_INT_STAT_EN_R);
    SdhciWritel(host, host->irqEnable, NORMAL_INT_SIGNAL_EN_R);
}

static void SdhciReset(struct SdhciHost *host, uint32_t mask)
{
    uint32_t i;
    uint8_t reg;

    SdhciWriteb(host, mask, SW_RST_R);
    /* hw clears the bit when it's done */
    for (i = 0; i < SDHCI_RESET_RETRY_TIMES; i++) {
        reg = SdhciReadb(host, SW_RST_R);
        if ((reg & mask) == 0) {
            return;
        }
        OsalMDelay(1);
    }

    HDF_LOGE("host%d: Reset 0x%x never completed.", host->hostId, mask);
    SdhciDumpregs(host);
}

static void SdhciDoReset(struct SdhciHost *host, uint32_t mask)
{
    SdhciReset(host, mask);

    if ((mask & SDHCI_RESET_ALL) > 0) {
        host->presetEnabled = false;
        host->clock = 0;
    }
}

static void SdhciSetTransferMode(struct SdhciHost *host, struct MmcCmd *cmd)
{
    uint16_t mode;

    if (cmd->data == NULL) {
        mode = SdhciReadw(host, XFER_MODE_R);
        SdhciWritew(host, (mode & (~(SDHCI_TRNS_AUTO_CMD12 | SDHCI_TRNS_AUTO_CMD23))), XFER_MODE_R);
        return;
    }

    mode = SDHCI_TRNS_BLK_CNT_EN;
    if (cmd->cmdCode == WRITE_MULTIPLE_BLOCK || cmd->cmdCode == READ_MULTIPLE_BLOCK || cmd->data->blockNum > 1) {
        mode |= SDHCI_TRNS_MULTI;
        if ((host->mmc->devType == MMC_DEV_SD && (MmcCntlrSdSupportCmd23(host->mmc) == false)) ||
            (host->mmc->devType == MMC_DEV_EMMC && (MmcCntlrEmmcSupportCmd23(host->mmc) == false))) {
            mode |= SDHCI_TRNS_AUTO_CMD12;
        } else {
            if ((host->flags & SDHCI_AUTO_CMD23)) {
                mode |= SDHCI_TRNS_AUTO_CMD23;
                SdhciWritel(host, cmd->data->blockNum, SDHCI_ARGUMENT2);
            } else if ((host->flags & SDHCI_AUTO_CMD12) > 0) {
                mode |= SDHCI_TRNS_AUTO_CMD12;
            }
        }
    }

    if ((cmd->data->dataFlags & DATA_READ) > 0) {
        mode |= SDHCI_TRNS_READ;
    }
    if ((host->flags & SDHCI_REQ_USE_DMA) > 0) {
        mode |= SDHCI_TRNS_DMA;
    }
    SdhciWritew(host, mode, XFER_MODE_R);
}

static void SdhciTaskletFinish(struct SdhciHost *host)
{
    struct MmcCmd *cmd = host->cmd;

    if (!(host->flags & SDHCI_DEVICE_DEAD) &&
        ((cmd->returnError != 0) || (cmd->data != NULL && cmd->data->returnError != 0))) {
        SdhciDoReset(host, SDHCI_RESET_CMD);
        SdhciDoReset(host, SDHCI_RESET_DATA);
    }

    host->cmd = NULL;
    (void)SDHCI_EVENT_SIGNAL(&host->sdhciEvent, SDHCI_PEND_REQUEST_DONE);
}

static uint32_t SdhciGenerateCmdFlag(struct MmcCmd *cmd)
{
    uint32_t flags;

    if ((cmd->respType & RESP_PRESENT) == 0) {
        flags = SDHCI_CMD_NONE_RESP;
    } else if ((cmd->respType & RESP_136) > 0) {
        flags = SDHCI_CMD_LONG_RESP;
    } else if ((cmd->respType & RESP_BUSY) > 0) {
        flags = SDHCI_CMD_SHORT_RESP_BUSY;
    } else {
        flags = SDHCI_CMD_SHORT_RESP;
    }

    if ((cmd->respType & RESP_CRC) > 0) {
        flags |= SDHCI_CMD_CRC_CHECK_ENABLE;
    }
    if ((cmd->respType & RESP_CMDCODE) > 0) {
        flags |= SDHCI_CMD_INDEX_CHECK_ENABLE;
    }

    /* CMD19 is special in that the Data Present Select should be set */
    if (cmd->data != NULL || cmd->cmdCode == SD_CMD_SEND_TUNING_BLOCK || cmd->cmdCode == SEND_TUNING_BLOCK_HS200) {
        flags |= SDHCI_CMD_DATA_TX;
    }

    return flags;
}

static void SdhciSetDmaConfig(struct SdhciHost *host)
{
    uint8_t ctrl;

    if (host->version < SDHCI_HOST_SPEC_200) {
        return;
    }

    ctrl = SdhciReadb(host, HOST_CTRL1_R);
    ctrl &= ~SDHCI_CTRL_DMA_ENABLE_MASK;
    if ((host->flags & SDHCI_REQ_USE_DMA) && (host->flags & SDHCI_USE_ADMA)) {
        if (host->flags & SDHCI_USE_64BIT_ADMA) {
            ctrl |= SDHCI_CTRL_ADMA64_ENABLE;
        } else {
            ctrl |= SDHCI_CTRL_ADMA32_ENABLE;
        }
    } else {
        ctrl |= SDHCI_CTRL_SDMA_ENABLE;
    }
    SdhciWriteb(host, ctrl, HOST_CTRL1_R);
}

static void SdhciSetTransferIrqs(struct SdhciHost *host)
{
    uint32_t pioIrqs = SDHCI_INTERRUPT_DATA_AVAIL | SDHCI_INTERRUPT_SPACE_AVAIL;
    uint32_t dmaIrqs = SDHCI_INTERRUPT_DMA_END | SDHCI_INTERRUPT_ADMA_ERROR;

    if (host->flags & SDHCI_REQ_USE_DMA) {
        host->irqEnable = (host->irqEnable & ~pioIrqs) | dmaIrqs;
    } else {
        host->irqEnable = (host->irqEnable & ~dmaIrqs) | pioIrqs;
    }
    SdhciEnablePlugIrq(host, host->irqEnable);
}

static void SdhciSetBlkSizeReg(struct SdhciHost *host, uint32_t blksz, uint32_t sdmaBoundary)
{
    if (host->flags & SDHCI_USE_ADMA) {
        SdhciWritel(host, SDHCI_MAKE_BLKSZ(7, blksz), BLOCKSIZE_R);
    } else {
        SdhciWritel(host, SDHCI_MAKE_BLKSZ(sdmaBoundary, blksz), BLOCKSIZE_R);
    }
}

static void SdhciAdmaConfig(struct SdhciHost *host)
{
    SdhciWritel(host, 0, (uintptr_t)ADMA_SA_HIGH_R);
    SdhciWritel(host, VMM_TO_DMA_ADDR((uintptr_t)host->admaDesc), (uintptr_t)ADMA_SA_LOW_R);
}

static void SdhciSetAdmaDesc(struct SdhciHost *host, char *desc, dma_addr_t addr, uint16_t len, uint16_t cmd)
{
    uint16_t *cmdlen = (uint16_t *)desc;

    cmdlen[0] = (cmd);
    cmdlen[1] = (len);

    if (host->flags & SDHCI_USE_64BIT_ADMA) {
        unsigned long *dataddr = (unsigned long*)(desc + 4);
        dataddr[0] = (addr);
    } else {
        uint32_t *dataddr = (uint32_t *)(desc + 4);
        dataddr[0] = (addr);
    }
}

static void SdhciAdmaMarkEnd(void *desc)
{
    uint16_t *d = (uint16_t *)desc;

    d[0] |= ADMA2_END;
}

static int32_t SdhciAdmaTablePre(struct SdhciHost *host, struct MmcData *data)
{
    char *admaDesc = NULL;
    dma_addr_t addr;
    uint32_t len, i;
    uint32_t dmaDir = DMA_TO_DEVICE;

    if (data->dataFlags & DATA_READ) {
        dmaDir = DMA_FROM_DEVICE;
    }

    admaDesc = host->admaDesc;
    (void)memset_s(admaDesc, ALIGN(host->admaDescSize, CACHE_ALIGNED_SIZE),
        0, ALIGN(host->admaDescSize, CACHE_ALIGNED_SIZE));

    for (i = 0; i < host->dmaSgCount; i++) {
        addr = SDHCI_SG_DMA_ADDRESS(&host->sg[i]);
        len = SDHCI_SG_DMA_LEN(&host->sg[i]);
        if (dmaDir == DMA_TO_DEVICE) {
            SdhciDmaCacheClean((void*)addr, len);
        } else {
            SdhciDmaCacheInv((void*)addr, len);
        }

        while (len > 0) {
            if (len > SDHCI_DMA_MAX_BUFF_SIZE) {
                SdhciSetAdmaDesc(host, admaDesc, addr, SDHCI_DMA_MAX_BUFF_SIZE, 0x21);
                len -= SDHCI_DMA_MAX_BUFF_SIZE;
                addr += SDHCI_DMA_MAX_BUFF_SIZE;
            } else {
                SdhciSetAdmaDesc(host, admaDesc, addr, len, 0x21);
                len = 0;
            }
            admaDesc += host->admaDescLineSize;
        }

        if ((uint32_t)(admaDesc - host->admaDesc) > host->admaDescSize) {
            HDF_LOGE("check wrong!");
        }
    }

    if (host->quirks.bits.noEndattrInNopdesc) {
        /* Mark the last descriptor as the terminating descriptor */
        if (admaDesc != host->admaDesc) {
            admaDesc -= host->admaDescLineSize;
            SdhciAdmaMarkEnd(admaDesc);
        }
    } else {
        SdhciSetAdmaDesc(host, admaDesc, 0, 0, 0x3);
    }
    SdhciDmaCacheClean((void*)VMM_TO_DMA_ADDR((uintptr_t)host->admaDesc),
        ALIGN(host->admaDescSize, CACHE_ALIGNED_SIZE));

    return 0;
}

static void SdhciPrepareData(struct SdhciHost *host)
{
    struct MmcCmd *cmd = host->cmd;
    struct MmcData *data = cmd->data;
    int32_t retval;

    /* set timeout value , use default value. */
    if (data != NULL || (cmd->respType & RESP_BUSY) > 0) {
        SdhciWriteb(host, SDHCI_DEFINE_TIMEOUT, TOUT_CTRL_R);
    }
    if (data == NULL) {
        return;
    }

    if (host->flags & (SDHCI_USE_ADMA | SDHCI_USE_SDMA)) {
        host->flags |= SDHCI_REQ_USE_DMA;
    }

    if (host->flags & SDHCI_REQ_USE_DMA) {
        if (host->flags & SDHCI_USE_ADMA) {
            /* ADMA config */
            retval = SdhciAdmaTablePre(host, data);
            if (retval) {
                host->flags &= ~SDHCI_REQ_USE_DMA;
            } else {
                SdhciWritel(host, 0, SDHCI_DMA_ADDRESS);
                SdhciAdmaConfig(host);
            }
        } else {
            /* SDMA config */
            SdhciWritel(host, SDHCI_SG_DMA_ADDRESS(&host->sg[0]), SDHCI_DMA_ADDRESS);
        }
    }

    SdhciSetDmaConfig(host);
    SdhciSetTransferIrqs(host);
    SdhciSetBlkSizeReg(host, data->blockSize, SDHCI_DEFAULT_BOUNDARY_ARG);
    SdhciWritew(host, data->blockNum, BLOCKCOUNT_R);
}

static void SdhciExecCmd(struct SdhciHost *host, struct MmcCmd *cmd)
{
    uint32_t mask, flags;
    uint32_t timeout = 10; /* 10ms */

    mask = SDHCI_CMD_INVALID;
    if ((cmd->data != NULL) || ((cmd->respType & RESP_BUSY) > 0)) {
        mask |= SDHCI_DATA_INVALID;
    }

    if ((cmd->data != NULL) && (cmd->data->sendStopCmd == true)) {
        mask &= ~SDHCI_DATA_INVALID;
    }

    /* wait host ready */
    while (SdhciReadl(host, PSTATE_R) & mask) {
        if (timeout == 0) {
            HDF_LOGE("exec cmd %d timeout!\n", cmd->cmdCode);
            SdhciDumpregs(host);
            cmd->returnError = HDF_ERR_IO;
            SdhciTaskletFinish(host);
            return;
        }
        timeout--;
        OsalMDelay(1);
    }

    host->cmd = cmd;
    SdhciPrepareData(host);
    SdhciWritel(host, cmd->argument, ARGUMENT_R);
    SdhciSetTransferMode(host, cmd);

    if ((cmd->respType & RESP_136) && (cmd->respType & RESP_BUSY)) {
        HDF_LOGE("host%d: Unsupported response type!", host->hostId);
        cmd->returnError = HDF_FAILURE;
        SdhciTaskletFinish(host);
        return;
    }

    flags = SdhciGenerateCmdFlag(cmd);
    SdhciWritew(host, SDHCI_GEN_CMD(cmd->cmdCode, flags), CMD_R);
}

static bool SdhciCardPluged(struct MmcCntlr *cntlr)
{
    struct SdhciHost *host = NULL;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return false;
    }

    if (cntlr->devType == MMC_DEV_SDIO || cntlr->devType == MMC_DEV_EMMC) {
        return true;
    }

    host = (struct SdhciHost *)cntlr->priv;
    if (host->quirks.bits.brokenCardDetection > 0 ||
        host->quirks.bits.forceSWDetect > 0) {
        return true;
    }
    return ((SdhciReadl(host, PSTATE_R) & SDHCI_CARD_PRESENT) > 0 ? true : false);
}

static int32_t SdhciFillDmaSg(struct SdhciHost *host)
{
    struct MmcData *data = host->cmd->data;
    uint32_t len = data->blockNum * data->blockSize;
    int32_t ret;

    if (len == 0) {
        return HDF_ERR_INVALID_PARAM;
    }

    if (data->scatter != NULL && data->dataBuffer == NULL) {
        host->sg = data->scatter;
        host->dmaSgCount = data->scatterLen;
        return HDF_SUCCESS;
    }
    if (data->dataBuffer == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }

    host->alignedBuff = (uint8_t *)OsalMemAllocAlign(CACHE_ALIGNED_SIZE, ALIGN(len, CACHE_ALIGNED_SIZE));
    if (host->alignedBuff == NULL) {
        HDF_LOGE("out of memory.");
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = memcpy_s(host->alignedBuff, len, data->dataBuffer, len);
    if (ret != EOK) {
        HDF_LOGE("memcpy_s fail ret = %d.", ret);
        free(host->alignedBuff);
        host->alignedBuff = NULL;
        return HDF_FAILURE;
    }
    host->buffLen = len;
    sg_init_one(&host->dmaSg, (const void *)host->alignedBuff, len);
    host->dmaSgCount = 1;
    host->sg = &host->dmaSg;
    return HDF_SUCCESS;
}

static void SdhciClearDmaSg(struct SdhciHost *host, struct MmcData *data)
{
    uint32_t len;

    if (data == NULL) {
        return;
    }

    len = data->blockNum * data->blockSize;
    if (host->alignedBuff != NULL && data->dataBuffer != NULL && len > 0 && host->buffLen > 0) {
        if ((data->dataFlags & DATA_READ) > 0) {
            (void)memcpy_s(data->dataBuffer, len, host->alignedBuff, host->buffLen);
        }
    }
    if (host->alignedBuff != NULL) {
        OsalMemFree(host->alignedBuff);
        host->alignedBuff = NULL;
    }
    host->buffLen = 0;
    host->dmaSgCount = 0;
    host->sg = NULL;
}

static void SdhciDataSync(struct SdhciHost *host, struct MmcData *data)
{
    uint32_t sgPhyAddr, sgLength, i;

    if ((data->dataFlags & DATA_READ) > 0) {
        for (i = 0; i < host->dmaSgCount; i++) {
            sgLength = SDHCI_SG_DMA_LEN(&host->sg[i]);
            sgPhyAddr = SDHCI_SG_DMA_ADDRESS(&host->sg[i]);
            SdhciDmaCacheInv((void *)(uintptr_t)sgPhyAddr, sgLength);
        }
    }
}

static int32_t SdhciDoRequest(struct MmcCntlr *cntlr, struct MmcCmd *cmd)
{
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;
    int32_t ret;
    unsigned long flags = 0;
    uint32_t timeout = SDHCI_CMD_DATA_REQ_TIMEOUT;

    (void)OsalMutexLock(&host->mutex);

    host->cmd = cmd;
    if (cmd->data != NULL) {
        ret = SdhciFillDmaSg(host);
        if (ret != HDF_SUCCESS) {
            goto _END;
        }
    }

    if (SdhciCardPluged(host->mmc) == false || (host->flags & SDHCI_DEVICE_DEAD) > 0) {
        cmd->returnError = HDF_ERR_IO;
        HDF_LOGE("card is not present!");
        SdhciEnableCardDetection(host);
        ret = HDF_ERR_IO;
        goto _END;
    } else {
        SdhciExecCmd(host, cmd);
    }

    SDHCI_IRQ_LOCK(&flags);
    host->waitForEvent = true;
    SDHCI_IRQ_UNLOCK(flags);

    if (cmd->data == NULL && host->quirks.bits.forceSWDetect > 0) {
        timeout = SDHCI_CMD_REQ_TIMEOUT;
    }

    ret = SDHCI_EVENT_WAIT(&host->sdhciEvent, SDHCI_PEND_REQUEST_DONE | SDHCI_PEND_ACCIDENT, timeout);

    SDHCI_IRQ_LOCK(&flags);
    host->waitForEvent = false;
    SDHCI_IRQ_UNLOCK(flags);

    if (ret != SDHCI_PEND_REQUEST_DONE) {
        SdhciDumpregs(host);
        cmd->returnError = HDF_ERR_TIMEOUT;
        if (ret == SDHCI_PEND_ACCIDENT) {
            cmd->returnError = HDF_ERR_IO;
        }
        SdhciDoReset(host, SDHCI_RESET_CMD);
        SdhciDoReset(host, SDHCI_RESET_DATA);
        host->cmd = NULL;
    } else if (cmd->data != NULL) {
        SdhciDataSync(host, cmd->data);
    }
    ret = HDF_SUCCESS;
_END:
    SdhciClearDmaSg(host, cmd->data);
    (void)OsalMutexUnlock(&host->mutex);
    return ret;
}

void SdhciResetDll(struct SdhciHost *host)
{
    uint32_t reg, value;
    uint32_t cfgArray[] = { PERI_CRG125, PERI_CRG139 };

    if (host->hostId >= SDHCI_MAX_HOST_NUM) {
        return;
    }

    reg = cfgArray[host->hostId];
    value = OSAL_READL((uintptr_t)reg);
    value |= SDHCI_EMMC_DLL_RST;
    OSAL_WRITEL(value, (uintptr_t)reg);
}

static uint32_t SdhciSelectClock(struct SdhciHost *host, uint32_t clock)
{
    uint32_t reg, realClock, value;
    uint32_t temp = 0;
    uint32_t cfgArray[] = { PERI_CRG125, PERI_CRG139 };

    if (host->hostId >= SDHCI_MAX_HOST_NUM) {
        HDF_LOGE("host id=%d is not supported!", host->hostId);
        return 0;
    }

    if (host->hostId == 1 && (clock > SDHCI_MMC_FREQ_50M)) {
        HDF_LOGE("host%d doesn't support freq %d!", host->hostId, clock);
        return 0;
    }

    if (clock >= SDHCI_MMC_FREQ_150M) {
        temp |= SDHCI_CLK_SEL_150M;
        realClock = SDHCI_MMC_FREQ_150M;
    } else if (clock >= SDHCI_MMC_FREQ_112P5M) {
        temp |= SDHCI_CLK_SEL_112P5M;
        realClock = SDHCI_MMC_FREQ_112P5M;
    } else if (clock >= SDHCI_MMC_FREQ_90M) {
        temp |= SDHCI_CLK_SEL_90M;
        realClock = SDHCI_MMC_FREQ_90M;
    } else if (clock >= SDHCI_MMC_FREQ_50M) {
        temp |= SDHCI_CLK_SEL_50M;
        realClock = SDHCI_MMC_FREQ_50M;
    } else if (clock >= SDHCI_MMC_FREQ_25M) {
        temp |= SDHCI_CLK_SEL_25M;
        realClock = SDHCI_MMC_FREQ_25M;
    } else if (clock >= SDHCI_MMC_FREQ_400K) {
        temp |= SDHCI_CLK_SEL_400K;
        realClock = SDHCI_MMC_FREQ_400K;
    } else if (clock >= SDHCI_MMC_FREQ_100K) {
        temp = SDHCI_CLK_SEL_100K;
        realClock = SDHCI_MMC_FREQ_100K;
    } else {
        temp = SDHCI_CLK_SEL_100K;
        realClock = 0;
    }

    reg = cfgArray[host->hostId];
    value = OSAL_READL((uintptr_t)reg);
    value &= ~(SDHCI_MMC_FREQ_MASK << SDHCI_MMC_FREQ_SHIFT);
    value |= ((temp & SDHCI_MMC_FREQ_MASK) << SDHCI_MMC_FREQ_SHIFT);
    value |= SDHCI_CKEN;
    OSAL_WRITEL(value, (uintptr_t)reg);

    return realClock;
}

static void SdhciSetDrvPhase(uint32_t id, uint32_t phase)
{
    uint32_t value;
    uint32_t drv[] = { PERI_CRG127, PERI_CRG136 };

    if (id >= SDHCI_MAX_HOST_NUM) {
        return;
    }

    value = OSAL_READL(drv[id]);
    value &= (~SDHCI_DRV_CLK_PHASE_MASK);
    value |= (phase << SDHCI_DRV_CLK_PHASE_SHFT);
    OSAL_WRITEL(value, drv[id]);
}

static void SdhciEnableSample(struct SdhciHost *host)
{
    uint32_t val;

    val = SdhciReadl(host, AT_CTRL_R);
    val |= SDHCI_SW_TUNING_EN;
    SdhciWritel(host, val, AT_CTRL_R);
}

static void SdhciSetSampPhase(struct SdhciHost *host, uint32_t phase)
{
    uint32_t val;
    val = SdhciReadl(host, AT_STAT_R);
    val = SdhciReadl(host, AT_STAT_R);
    val &= ~SDHCI_CENTER_PH_CODE_MASK;
    val |= phase;
    SdhciWritel(host, val, AT_STAT_R);
}

static void SdhciSetIo(uint32_t offset, uint32_t val)
{
    uint32_t reg;

    reg = OSAL_READL(offset);
    reg &= ~IO_DRV_MASK;
    reg |= val & IO_DRV_MASK;
    OSAL_WRITEL(reg, offset);
}

static void SdhciSetIodriver(uint32_t offset, uint32_t pull, uint32_t sr, uint32_t drv)
{
    SdhciSetIo(offset, pull | (sr ? IO_CFG_SR : 0) | IO_DRV_STR_SEL(drv));
}

static void SdhciSetSdiodriver(struct SdhciHost *host)
{
    uint32_t i, count;
    uint32_t dataRegs[] = { REG_CTRL_SDIO_DATA0, REG_CTRL_SDIO_DATA1, REG_CTRL_SDIO_DATA2, REG_CTRL_SDIO_DATA3 };

    count = sizeof(dataRegs) / sizeof(dataRegs[0]);
    if (host->mmc->caps.bits.cap4Bit == 0) {
        /* only 1 pin can be initialized, because other pins are reserved to configured as other functions. */
        count = 1;
    }
    SdhciSetIodriver(REG_CTRL_SDIO_CLK, IO_CFG_PULL_DOWN, IO_CFG_SR, IO_DRV_SDIO_CLK);
    SdhciSetIodriver(REG_CTRL_SDIO_CMD, IO_CFG_PULL_UP, 0, IO_DRV_SDIO_CMD);
    for (i = 0; i < count; i++) {
        SdhciSetIodriver(dataRegs[i], IO_CFG_PULL_UP, 0, IO_DRV_SDIO_DATA);
    }
}

static void SdhciSetSdDriver(struct SdhciHost *host, enum MmcBusTiming timing)
{
    uint32_t i, count;
    uint32_t dataRegs[] = { REG_CTRL_SD_DATA0, REG_CTRL_SD_DATA1, REG_CTRL_SD_DATA2, REG_CTRL_SD_DATA3 };

    count = sizeof(dataRegs) / sizeof(dataRegs[0]);
    if (host->mmc->caps.bits.cap4Bit == 0) {
        /* only 1-pin GPIO can be initialized, because other pins are reserved to configured as other functions. */
        count = 1;
    }

    switch (timing) {
        case BUS_TIMING_SD_HS:
            SdhciSetIodriver(REG_CTRL_SD_CLK, IO_CFG_PULL_DOWN, IO_CFG_SR, IO_DRV_SD_SDHS_CLK);
            SdhciSetIodriver(REG_CTRL_SD_CMD, IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_SD_SDHS_CMD);
            for (i = 0; i < count; i++) {
                SdhciSetIodriver(dataRegs[i], IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_SD_SDHS_DATA);
            }
            break;
        default:
            SdhciSetIodriver(REG_CTRL_SD_CLK, IO_CFG_PULL_DOWN, IO_CFG_SR, IO_DRV_SD_OTHER_CLK);
            SdhciSetIodriver(REG_CTRL_SD_CMD, IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_SD_OTHER_CMD);
            for (i = 0; i < count; i++) {
                SdhciSetIodriver(dataRegs[i], IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_SD_OTHER_DATA);
            }
            break;
    }
}

static void SdhciSetEmmcCtrl(struct SdhciHost *host)
{
    unsigned int val;

    val = SdhciReadl(host, EMMC_CTRL_R);
    val |= SDHCI_EMMC_CTRL_EMMC;
    SdhciWritel(host, val, EMMC_CTRL_R);
}

static void SdhciSetEmmcDriver(struct SdhciHost *host, enum MmcBusTiming timing)
{
    uint32_t i, count;
    uint32_t dataRegs[] = { REG_CTRL_EMMC_DATA0, REG_CTRL_EMMC_DATA1, REG_CTRL_EMMC_DATA2, REG_CTRL_EMMC_DATA3 };

    count = sizeof(dataRegs) / sizeof(dataRegs[0]);
    switch (timing) {
        case BUS_TIMING_MMC_HS400:
            SdhciSetEmmcCtrl(host);
            SdhciSetIodriver(REG_CTRL_EMMC_CLK, IO_CFG_PULL_DOWN, 0, IO_DRV_EMMC_HS400_CLK);
            SdhciSetIodriver(REG_CTRL_EMMC_CMD, IO_CFG_PULL_UP, 0, IO_DRV_EMMC_HS400_CMD);
            for (i = 0; i < count; i++) {
                SdhciSetIodriver(dataRegs[i], IO_CFG_PULL_UP, 0, IO_DRV_EMMC_HS400_DATA);
            }
            SdhciSetIodriver(REG_CTRL_EMMC_DS, IO_CFG_PULL_DOWN, IO_CFG_SR, IO_DRV_EMMC_HS400_DS);
            SdhciSetIodriver(REG_CTRL_EMMC_RST, IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_EMMC_HS400_RST);
            break;
        case BUS_TIMING_MMC_HS200:
            SdhciSetEmmcCtrl(host);
            SdhciSetIodriver(REG_CTRL_EMMC_CLK, IO_CFG_PULL_DOWN, 0, IO_DRV_EMMC_HS200_CLK);
            SdhciSetIodriver(REG_CTRL_EMMC_CMD, IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_EMMC_HS200_CMD);
            for (i = 0; i < count; i++) {
                SdhciSetIodriver(dataRegs[i], IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_EMMC_HS200_DATA);
            }
            SdhciSetIodriver(REG_CTRL_EMMC_RST, IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_EMMC_HS200_RST);
            break;
        case BUS_TIMING_MMC_HS:
            SdhciSetEmmcCtrl(host);
            SdhciSetIodriver(REG_CTRL_EMMC_CLK, IO_CFG_PULL_DOWN, IO_CFG_SR, IO_DRV_EMMC_HS_CLK);
            SdhciSetIodriver(REG_CTRL_EMMC_CMD, IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_EMMC_HS_CMD);
            for (i = 0; i < count; i++) {
                SdhciSetIodriver(dataRegs[i], IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_EMMC_HS_DATA);
            }
            SdhciSetIodriver(REG_CTRL_EMMC_RST, IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_EMMC_HS_RST);
            break;
        default:
            SdhciSetIodriver(REG_CTRL_EMMC_CLK, IO_CFG_PULL_DOWN, IO_CFG_SR, IO_DRV_EMMC_OTHER_CLK);
            SdhciSetIodriver(REG_CTRL_EMMC_CMD, IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_EMMC_OTHER_CMD);
            for (i = 0; i < count; i++) {
                SdhciSetIodriver(dataRegs[i], IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_EMMC_OTHER_DATA);
            }
            SdhciSetIodriver(REG_CTRL_EMMC_RST, IO_CFG_PULL_UP, IO_CFG_SR, IO_DRV_EMMC_OTHER_RST);
            break;
    }
}

static void SdhciSetMmcIoDriver(struct SdhciHost *host, enum MmcBusTiming timing)
{
    uint32_t val;

    if (host->hostId == 0) {
        /* mmc0: eMMC or SD card */
        val = OSAL_READL(REG_CTRL_EMMC_CLK) & IO_MUX_MASK;
        if (val == IO_MUX_SHIFT(IO_MUX_CLK_TYPE_EMMC)) {
            SdhciSetEmmcDriver(host, timing);
        }
        val = OSAL_READL(REG_CTRL_SD_CLK) & IO_MUX_MASK;
        if (val == IO_MUX_SHIFT(IO_MUX_CLK_TYPE_SD)) {
            SdhciSetSdDriver(host, timing);
        }
    } else if (host->hostId == 1) {
        /* mmc1: SDIO */
        SdhciSetSdiodriver(host);
    }
}

static void SdhciSetPhase(struct SdhciHost *host)
{
    uint32_t drvPhase, phase;
    enum MmcBusTiming timing;

    if (host->mmc->curDev == NULL) {
        return;
    }
    timing = host->mmc->curDev->workPara.timing;

    if (host->hostId == 0) {
        /* eMMC or SD card */
        if (timing == BUS_TIMING_MMC_HS400) {
            drvPhase = SDHCI_PHASE_112P5_DEGREE;
            phase = host->tuningPhase;
        } else if (timing == BUS_TIMING_MMC_HS200) {
            drvPhase = SDHCI_PHASE_258P75_DEGREE;
            phase = host->tuningPhase;
        } else if (timing == BUS_TIMING_MMC_HS) {
            drvPhase = SDHCI_PHASE_180_DEGREE;
            phase = SDHCI_SAMPLE_PHASE;
        } else if (timing == BUS_TIMING_SD_HS) {
            drvPhase = SDHCI_PHASE_225_DEGREE;
            phase = SDHCI_SAMPLE_PHASE;
        } else if (timing == BUS_TIMING_MMC_DS) {
            drvPhase = SDHCI_PHASE_180_DEGREE;
            phase = 0;
        } else {
            drvPhase = SDHCI_PHASE_225_DEGREE;
            phase = SDHCI_SAMPLE_PHASE;
        }
    } else {
        /* SDIO device */
        if ((timing == BUS_TIMING_SD_HS) ||
            (timing == BUS_TIMING_UHS_SDR25)) {
            drvPhase = SDHCI_PHASE_180_DEGREE;
            phase = SDHCI_SAMPLE_PHASE;
        } else {
            /* UHS_SDR12 */
            drvPhase = SDHCI_PHASE_180_DEGREE;
            phase = 0;
        }
    }

    SdhciSetDrvPhase(host->hostId, drvPhase);
    SdhciEnableSample(host);
    SdhciSetSampPhase(host, phase);
    SdhciSetMmcIoDriver(host, timing);
}

static int32_t SdhciSetClock(struct MmcCntlr *cntlr, uint32_t clock)
{
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;
    uint16_t clk;
    uint32_t i, ret;

    /* turn off clk2card_on */
    clk = SdhciReadw(host, CLK_CTRL_R);
    clk &= ~(SDHCI_CLK_CTRL_CLK_EN);
    SdhciWritew(host, clk, CLK_CTRL_R);
    OsalUDelay(SDHCI_OFF_CLK2CARD_ON_DELAY);

    /* turn off card_clk_en */
    clk &= ~(SDHCI_CLK_CTRL_INT_CLK_EN | SDHCI_CLK_CTRL_PLL_EN);
    SdhciWritew(host, clk, CLK_CTRL_R);

    if (clock == 0) {
        return HDF_FAILURE;
    }
    if (clock >= host->maxClk) {
        clock = host->maxClk;
    }

    SdhciResetDll(host);
    SdhciSetPhase(host);

    ret = SdhciSelectClock(host, clock);
    if (ret == 0) {
        HDF_LOGE("Select clock fail!");
        return HDF_FAILURE;
    }
    host->clock = ret;
    if (cntlr->caps.bits.nonremovable == 0) {
        SdhciResetDll(host);
    }

    /* turn on card_clk_en */
    clk |= SDHCI_CLK_CTRL_INT_CLK_EN | SDHCI_CLK_CTRL_PLL_EN;
    SdhciWritew(host, clk, CLK_CTRL_R);
    for (i = 0; i < SDHCI_CLK_CTRL_RETRY_TIMES; i++) {
        clk = SdhciReadw(host, CLK_CTRL_R);
        if ((clk & SDHCI_CLK_CTRL_INT_STABLE) > 0) {
            break;
        }
        OsalMDelay(1);
    }
    if (i == SDHCI_CLK_CTRL_RETRY_TIMES) {
        HDF_LOGE("Internal clock never stabilized.");
        return HDF_ERR_TIMEOUT;
    }

    SdhciResetDll(host);
    /* turn on clk2card_on */
    clk |= SDHCI_CLK_CTRL_CLK_EN;
    SdhciWritew(host, clk, CLK_CTRL_R);
    return HDF_SUCCESS;
}

static void SdhciInit(struct SdhciHost *host, bool resetAll)
{
    uint32_t reg;
    if (resetAll == false) {
        SdhciDoReset(host, (SDHCI_RESET_CMD | SDHCI_RESET_DATA));
    } else {
        SdhciDoReset(host, SDHCI_RESET_ALL);
    }

    host->pwr = 0;
    host->irqEnable = SDHCI_INTERRUPT_BUS_POWER | SDHCI_INTERRUPT_DATA_END_BIT | SDHCI_INTERRUPT_DATA_CRC |
                      SDHCI_INTERRUPT_DATA_TIMEOUT | SDHCI_INTERRUPT_INDEX | SDHCI_INTERRUPT_END_BIT |
                      SDHCI_INTERRUPT_CRC | SDHCI_INTERRUPT_TIMEOUT | SDHCI_INTERRUPT_DATA_END |
                      SDHCI_INTERRUPT_RESPONSE | SDHCI_INTERRUPT_AUTO_CMD_ERR;

    SdhciEnablePlugIrq(host, host->irqEnable);

    if (resetAll == false) {
        host->clock = 0;
    } else {
        reg = SdhciReadw(host, MSHC_CTRL_R);
        reg &= ~SDHC_CMD_CONFLIT_CHECK;
        SdhciWritew(host, reg, MSHC_CTRL_R);

        reg = SdhciReadl(host, MBIU_CTRL_R);
        reg &= ~(SDHCI_GM_WR_OSRC_LMT_MASK | SDHCI_GM_RD_OSRC_LMT_MASK | SDHCI_UNDEFL_INCR_EN);
        reg |= SDHCI_GM_WR_OSRC_LMT_VAL | SDHCI_GM_RD_OSRC_LMT_VAL;
        SdhciWritel(host, reg, MBIU_CTRL_R);

        reg = SdhciReadl(host, MULTI_CYCLE_R);
        reg |= SDHCI_EDGE_DETECT_EN | SDHCI_DATA_DLY_EN;
        reg &= ~SDHCI_CMD_DLY_EN;
        SdhciWritel(host, reg, MULTI_CYCLE_R);
    }
}

static void SdhciReinit(struct SdhciHost *host)
{
    SdhciInit(host, true);

    if (host->flags & SDHCI_USING_RETUNING_TIMER) {
        host->flags &= ~SDHCI_USING_RETUNING_TIMER;
        host->mmc->maxBlkNum = SDHCI_MAX_BLK_NUM;
    }
    SdhciEnableCardDetection(host);
}

static void SdhciEnablePresetValue(struct SdhciHost *host, bool enable)
{
    uint32_t reg;

    if (host->version < SDHCI_HOST_SPEC_300) {
        return;
    }

    if (host->presetEnabled != enable) {
        reg = SdhciReadw(host, HOST_CTRL2_R);
        if (enable == true) {
            reg |= SDHCI_PRESET_VAL_ENABLE;
        } else {
            reg &= ~SDHCI_PRESET_VAL_ENABLE;
        }
        SdhciWritew(host, reg, HOST_CTRL2_R);

        if (enable == true) {
            host->flags |= SDHCI_PV_ENABLED;
        } else {
            host->flags &= ~SDHCI_PV_ENABLED;
        }
        host->presetEnabled = enable;
    }
}

static int32_t SdhciSetPowerMode(struct MmcCntlr *cntlr, enum MmcPowerMode mode)
{
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;
    uint8_t pwr = SDHCI_POWER_330;

    if (mode == MMC_POWER_MODE_POWER_OFF) {
        SdhciWritel(host, 0, NORMAL_INT_SIGNAL_EN_R);
        SdhciReinit(host);
    } else {
        if (host->version >= SDHCI_HOST_SPEC_300) {
            SdhciEnablePresetValue(host, false);
        }
        if (host->pwr == SDHCI_POWER_330) {
            return HDF_SUCCESS;
        }

        host->pwr = pwr;

        SdhciWriteb(host, 0, PWR_CTRL_R);
        SdhciWriteb(host, pwr, PWR_CTRL_R);
        pwr |= SDHCI_POWER_ON;
        SdhciWriteb(host, pwr, PWR_CTRL_R);
        OsalMDelay(10);
    }
    return HDF_SUCCESS;
}

static int32_t SdhciSetBusWidth(struct MmcCntlr *cntlr, enum MmcBusWidth width)
{
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;
    uint8_t val;

    val = SdhciReadb(host, HOST_CTRL1_R);
    if (width == BUS_WIDTH8) {
        val &= (~SDHCI_CTRL_4_BIT_BUS);
        if (host->version >= SDHCI_HOST_SPEC_300) {
            val |= SDHCI_CTRL_8_BIT_BUS;
        }
    } else {
        if (host->version >= SDHCI_HOST_SPEC_300) {
            val &= (~SDHCI_CTRL_8_BIT_BUS);
        }
        if (width == BUS_WIDTH4) {
            val |= SDHCI_CTRL_4_BIT_BUS;
        } else {
            val &= (~SDHCI_CTRL_4_BIT_BUS);
        }
    }
    SdhciWriteb(host, val, HOST_CTRL1_R);
    return HDF_SUCCESS;
}

static void SdhciSetUhsSignaling(struct SdhciHost *host, enum MmcBusTiming timing)
{
    uint32_t val;

    val = SdhciReadw(host, HOST_CTRL2_R);

    /* Select Bus Speed Mode for host */
    val &= (~SDHCI_UHS_MASK);
    if (timing == BUS_TIMING_MMC_HS200 || timing == BUS_TIMING_UHS_SDR104) {
        val |= SDHCI_UHS_SDR104;
    } else if (timing == BUS_TIMING_UHS_SDR12) {
        val |= SDHCI_UHS_SDR12;
    } else if (timing == BUS_TIMING_UHS_SDR25) {
        val |= SDHCI_UHS_SDR25;
    } else if (timing == BUS_TIMING_UHS_SDR50) {
        val |= SDHCI_UHS_SDR50;
    } else if (timing == BUS_TIMING_UHS_DDR50 || timing == BUS_TIMING_UHS_DDR52) {
        val |= SDHCI_UHS_DDR50;
    } else if (timing == BUS_TIMING_MMC_HS400) {
        val |= SDHCI_HS400;
    }
    SdhciWritew(host, val, HOST_CTRL2_R);
}

static int32_t SdhciSetBusTiming(struct MmcCntlr *cntlr, enum MmcBusTiming timing)
{
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;
    uint32_t clockVal, ctrl1Val, ctrl2Val;

    ctrl1Val = SdhciReadb(host, HOST_CTRL1_R);

    if (timing >= BUS_TIMING_MMC_HS) {
        ctrl1Val |= SDHCI_CTRL_HIGH_SPEED;
    }

    if (host->version < SDHCI_HOST_SPEC_300) {
        SdhciWriteb(host, ctrl1Val, HOST_CTRL1_R);
        return HDF_SUCCESS;
    }

    if (host->presetEnabled == false) {
        SdhciWriteb(host, ctrl1Val, HOST_CTRL1_R);
        ctrl2Val = SdhciReadw(host, HOST_CTRL2_R);
        ctrl2Val &= ~SDHCI_DRV_TYPE_MASK;

        SdhciWritew(host, ctrl2Val, HOST_CTRL2_R);
    } else {
        clockVal = SdhciReadw(host, CLK_CTRL_R);
        clockVal &= ~SDHCI_CLK_CTRL_CLK_EN;
        SdhciWritew(host, clockVal, CLK_CTRL_R);

        SdhciWriteb(host, ctrl1Val, HOST_CTRL1_R);
        (void)SdhciSetClock(cntlr, host->clock);
    }
    clockVal = SdhciReadw(host, CLK_CTRL_R);
    clockVal &= ~SDHCI_CLK_CTRL_CLK_EN;
    SdhciWritew(host, clockVal, CLK_CTRL_R);

    SdhciSetUhsSignaling(host, timing);
    if (timing > BUS_TIMING_UHS_SDR12 && timing <= BUS_TIMING_UHS_DDR50) {
        SdhciEnablePresetValue(host, true);
    }
    (void)SdhciSetClock(cntlr, host->clock);
    return HDF_SUCCESS;
}

static int32_t SdhciSetSdioIrq(struct MmcCntlr *cntlr, bool enable)
{
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;

    if (enable == true) {
        host->flags |= SDHCI_SDIO_IRQ_ENABLED;
    } else {
        host->flags &= ~SDHCI_SDIO_IRQ_ENABLED;
    }

    if ((host->flags & SDHCI_DEVICE_DEAD) == 0) {
        if (enable == true) {
            host->irqEnable |= SDHCI_INTERRUPT_CARD_INT;
        } else {
            host->irqEnable &= ~SDHCI_INTERRUPT_CARD_INT;
        }
        SdhciEnablePlugIrq(host, host->irqEnable);
    }
    return HDF_SUCCESS;
}

static int32_t SdhciHardwareReset(struct MmcCntlr *cntlr)
{
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;

    SdhciWritel(host, 0x0, EMMC_HW_RESET_R);
    OsalUDelay(10);
    SdhciWritel(host, 0x1, EMMC_HW_RESET_R);
    OsalUDelay(200);

    return HDF_SUCCESS;
}

static int32_t SdhciSystemInit(struct MmcCntlr *cntlr)
{
    struct SdhciHost *host = NULL;

    if (cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct SdhciHost *)cntlr->priv;
    SdhciInit(host, true);
    SdhciEnableCardDetection(host);
    return HDF_SUCCESS;
}

static int32_t SdhciSetEnhanceSrobe(struct MmcCntlr *cntlr, bool enable)
{
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;
    uint32_t reg;

    reg = SdhciReadl(host, EMMC_CTRL_R);
    if (enable == true) {
        reg |= SDHCI_EMMC_CTRL_ENH_STROBE_EN;
    } else {
        reg &= ~SDHCI_EMMC_CTRL_ENH_STROBE_EN;
    }
    SdhciWritel(host, reg, EMMC_CTRL_R);

    reg = SdhciReadl(host, MULTI_CYCLE_R);
    if (enable == true) {
        reg |= SDHCI_CMD_DLY_EN;
    } else {
        reg &= ~SDHCI_CMD_DLY_EN;
    }
    SdhciWritel(host, reg, MULTI_CYCLE_R);
    return HDF_SUCCESS;
}

static int32_t SdhciSwitchVoltage(struct MmcCntlr *cntlr, enum MmcVolt volt)
{
    uint16_t ctrl;
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;

    if (cntlr->devType == MMC_DEV_EMMC) {
        ctrl = SdhciReadw(host, HOST_CTRL2_R);
        if (volt == VOLT_1V8) {
            ctrl |= SDHCI_VDD_180;
            SdhciWritew(host, ctrl, HOST_CTRL2_R);
        } else {
            ctrl &= ~SDHCI_VDD_180;
            SdhciWritew(host, ctrl, HOST_CTRL2_R);
        }
    } else {
        if (volt == VOLT_3V3) {
            return HDF_SUCCESS;
        }
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static bool SdhciDevReadOnly(struct MmcCntlr *cntlr)
{
    uint32_t val;
    bool readOnly = true;
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;

    if (host->flags & SDHCI_DEVICE_DEAD) {
        readOnly = false;
    } else {
        val = SdhciReadl(host, PSTATE_R);
        readOnly = ((val & SDHCI_WRITE_PROTECT) > 0 ? false : true);
    }

    if (host->quirks.bits.invertedWriteProtect > 0) {
        return (!readOnly);
    }
    return readOnly;
}

static bool SdhciDevBusy(struct MmcCntlr *cntlr)
{
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;
    uint32_t state;

    /* Check whether DAT[0] is 0 */
    state = SdhciReadl(host, PSTATE_R);
    return !(state & SDHCI_DATA_0_LEVEL_MASK);
}

static void SdhciWaitDrvDllLock(uint32_t id)
{
    uint32_t i, val;
    uint32_t offset[] = { PERI_SD_DRV_DLL_CTRL, PERI_SDIO_DRV_DLL_CTRL };

    if (id >= (sizeof(offset) / sizeof(offset[0]))) {
        return;
    }

    for (i = 0; i < SDHCI_CLK_CTRL_RETRY_TIMES; i++) {
        val = OSAL_READL(offset[id]);
        if ((val & SDHCI_DRV_DLL_LOCK) > 0) {
            return;
        }
        OsalMDelay(1);
    }
    HDF_LOGD("host%d: DRV DLL not locked.", id);
}

static void SdhciEnableSamplDll(uint32_t id)
{
    uint32_t val;
    uint32_t offset[] = { PERI_CRG125, PERI_CRG139 };

    if (id >= (sizeof(offset) / sizeof(offset[0]))) {
        return;
    }

    val = OSAL_READL(offset[id]);
    val |= SDHCI_SAMPL_DLL_SLAVE_EN;
    OSAL_WRITEL(val, offset[id]);
}

static void SdhciPreTune(struct SdhciHost *host)
{
    uint32_t val;

    val = SdhciReadl(host, NORMAL_INT_STAT_EN_R);
    val |= SDHCI_INTERRUPT_DATA_AVAIL;
    SdhciWritel(host, val, NORMAL_INT_STAT_EN_R);

    val = SdhciReadl(host, NORMAL_INT_SIGNAL_EN_R);
    val |= SDHCI_INTERRUPT_DATA_AVAIL;
    SdhciWritel(host, val, NORMAL_INT_SIGNAL_EN_R);

    SdhciWaitDrvDllLock(host->hostId);
    SdhciEnableSamplDll(host->hostId);
    SdhciEnableSample(host);
}

static void SdhciEnableEdgeTune(struct SdhciHost *host)
{
    uint32_t val;
    uint32_t offset[] = { PERI_CRG126, PERI_CRG135 };

    if (host->hostId >= (sizeof(offset) / sizeof(offset[0]))) {
        return;
    }
    val = OSAL_READL(offset[host->hostId]);
    val = (val & (~SDHCI_SAMPLB_DLL_CLK_MASK)) | SDHCI_SAMPLB_SEL(SDHCI_SAMPLB_DLL_CLK);
    OSAL_WRITEL(val, offset[host->hostId]);
    val = SdhciReadl(host, MULTI_CYCLE_R);
    val |= SDHCI_EDGE_DETECT_EN | SDHCI_DATA_DLY_EN;
    val &= ~SDHCI_CMD_DLY_EN;
    SdhciWritel(host, val, MULTI_CYCLE_R);
}

void SdhciWaitSamplDllReady(uint32_t id)
{
    uint32_t offset[] = { PERI_SD_SAMPL_DLL_STATUS, PERI_SDIO_SAMPL_DLL_STATUS };
    uint32_t i, val;

    if (id >= (sizeof(offset) / sizeof(offset[0]))) {
        return;
    }

    for (i = 0; i < SDHCI_CLK_CTRL_RETRY_TIMES; i++) {
        val = OSAL_READL(offset[id]);
        if ((val & SDHCI_SAMPL_DLL_SLAVE_READY) > 0) {
            return;
        }
        OsalMDelay(1);
    }
    HDF_LOGD("host%d: SAMPL DLL not ready.", id);
}

static void SdhciCardClk(struct SdhciHost *host, bool action)
{
    uint32_t value;

    value = SdhciReadl(host, CLK_CTRL_R);
    if (action == false) {
        /* close the clock gate */
        value &= ~SDHCI_CLK_CTRL_CLK_EN;
    } else {
        /* open the clk of interface */
        value |= SDHCI_CLK_CTRL_CLK_EN;
    }
    SdhciWritel(host, value, CLK_CTRL_R);
}

static void SdhciSelectSamplPhase(struct SdhciHost *host, uint32_t phase)
{
    SdhciCardClk(host, false);
    SdhciSetSampPhase(host, phase);
    SdhciWaitSamplDllReady(host->hostId);
    SdhciCardClk(host, true);
    OsalUDelay(1);
}

static void SdhciDisEdgeTune(struct SdhciHost *host)
{
    uint32_t val;

    val = SdhciReadl(host, MULTI_CYCLE_R);
    val &= ~SDHCI_EDGE_DETECT_EN;
    SdhciWritel(host, val, MULTI_CYCLE_R);
}

static void SdhciPostTune(struct SdhciHost *host)
{
    uint32_t val;
    uint16_t ctrl;

    ctrl = SdhciReadw(host, HOST_CTRL2_R);
    ctrl |= SDHCI_TUNED_CLK;
    SdhciWritew(host, ctrl, HOST_CTRL2_R);

    val = SdhciReadl(host, NORMAL_INT_STAT_EN_R);
    val &= ~SDHCI_INTERRUPT_DATA_AVAIL;
    SdhciWritel(host, val, NORMAL_INT_STAT_EN_R);
    val = SdhciReadl(host, NORMAL_INT_SIGNAL_EN_R);
    val &= ~SDHCI_INTERRUPT_DATA_AVAIL;
    SdhciWritel(host, val, NORMAL_INT_SIGNAL_EN_R);
}

static void SdhciDoTune(struct SdhciHost *host, uint32_t opcode, uint32_t start, uint32_t end)
{
    int32_t err;
    int32_t prevError = 0;
    uint32_t phase, fall, rise, fallUpdateFlag, index;

    fall = start;
    rise = end;
    fallUpdateFlag = 0;
    for (index = start; index <= end; index++) {
        SdhciSelectSamplPhase(host, index % SDHCI_PHASE_SCALE);
        err = MmcSendTuning(host->mmc, opcode, true);
        if (err != HDF_SUCCESS) {
            HDF_LOGD("send tuning CMD%u fail! phase:%d err:%d.", opcode, index, err);
        }
        if (err && index == start) {
            if (!fallUpdateFlag) {
                fallUpdateFlag = 1;
                fall = start;
            }
        } else {
            if (!prevError && err && !fallUpdateFlag) {
                fallUpdateFlag = 1;
                fall = index;
            }
        }

        if (prevError && !err) {
            rise = index;
        }

        if (err && index == end) {
            rise = end;
        }
        prevError = err;
    }

    phase = ((fall + rise) / SDHCI_DIV_MIDDLE + SDHCI_PHASE_SCALE_GAP) % SDHCI_PHASE_SCALE;
    host->tuningPhase = phase;
    SdhciSelectSamplPhase(host, phase);
    SdhciPostTune(host);
}

static int32_t SdhciTune(struct MmcCntlr *cntlr, uint32_t cmdCode)
{
    struct SdhciHost *host = (struct SdhciHost *)cntlr->priv;
    uint32_t index, val;
    bool found = false;
    bool prevFound = false;
    uint32_t edgeP2F = 0;
    uint32_t start = 0;
    uint32_t edgeF2P = SDHCI_PHASE_SCALE / SDHCI_PHASE_SCALE_TIMES;
    uint32_t end = SDHCI_PHASE_SCALE / SDHCI_PHASE_SCALE_TIMES;
    int32_t err;

    SdhciPreTune(host);
    SdhciEnableEdgeTune(host);
    for (index = 0; index <= end; index++) {
        SdhciSelectSamplPhase(host, index * SDHCI_PHASE_SCALE_TIMES);
        err = MmcSendTuning(cntlr, cmdCode, true);
        if (err == HDF_SUCCESS) {
            val = SdhciReadl(host, MULTI_CYCLE_R);
            found = ((val & SDHCI_FOUND_EDGE) > 0 ? true : false);
        } else {
            found = true;
        }

        if (prevFound == true && found == false) {
            edgeF2P = index;
        } else if (prevFound == false && found == true) {
            edgeP2F = index;
        }
        if ((edgeP2F != start) && (edgeF2P != end)) {
            break;
        }
        prevFound = found;
        found = false;
    }

    if ((edgeP2F == start) && (edgeF2P == end)) {
        HDF_LOGE("host%d: tuning failed! can not found edge!", host->hostId);
        return HDF_FAILURE;
    }
    SdhciDisEdgeTune(host);

    start = edgeP2F * SDHCI_PHASE_SCALE_TIMES;
    end = edgeF2P * SDHCI_PHASE_SCALE_TIMES;
    if (end <= start) {
        end += SDHCI_PHASE_SCALE;
    }

    SdhciDoTune(host, cmdCode, start, end);
    return HDF_SUCCESS;
}

static int32_t SdhciRescanSdioDev(struct MmcCntlr *cntlr)
{
    struct SdhciHost *host = NULL;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct SdhciHost *)cntlr->priv;
    if (host->waitForEvent == true) {
        (void)SDHCI_EVENT_SIGNAL(&host->sdhciEvent, SDHCI_PEND_ACCIDENT);
    }

    return MmcCntlrAddSdioRescanMsgToQueue(cntlr);
}

static struct MmcCntlrOps g_sdhciHostOps = {
    .request = SdhciDoRequest,
    .setClock = SdhciSetClock,
    .setPowerMode = SdhciSetPowerMode,
    .setBusWidth = SdhciSetBusWidth,
    .setBusTiming = SdhciSetBusTiming,
    .setSdioIrq = SdhciSetSdioIrq,
    .hardwareReset = SdhciHardwareReset,
    .systemInit = SdhciSystemInit,
    .setEnhanceSrobe = SdhciSetEnhanceSrobe,
    .switchVoltage = SdhciSwitchVoltage,
    .devReadOnly = SdhciDevReadOnly,
    .devPluged = SdhciCardPluged,
    .devBusy = SdhciDevBusy,
    .tune = SdhciTune,
    .rescanSdioDev = SdhciRescanSdioDev,
};

static void SdhciDeleteHost(struct SdhciHost *host)
{
    struct MmcCntlr *cntlr = NULL;

    if (host == NULL) {
        return;
    }

    cntlr = host->mmc;
    if (cntlr != NULL) {
        if (cntlr->curDev != NULL) {
            MmcDeviceRemove(cntlr->curDev);
            OsalMemFree(cntlr->curDev);
            cntlr->curDev = NULL;
        }
        MmcCntlrRemove(cntlr);
        cntlr->hdfDevObj = NULL;
        cntlr->priv = NULL;
        cntlr->ops = NULL;
        OsalMemFree(cntlr);
        host->mmc = NULL;
    }

    OsalUnregisterIrq(host->irqNum, host);
    if (host->admaDesc != NULL) {
        OsalMemFree(host->admaDesc);
        host->admaDesc = NULL;
    }
    if (host->base != NULL) {
        OsalIoUnmap(host->base);
    }

    (void)SDHCI_EVENT_DELETE(&host->sdhciEvent);
    (void)OsalMutexDestroy(&host->mutex);
    OsalMemFree(host);
}

static int32_t SdhciHostParse(struct SdhciHost *host, struct HdfDeviceObject *obj)
{
    const struct DeviceResourceNode *node = NULL;
    struct DeviceResourceIface *drsOps = NULL;
    int32_t ret;
    uint32_t regBase, regSize;

    if (obj == NULL || host == NULL) {
        HDF_LOGE("%s: input param is NULL.", __func__);
        return HDF_FAILURE;
    }

    node = obj->property;
    if (node == NULL) {
        HDF_LOGE("%s: drs node is NULL.", __func__);
        return HDF_FAILURE;
    }
    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL) {
        HDF_LOGE("%s: invalid drs ops fail!", __func__);
        return HDF_FAILURE;
    }

    ret = drsOps->GetUint32(node, "regBasePhy", &regBase, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regBasePhy fail!", __func__);
        return ret;
    }
    ret = drsOps->GetUint32(node, "regSize", &regSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regSize fail!", __func__);
        return ret;
    }
    host->base = OsalIoRemap(regBase, regSize);
    if (host->base == NULL) {
        HDF_LOGE("%s: ioremap regBase fail!", __func__);
        return HDF_ERR_IO;
    }

    ret = drsOps->GetUint32(node, "irqNum", &(host->irqNum), 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read irqNum fail!", __func__);
    }

    ret = drsOps->GetUint32(node, "quirks", &(host->quirks.quirksData), 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGD("%s: read quirks fail!", __func__);
        host->quirks.quirksData = 0;
    }
    return ret;
}

static void SdhciCrgInit(struct SdhciHost *host)
{
    uint32_t value, reg;
    uint32_t cfgArray[] = {PERI_CRG125, PERI_CRG139};
    if (host->hostId >= SDHCI_MAX_HOST_NUM) {
        return;
    }

    /* open the clock gate */
    reg = cfgArray[host->hostId];
    value = OSAL_READL(reg);
    value |= SDHCI_EMMC_CKEN;
    OSAL_WRITEL(value, reg);

    /* crg_reset, dll_reset, sampl_reset */
    reg = cfgArray[host->hostId];
    value = OSAL_READL(reg);
    value |= SDHCI_EMMC_CRG_REQ;
    value |= SDHCI_EMMC_DLL_RST;
    OSAL_WRITEL(value, reg);

    /* wait the card clk close for 25us */
    HalDelayUs(25);

    /* reset of host contorl is done */
    value = OSAL_READL(reg);
    value &= ~SDHCI_EMMC_CRG_REQ;
    OSAL_WRITEL(value, reg);
    HalDelayUs(25);

    /* close the clock gate */
    SdhciCardClk(host, false);

    if (host->mmc->devType == MMC_DEV_EMMC) {
        (void)SdhciSelectClock(host, SDHCI_MMC_FREQ_150M);
    } else {
        (void)SdhciSelectClock(host, SDHCI_MMC_FREQ_50M);
    }

    /* SAM/DRV */
    SdhciSetDrvPhase(host->hostId, 0x10);
    /* wait the clock switch over for 25us */
    OsalUDelay(25);

    /* open the clk of interface */
    SdhciCardClk(host, true);

    /* open the clk of card */
    value = SdhciReadl(host, CLK_CTRL_R);
    value |= (SDHCI_CLK_CTRL_INT_CLK_EN | SDHCI_CLK_CTRL_CLK_EN | SDHCI_CLK_CTRL_PLL_EN);
    SdhciWritel(host, value, CLK_CTRL_R);
    /* wait the phase switch over, 75us */
    OsalUDelay(75);
}

static void SdhciUpdateCapFlag(struct SdhciHost *host, uint32_t cap)
{
    struct MmcCntlr *mmc = host->mmc;

    if (cap & SDHCI_SUPPORT_SDMA) {
        host->flags |= SDHCI_USE_SDMA;
    }

    if ((host->version >= SDHCI_HOST_SPEC_200) && (cap & SDHCI_SUPPORT_ADMA2)) {
        host->flags |= SDHCI_USE_ADMA;
        if (cap & SDHCI_SUPPORT_64BIT) {
            host->flags |= SDHCI_USE_64BIT_ADMA;
        }
    }

    if ((host->version >= SDHCI_HOST_SPEC_300 && (host->flags & SDHCI_USE_ADMA) > 0) ||
        (host->flags & SDHCI_USE_SDMA) == 0) {
        host->flags |= SDHCI_AUTO_CMD23;
        HDF_LOGD("Auto-CMD23 available!");
    }

    host->flags |= SDHCI_HOST_IRQ_STATUS;
    if (mmc->devType == MMC_DEV_SDIO) {
        host->flags &= ~(SDHCI_SUPPORT_SDMA | SDHCI_AUTO_CMD23 | SDHCI_AUTO_CMD12);
    } else {
        host->flags &= ~(SDHCI_SUPPORT_SDMA);
        host->flags |= SDHCI_AUTO_CMD23;
    }
}

static int32_t SdhciFillAdmaInfo(struct SdhciHost *host)
{
    if (host->flags & SDHCI_USE_ADMA) {
        host->admaMaxDesc = SDHCI_ADMA_MAX_DESC;
        host->admaDescSize = SDHCI_ADMA_DEF_SIZE;

        if (host->flags & SDHCI_USE_64BIT_ADMA) {
            host->admaDescLineSize = SDHCI_ADMA_64BIT_LINE_SIZE;
        } else {
            host->admaDescLineSize = SDHCI_ADMA_LINE_SIZE;
        }
        host->admaDescSize = (host->admaMaxDesc * 2 + 1) * host->admaDescLineSize;
        host->admaDesc = (void *)OsalMemAllocAlign(CACHE_ALIGNED_SIZE, ALIGN(host->admaDescSize, CACHE_ALIGNED_SIZE));
        if (host->admaDesc == NULL) {
            HDF_LOGE("SdhciFillAdmaInfo: allocate ADMA buffer fail!");
            return HDF_ERR_MALLOC_FAIL;
        }
    } else {
        HDF_LOGE("SdhciFillAdmaInfo: Warning! ADMA not support!");
        return HDF_ERR_NOT_SUPPORT;
    }
    return HDF_SUCCESS;
}

static void SdhciFillClkInfo(struct SdhciHost *host, uint32_t cap1, uint32_t cap2)
{
    struct MmcCntlr *mmc = host->mmc;

    if (host->version >= SDHCI_HOST_SPEC_300) {
        host->maxClk = (cap1 & SDHCI_BASIC_FREQ_OF_CLK_MASK) >> SDHCI_BASIC_FREQ_OF_CLK_SHIFT;
    } else {
        host->maxClk = (cap1 & SDHCI_CLK_BASE_MASK) >> SDHCI_BASIC_FREQ_OF_CLK_SHIFT;
    }
    /* unit: MHz */
    host->maxClk *= 1000000;
    host->clkMul = (cap2 & SDHCI_CLK_MUL_MASK) >> SDHCI_CLK_MUL_SHIFT;

    if (host->clkMul) {
        host->clkMul += 1;
    }
    if (mmc->freqMax == 0) {
        mmc->freqMax = host->maxClk;
    } else {
        host->maxClk = mmc->freqMax;
    }

    if (mmc->freqMin == 0) {
        if (host->version >= SDHCI_HOST_SPEC_300) {
            if (host->clkMul) {
                mmc->freqMin = (host->maxClk * host->clkMul) / 1024;
                mmc->freqMax = host->maxClk * host->clkMul;
            } else {
                mmc->freqMin = host->maxClk / SDHCI_MAX_DIV_SPEC_300;
            }
        } else {
            mmc->freqMin = host->maxClk / SDHCI_MAX_DIV_SPEC_200;
        }
    }
}

static void SdhciUpdateDrvCap(struct SdhciHost *host, uint32_t cap1, uint32_t cap2)
{
    struct MmcCntlr *mmc = host->mmc;

    if (cap1 & SDHCI_SUPPORT_HISPD) {
        mmc->caps.bits.highSpeed = 1;
    }

    if (cap2 & SDHCI_SUPPORT_DRIVER_TYPE_A) {
        mmc->caps.bits.driverTypeA = 1;
    }
    if (cap2 & SDHCI_SUPPORT_DRIVER_TYPE_C) {
        mmc->caps.bits.driverTypeC = 1;
    }
    if (cap2 & SDHCI_SUPPORT_DRIVER_TYPE_D) {
        mmc->caps.bits.driverTypeD = 1;
    }

    if (mmc->devType == MMC_DEV_EMMC) {
        if (cap1 & SDHCI_SUPPORT_VDD_180) {
            mmc->ocrDef.bits.vdd1v65To1v95 = 1;
            HDF_LOGD("VDD1.8 support.");
        }
    }

    if (host->flags & SDHCI_USE_ADMA) {
        mmc->maxReqSize = host->admaMaxDesc * 65536;
    }

    mmc->maxBlkSize = (cap1 & SDHCI_MAX_BLOCK_SIZE_MASK) >> SDHCI_MAX_BLOCK_SIZE_SHIFT;
    mmc->maxBlkSize = MMC_SEC_SIZE << mmc->maxBlkSize;
    mmc->maxBlkNum = (mmc->maxReqSize / mmc->maxBlkSize);
}

static void SdhciEnableSdioIrqNoLock(struct SdhciHost *host, bool enable)
{
    if (!(host->flags & SDHCI_DEVICE_DEAD)) {
        if (enable == true) {
            host->irqEnable |= SDHCI_INTERRUPT_CARD_INT;
        } else {
            host->irqEnable &= ~SDHCI_INTERRUPT_CARD_INT;
        }
        SdhciEnablePlugIrq(host, host->irqEnable);
    }
}

static void SdhciSaveCommandResp(struct SdhciHost *host, struct MmcCmd *cmd)
{
    uint32_t i;

    for (i = 0; i < MMC_CMD_RESP_SIZE; i++) {
        cmd->resp[i] = SdhciReadl(host, RESP01_R + (3 - i) * 4) << 8;
        if (i != 3) {
            cmd->resp[i] |= SdhciReadb(host, RESP01_R + (3 - i) * 4 - 1);
        }
    }
}

static void SdhciFinishCommand(struct SdhciHost *host)
{
    struct MmcCmd *cmd = host->cmd;

    if (cmd == NULL) {
        return;
    }

    if (cmd->respType & RESP_PRESENT) {
        if (cmd->respType & RESP_136) {
            SdhciSaveCommandResp(host, cmd);
        } else {
            cmd->resp[0] = SdhciReadl(host, RESP01_R);
        }
    }

    if (cmd->data == NULL || cmd->cmdCode == STOP_TRANSMISSION) {
        SdhciTaskletFinish(host);
    }
}

static void SdhciCmdIrq(struct SdhciHost *host, uint32_t intMask)
{
    if (host->cmd == NULL) {
        return;
    }

    if (intMask & SDHCI_INTERRUPT_TIMEOUT) {
        host->cmd->returnError = HDF_ERR_TIMEOUT;
    } else if (intMask & (SDHCI_INTERRUPT_CRC | SDHCI_INTERRUPT_END_BIT | SDHCI_INTERRUPT_INDEX)) {
        host->cmd->returnError = HDF_MMC_ERR_ILLEGAL_SEQ;
    }

    if (host->cmd->data == NULL && host->cmd->returnError != HDF_SUCCESS) {
        SdhciTaskletFinish(host);
        return;
    }

    if (host->cmd->respType & RESP_BUSY) {
        return;
    }

    if (intMask & SDHCI_INTERRUPT_RESPONSE) {
        SdhciFinishCommand(host);
    }
}

static void SdhciFinishData(struct SdhciHost *host)
{
    struct MmcData *data = host->cmd->data;

    if (data->sendStopCmd == true) {
        if (data->returnError != HDF_SUCCESS) {
            SdhciDoReset(host, SDHCI_RESET_CMD);
            SdhciDoReset(host, SDHCI_RESET_DATA);
        }
        SdhciExecCmd(host, &(data->stopCmd));
    } else {
        SdhciTaskletFinish(host);
    }
}

static void SdhciDataIrq(struct SdhciHost *host, uint32_t intMask)
{
    uint32_t command;
    struct MmcCmd *cmd = host->cmd;

    if (cmd->data == NULL || (cmd->cmdCode == STOP_TRANSMISSION)) {
        if ((cmd->respType & RESP_BUSY)) {
            if (intMask & SDHCI_INTERRUPT_DATA_TIMEOUT) {
                cmd->returnError = HDF_ERR_TIMEOUT;
                SdhciTaskletFinish(host);
                return;
            }
            if (intMask & SDHCI_INTERRUPT_DATA_END) {
                SdhciFinishCommand(host);
                return;
            }
        }
    }
    if (cmd->data == NULL) {
        return;
    }

    if (intMask & SDHCI_INTERRUPT_DATA_TIMEOUT) {
        cmd->data->returnError = HDF_ERR_TIMEOUT;
    } else if (intMask & SDHCI_INTERRUPT_END_BIT) {
        cmd->data->returnError = HDF_MMC_ERR_ILLEGAL_SEQ;
    } else if ((intMask & SDHCI_INTERRUPT_DATA_CRC)) {
        cmd->data->returnError = HDF_MMC_ERR_ILLEGAL_SEQ;
    } else if (intMask & SDHCI_INTERRUPT_ADMA_ERROR) {
        cmd->data->returnError = HDF_ERR_IO;
    }

    if (cmd->data->returnError != HDF_SUCCESS) {
        command = SDHCI_PARSE_CMD(SdhciReadw(host, CMD_R));
        if (command != SD_CMD_SEND_TUNING_BLOCK && command != SEND_TUNING_BLOCK_HS200) {
            HDF_LOGE("err = 0x%x, cmd = %d, interrupt = 0x%x.", cmd->data->returnError, command, intMask);
            SdhciDumpregs(host);
        }
        SdhciFinishData(host);
    } else {
        if (intMask & SDHCI_INTERRUPT_DATA_END) {
            SdhciFinishData(host);
        } else {
            HDF_LOGE("do check here! intmask = 0x%x.", intMask);
        }
    }
}

static uint32_t SdhciIrqHandler(uint32_t irq, void *data)
{
    struct SdhciHost *host = (struct SdhciHost *)data;
    uint32_t intMask;
    (void)irq;

    if (host == NULL || host->mmc == NULL) {
        HDF_LOGE("SdhciIrqHandler: data is null!");
        return HDF_SUCCESS;
    }

    while ((intMask = SdhciReadl(host, NORMAL_INT_STAT_R)) != 0) {
        SdhciWritel(host, intMask, NORMAL_INT_STAT_R);
        if (intMask & SDHCI_CART_PLUG_STATE) {
            host->irqEnable &= ~SDHCI_CART_PLUG_STATE;
            host->irqEnable |= SDHCI_PLUG_STATE(host) ? SDHCI_INTERRUPT_CARD_REMOVE : SDHCI_INTERRUPT_CARD_INSERT;
            SdhciEnablePlugIrq(host, host->irqEnable);
            SdhciWritel(host, intMask & SDHCI_CART_PLUG_STATE, NORMAL_INT_STAT_R);
            MmcCntlrAddPlugMsgToQueue(host->mmc);
            if (host->waitForEvent) {
                (void)SDHCI_EVENT_SIGNAL(&host->sdhciEvent, SDHCI_PEND_ACCIDENT);
            }
        }

        if (intMask & SDHCI_INT_CMD_MASK) {
            SdhciCmdIrq(host, (intMask & SDHCI_INT_CMD_MASK));
        }
        if (intMask & SDHCI_INT_DATA_MASK) {
            SdhciDataIrq(host, (intMask & SDHCI_INT_DATA_MASK));
        }
        if (intMask & SDHCI_INTERRUPT_BUS_POWER) {
            HDF_LOGD("host%d: card is consuming too much power!", host->hostId);
        }
        if (intMask & SDHCI_INTERRUPT_CARD_INT) {
            SdhciEnableSdioIrqNoLock(host, false);
            MmcCntlrNotifySdioIrqThread(host->mmc);
        }
    }
    return HDF_SUCCESS;
}

static int32_t SdhciHostInit(struct SdhciHost *host, struct MmcCntlr *cntlr)
{
    int32_t ret;
    uint32_t Capability1;
    uint32_t Capability2 = 0;

    host->hostId = (uint32_t)cntlr->index;
    if (SDHCI_EVENT_INIT(&host->sdhciEvent) != HDF_SUCCESS) {
        HDF_LOGE("SdhciHostInit: sdhciEvent init fail!\n");
        return HDF_FAILURE;
    }
    if (OsalMutexInit(&host->mutex) != HDF_SUCCESS) {
        HDF_LOGE("SdhciHostInit: init mutex lock fail!");
        return HDF_FAILURE;
    }

    SdhciCrgInit(host);
    host->version = SdhciReadw(host, HOST_VERSION_R);
    host->version = (host->version & SDHCI_HOST_SPEC_VER_MASK);
    Capability1 = SdhciReadl(host, CAPABILITIES1_R);
    if (host->version >= SDHCI_HOST_SPEC_300) {
        Capability2 = SdhciReadl(host, CAPABILITIES2_R);
    }

    SdhciUpdateCapFlag(host, Capability1);
    ret = SdhciFillAdmaInfo(host);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    Capability2 &= ~(SDHCI_SUPPORT_SDR50 | SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_DDR50 | SDHCI_SUPPORT_ADMA3);
    Capability2 |= SDHCI_USE_SDR50_TUNING;
    SdhciFillClkInfo(host, Capability1, Capability2);
    SdhciUpdateDrvCap(host, Capability1, Capability2);
    ret = SdhciSystemInit(host->mmc);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = OsalRegisterIrq(host->irqNum, 0, (OsalIRQHandle)SdhciIrqHandler, "MMC_IRQ", host);
    if (ret) {
        HDF_LOGE("SdhciHostInit: request irq for sdhci is err.");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t SdhciMmcBind(struct HdfDeviceObject *obj)
{
    struct MmcCntlr *cntlr = NULL;
    struct SdhciHost *host = NULL;
    int32_t ret;

    if (obj == NULL) {
        HDF_LOGE("SdhciMmcBind: Fail, device is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = (struct MmcCntlr *)OsalMemCalloc(sizeof(struct MmcCntlr));
    if (cntlr == NULL) {
        HDF_LOGE("SdhciMmcBind: no mem for MmcCntlr.");
        return HDF_ERR_MALLOC_FAIL;
    }
    host = (struct SdhciHost *)OsalMemCalloc(sizeof(struct SdhciHost));
    if (host == NULL) {
        HDF_LOGE("SdhciMmcBind: no mem for SdhciHost.");
        OsalMemFree(cntlr);
        return HDF_ERR_MALLOC_FAIL;
    }

    host->mmc = cntlr;
    cntlr->priv = (void *)host;
    cntlr->ops = &g_sdhciHostOps;
    cntlr->hdfDevObj = obj;
    obj->service = &cntlr->service;
    /* init cntlr. */
    ret = MmcCntlrParse(cntlr, obj);
    if (ret != HDF_SUCCESS) {
        goto _ERR;
    }
    ret = SdhciHostParse(host, obj);
    if (ret != HDF_SUCCESS) {
        goto _ERR;
    }
    ret = SdhciHostInit(host, cntlr);
    if (ret != HDF_SUCCESS) {
        goto _ERR;
    }
    ret = MmcCntlrAdd(cntlr);
    if (ret != HDF_SUCCESS) {
        goto _ERR;
    }

    (void)MmcCntlrAddDetectMsgToQueue(cntlr);
    HDF_LOGD("SdhciMmcBind: success.");
    return HDF_SUCCESS;
_ERR:
    SdhciDeleteHost(host);
    HDF_LOGE("SdhciMmcBind: fail, err = %d.", ret);
    return ret;
}

static int32_t SdhciMmcInit(struct HdfDeviceObject *obj)
{
    static bool procInit = false;
    (void)obj;

    if (procInit == false) {
        if (ProcMciInit() == HDF_SUCCESS) {
            procInit = true;
            HDF_LOGD("SdhciMmcInit: proc init success.");
        }
    }

    HDF_LOGD("SdhciMmcInit: success.");
    return HDF_SUCCESS;
}

static void SdhciMmcRelease(struct HdfDeviceObject *obj)
{
    struct MmcCntlr *cntlr = NULL;

    if (obj == NULL) {
        return;
    }

    cntlr = (struct MmcCntlr *)obj->service;
    if (cntlr == NULL) {
        return;
    }
    SdhciDeleteHost((struct SdhciHost *)cntlr->priv);
}

struct HdfDriverEntry g_mmcDriverEntry = {
    .moduleVersion = 1,
    .Bind = SdhciMmcBind,
    .Init = SdhciMmcInit,
    .Release = SdhciMmcRelease,
    .moduleName = "hi3518_mmc_driver",
};
HDF_INIT(g_mmcDriverEntry);
