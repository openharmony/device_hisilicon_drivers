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

#include "himci.h"
#include "himci_proc.h"
#include "securec.h"

#define HDF_LOG_TAG himci_adapter

#define HIMCI_PIN_NUM 6
#define HIMCI_VOLT_SWITCH_TIMEOUT 10
#define HIMCI_PHASE_DLL_START_ELEMENT 2

static void HimciDumpRegs(struct HimciHost *host)
{
    HDF_LOGE(": =========== DUMP (host%d) REGISTER===========", host->id);
    HDF_LOGE(": CTRL : 0x%08x | PWREN:  0x%04x",
        HIMCI_READL((uintptr_t)host->base + MMC_CTRL), HIMCI_READL((uintptr_t)host->base + MMC_PWREN));
    HDF_LOGE(": CLKDIV : 0x%08x | CLKENA:  0x%04x",
        HIMCI_READL((uintptr_t)host->base + MMC_CLKDIV), HIMCI_READL((uintptr_t)host->base + MMC_CLKENA));
    HDF_LOGE(": TMOUT : 0x%08x | CTYPE:  0x%04x",
        HIMCI_READL((uintptr_t)host->base + MMC_TMOUT), HIMCI_READL((uintptr_t)host->base + MMC_CTYPE));
    HDF_LOGE(": BLKSIZ : 0x%08x | BYTCNT:  0x%04x",
        HIMCI_READL((uintptr_t)host->base + MMC_BLKSIZ), HIMCI_READL((uintptr_t)host->base + MMC_BYTCNT));
    HDF_LOGE(": CMD : 0x%08x | CMDARG:  0x%04x",
        HIMCI_READL((uintptr_t)host->base + MMC_CMD), HIMCI_READL((uintptr_t)host->base + MMC_CMDARG));
    HDF_LOGE(": RESP0 : 0x%08x | RESP1:  0x%04x",
        HIMCI_READL((uintptr_t)host->base + MMC_RESP0), HIMCI_READL((uintptr_t)host->base + MMC_RESP1));
    HDF_LOGE(": RESP2 : 0x%08x | RESP3:  0x%04x",
        HIMCI_READL((uintptr_t)host->base + MMC_RESP2), HIMCI_READL((uintptr_t)host->base + MMC_RESP3));
    HDF_LOGE(": RINTSTS : 0x%08x | STATUS:  0x%04x",
        HIMCI_READL((uintptr_t)host->base + MMC_RINTSTS), HIMCI_READL((uintptr_t)host->base + MMC_STATUS));
    HDF_LOGE(": BMOD : 0x%08x | IDSTS:  0x%04x",
        HIMCI_READL((uintptr_t)host->base + MMC_BMOD), HIMCI_READL((uintptr_t)host->base + MMC_IDSTS));
    HDF_LOGE(": IDINTEN : 0x%08x | CARDTHRCTL : 0x%08x",
        HIMCI_READL((uintptr_t)host->base + MMC_IDINTEN), HIMCI_READL((uintptr_t)host->base + MMC_CARDTHRCTL));
    HDF_LOGE(": DDR_REG:  0x%04x | ENABLE_SHIFT : 0x%08x",
        HIMCI_READL((uintptr_t)host->base + MMC_EMMC_DDR_REG), HIMCI_READL((uintptr_t)host->base + MMC_ENABLE_SHIFT));
    HDF_LOGE(": =============================================");
}

static void HimciSetEmmcDrvCap(struct MmcCntlr *cntlr)
{
    uint32_t i, j, val;
    uint32_t *pinDrvCap = NULL;
    /*  clk   cmd   data0  data1  data2  data3 */
    uint32_t emmcHs200Drv[] = { 0x2b0, 0x1c0, 0x1c0, 0x1c0, 0x1c0, 0x1c0 };
    uint32_t emmcHsDrv[] = { 0x6b0, 0x5e0, 0x5e0, 0x5e0, 0x5e0, 0x5e0 };
    uint32_t emmcDsDrv[] = { 0x6b0, 0x5f0, 0x5f0, 0x5f0, 0x5f0, 0x5f0 };
    uint32_t emmcDs400kDrv[] = { 0x6c0, 0x5f0, 0x5f0, 0x5f0, 0x5f0, 0x5f0 };

    if (cntlr->curDev->workPara.timing == BUS_TIMING_MMC_HS200) {
        pinDrvCap = emmcHs200Drv;
    } else if (cntlr->curDev->workPara.timing == BUS_TIMING_MMC_HS) {
        pinDrvCap = emmcHsDrv;
    } else {
        if (cntlr->curDev->workPara.clock == 400000) {
            pinDrvCap = emmcDs400kDrv;
        } else {
            pinDrvCap = emmcDsDrv;
        }
    }

    for (i = REG_CTRL_EMMC_START, j = 0; j < HIMCI_PIN_NUM; i = i + REG_CTRL_NUM, j++) {
        val = HIMCI_READL(i);
        /*
         * [10]:SR
         * [9]:internel pull down
         * [8]:internel pull up
         */
        val = val & (~(0x7f0));
        val |= pinDrvCap[j];
        HIMCI_WRITEL(val, i);
    }
}

static void HimciSetSdDrvCap(struct MmcCntlr *cntlr)
{
    uint32_t i, j, val;
    uint32_t *pinDrvCap = NULL;
    /*  clk   cmd   data0  data1  data2  data3 */
    uint32_t sdSdr104Drv[] = { 0x290, 0x1c0, 0x1c0, 0x1c0, 0x1c0, 0x1c0 };
    uint32_t sdSdr50Drv[] = { 0x290, 0x1c0, 0x1c0, 0x1c0, 0x1c0, 0x1c0 };
    uint32_t sdSdr25Drv[] = { 0x6b0, 0x5d0, 0x5d0, 0x5d0, 0x5d0, 0x5d0 };
    uint32_t sdSdr12Drv[] = { 0x6b0, 0x5e0, 0x5e0, 0x5e0, 0x5e0, 0x5e0 };
    uint32_t sdHsDrv[] = { 0x6d0, 0x5f0, 0x5f0, 0x5f0, 0x5f0, 0x5f0 };
    uint32_t sdDsDrv[] = { 0x6b0, 0x5e0, 0x5e0, 0x5e0, 0x5e0, 0x5e0 };

    if (cntlr->curDev->workPara.timing == BUS_TIMING_UHS_SDR104) {
        pinDrvCap = sdSdr104Drv;
    } else if (cntlr->curDev->workPara.timing == BUS_TIMING_UHS_SDR50) {
        pinDrvCap = sdSdr50Drv;
    } else if (cntlr->curDev->workPara.timing == BUS_TIMING_UHS_SDR25) {
        pinDrvCap = sdSdr25Drv;
    } else if (cntlr->curDev->workPara.timing == BUS_TIMING_UHS_SDR12) {
        pinDrvCap = sdSdr12Drv;
    } else if (cntlr->curDev->workPara.timing == BUS_TIMING_SD_HS) {
        pinDrvCap = sdHsDrv;
    } else {
        pinDrvCap = sdDsDrv;
    }

    for (i = REG_CTRL_SD_START, j = 0; j < HIMCI_PIN_NUM; i = i + REG_CTRL_NUM, j++) {
        val = HIMCI_READL(i);
        /*
         * [10]:SR
         * [9]:internel pull down
         * [8]:internel pull up
         */
        val = val & (~(0x7f0));
        val |= pinDrvCap[j];
        HIMCI_WRITEL(val, i);
    }
}

static void HimciSetSdioDrvCap(struct MmcCntlr *cntlr)
{
    uint32_t i, j, val;
    uint32_t *pinDrvCap = NULL;
    /*  clk   cmd   data0  data1  data2  data3 */
    uint32_t sdioSdr104Drv[] = { 0x290, 0x1c0, 0x1c0, 0x1c0, 0x1c0, 0x1c0 };
    uint32_t sdioSdr50Drv[] = { 0x290, 0x1c0, 0x1c0, 0x1c0, 0x1c0, 0x1c0 };
    uint32_t sdioSdr25Drv[] = { 0x6b0, 0x5d0, 0x5d0, 0x5d0, 0x5d0, 0x5d0 };
    uint32_t sdioSdr12Drv[] = { 0x6b0, 0x5e0, 0x5e0, 0x5e0, 0x5e0, 0x5e0 };
    uint32_t sdioHsDrv[] = { 0x6d0, 0x5f0, 0x5f0, 0x5f0, 0x5f0, 0x5f0 };
    uint32_t sdioDsDrv[] = { 0x6b0, 0x5e0, 0x5e0, 0x5e0, 0x5e0, 0x5e0 };

    if (cntlr->curDev->workPara.timing == BUS_TIMING_UHS_SDR104) {
        pinDrvCap = sdioSdr104Drv;
    } else if (cntlr->curDev->workPara.timing == BUS_TIMING_UHS_SDR50) {
        pinDrvCap = sdioSdr50Drv;
    } else if (cntlr->curDev->workPara.timing == BUS_TIMING_UHS_SDR25) {
        pinDrvCap = sdioSdr25Drv;
    } else if (cntlr->curDev->workPara.timing == BUS_TIMING_UHS_SDR12) {
        pinDrvCap = sdioSdr12Drv;
    } else if (cntlr->curDev->workPara.timing == BUS_TIMING_SD_HS) {
        pinDrvCap = sdioHsDrv;
    } else {
        pinDrvCap = sdioDsDrv;
    }

    for (i = REG_CTRL_SDIO_START, j = 0; j < HIMCI_PIN_NUM; i = i + REG_CTRL_NUM, j++) {
        val = HIMCI_READL(i);
        /*
         * [10]:SR
         * [9]:internel pull down
         * [8]:internel pull up
         */
        val = val & (~(0x7f0));
        val |= pinDrvCap[j];
        HIMCI_WRITEL(val, i);
    }
}

static void HimciSetDrvCap(struct MmcCntlr *cntlr)
{
    if (cntlr == NULL) {
        return;
    }

    if (cntlr->devType == MMC_DEV_EMMC) {
        HimciSetEmmcDrvCap(cntlr);
    } else if (cntlr->devType == MMC_DEV_SD) {
        HimciSetSdDrvCap(cntlr);
    } else {
        HimciSetSdioDrvCap(cntlr);
    }
}

static uint32_t HimciClkDiv(struct HimciHost *host, uint32_t clock)
{
    uint32_t clkDiv = 0;
    uint32_t val, hostClk, debounce;
    uint32_t regs[] = { PERI_CRG82, PERI_CRG88, PERI_CRG85 };

    val = HIMCI_READL(regs[host->id]);
    val &= ~(HIMCI_CLK_SEL_MASK);
    if (clock >= HIMCI_MMC_FREQ_150M) {
        hostClk = HIMCI_MMC_FREQ_150M;
        debounce = DEBOUNCE_E;
    } else if (clock >= HIMCI_MMC_FREQ_100M) {
        hostClk = HIMCI_MMC_FREQ_100M;
        val |= HIMCI_CLK_SEL_100M;
        debounce = DEBOUNCE_H;
    } else if (clock >= HIMCI_MMC_FREQ_50M) {
        hostClk = HIMCI_MMC_FREQ_50M;
        val |= HIMCI_CLK_SEL_50M;
        debounce = DEBOUNCE_M;
    } else if (clock >= HIMCI_MMC_FREQ_25M) {
        hostClk = HIMCI_MMC_FREQ_25M;
        val |= HIMCI_CLK_SEL_25M;
        debounce = DEBOUNCE_L;
    } else {
        if (clock > (HIMCI_MMC_FREQ_150M / CLK_DIVIDER)) {
            hostClk = HIMCI_MMC_FREQ_150M;
            debounce = DEBOUNCE_E;
        } else if (clock > (HIMCI_MMC_FREQ_100M / CLK_DIVIDER)) {
            val |= HIMCI_CLK_SEL_100M;
            hostClk = HIMCI_MMC_FREQ_100M;
            debounce = DEBOUNCE_H;
        } else if (clock > (HIMCI_MMC_FREQ_50M / CLK_DIVIDER)) {
            val |= HIMCI_CLK_SEL_50M;
            hostClk = HIMCI_MMC_FREQ_50M;
            debounce = DEBOUNCE_M;
        } else {
            val |= HIMCI_CLK_SEL_25M;
            hostClk = HIMCI_MMC_FREQ_25M;
            debounce = DEBOUNCE_L;
        }
        clkDiv = hostClk / (clock * 2);
        if (hostClk % (clock * 2) > 0) {
            clkDiv++;
        }
        if (clkDiv > MAX_CLKDIV_VAL) {
            clkDiv = MAX_CLKDIV_VAL;
        }
    }
    HIMCI_WRITEL(debounce, (uintptr_t)host->base + MMC_DEBNCE);
    HIMCI_WRITEL(val, regs[host->id]);
    HIMCI_WRITEL(clkDiv, (uintptr_t)host->base + MMC_CLKDIV);

    return hostClk;
}

static void HimciDmaReset(struct HimciHost *host)
{
    uint32_t val;

    val = HIMCI_READL((uintptr_t)host->base + MMC_BMOD);
    val |= BMOD_SWR;
    HIMCI_WRITEL(val, (uintptr_t)host->base + MMC_BMOD);

    val = HIMCI_READL((uintptr_t)host->base + MMC_CTRL);
    val |= CTRL_RESET | FIFO_RESET | DMA_RESET;
    HIMCI_WRITEL(val, (uintptr_t)host->base + MMC_CTRL);

    OsalUDelay(1);
    HIMCI_WRITEL(ALL_INT_CLR, (uintptr_t)host->base + MMC_RINTSTS);
}

static void HimciDmaStart(struct HimciHost *host)
{
    uint32_t val;

    HIMCI_WRITEL(host->dmaPaddr, (uintptr_t)host->base + MMC_DBADDR);
    val = HIMCI_READL((uintptr_t)host->base + MMC_BMOD);
    val |= BMOD_DMA_EN;
    HIMCI_WRITEL(val, (uintptr_t)host->base + MMC_BMOD);
}

static void HimciDmaStop(struct HimciHost *host)
{
    uint32_t val;

    val = HIMCI_READL((uintptr_t)host->base + MMC_BMOD);
    val &= (~BMOD_DMA_EN);
    HIMCI_WRITEL(val, (uintptr_t)host->base + MMC_BMOD);
}

static void HimciDmaCacheClean(void *addr, uint32_t size)
{
    addr = (void *)(uintptr_t)DMA_TO_VMM_ADDR((paddr_t)(uintptr_t)addr);
    uint32_t start = (uintptr_t)addr & ~(CACHE_ALIGNED_SIZE - 1);
    uint32_t end = (uintptr_t)addr + size;

    end = ALIGN(end, CACHE_ALIGNED_SIZE);
    DCacheFlushRange(start, end);
}

static void HimciDmaCacheInv(void *addr, uint32_t size)
{
    addr = (void *)(uintptr_t)DMA_TO_VMM_ADDR((paddr_t)(uintptr_t)addr);
    uint32_t start = (uintptr_t)addr & ~(CACHE_ALIGNED_SIZE - 1);
    uint32_t end = (uintptr_t)addr + size;

    end = ALIGN(end, CACHE_ALIGNED_SIZE);
    DCacheInvRange(start, end);
}

static bool HimciIsMultiBlock(struct MmcCmd *cmd)
{
    if (cmd->cmdCode == WRITE_MULTIPLE_BLOCK || cmd->cmdCode == READ_MULTIPLE_BLOCK) {
        return true;
    }
    if (cmd->data->blockNum > 1) {
        return true;
    }
    return false;
}

static bool HimciNeedAutoStop(struct MmcCntlr *cntlr)
{
    if (cntlr->curDev->type == MMC_DEV_SDIO) {
        return false;
    }

    if (((cntlr->curDev->type == MMC_DEV_SD || cntlr->curDev->type == MMC_DEV_COMBO) &&
        MmcCntlrSdSupportCmd23(cntlr) == false) ||
        (cntlr->curDev->type == MMC_DEV_EMMC && MmcCntlrEmmcSupportCmd23(cntlr) == false)) {
        return true;
    }
    if (cntlr->caps.bits.cmd23 > 0) {
        /* both host and device support cmd23. */
        return false;
    }

    /* the device support cmd23 but host doesn't support cmd23 */
    return true;
}

static int32_t HimciFillCmdReg(union HimciCmdRegArg *reg, struct MmcCmd *cmd)
{
    if (cmd->cmdCode == STOP_TRANSMISSION) {
        reg->bits.stopAbortCmd = 1;
        reg->bits.waitDataComplete = 0;
    } else {
        reg->bits.stopAbortCmd = 0;
        reg->bits.waitDataComplete = 1;
    }

    switch (MMC_RESP_TYPE(cmd)) {
        case MMC_RESP_NONE:
            reg->bits.rspExpect = 0;
            reg->bits.rspLen = 0;
            reg->bits.checkRspCrc = 0;
            break;
        case MMC_RESP_R1:
        case MMC_RESP_R1B:
            reg->bits.rspExpect = 1;
            reg->bits.rspLen = 0;
            reg->bits.checkRspCrc = 1;
            break;
        case MMC_RESP_R2:
            reg->bits.rspExpect = 1;
            reg->bits.rspLen = 1;
            reg->bits.checkRspCrc = 1;
            break;
        case MMC_RESP_R3:
        case MMC_RESP_R1 & (~RESP_CRC):
            reg->bits.rspExpect = 1;
            reg->bits.rspLen = 0;
            reg->bits.checkRspCrc = 0;
            break;
        default:
            cmd->returnError = HDF_ERR_INVALID_PARAM;
            HDF_LOGE("unhandled response type 0x%x", MMC_RESP_TYPE(cmd));
            return HDF_ERR_INVALID_PARAM;
    }

    reg->bits.sendInitialization = 0;
    if (cmd->cmdCode == GO_IDLE_STATE) {
        reg->bits.sendInitialization = 1;
    }
    /* CMD 11 check switch voltage */
    reg->bits.voltSwitch = 0;
    if (cmd->cmdCode == SD_CMD_SWITCH_VOLTAGE) {
        reg->bits.voltSwitch = 1;
    }

    reg->bits.cardNumber = 0;
    reg->bits.cmdIndex = cmd->cmdCode;
    reg->bits.startCmd = 1;
    reg->bits.updateClkRegOnly = 0;
    return HDF_SUCCESS;
}

static int32_t HimciUpdateCmdReg(union HimciCmdRegArg *reg, struct HimciHost *host)
{
    struct MmcCmd *cmd = host->cmd;
    struct MmcData *data = cmd->data;

    if (data != NULL) {
        reg->bits.dataTransferExpected = 1;
        if ((data->dataFlags & (DATA_WRITE | DATA_READ)) > 0) {
            reg->bits.transferMode = 0;
        }
        if ((data->dataFlags & DATA_STREAM) > 0) {
            reg->bits.transferMode = 1;
        }
        if ((data->dataFlags & DATA_WRITE) > 0) {
            reg->bits.readWrite = 1;
        } else if ((data->dataFlags & DATA_READ) > 0) {
            reg->bits.readWrite = 0;
        }
        reg->bits.sendAutoStop = 0;
        if (HimciIsMultiBlock(cmd) == true && HimciNeedAutoStop(host->mmc) == true) {
            reg->bits.sendAutoStop = 1;
        }
    } else {
        reg->bits.dataTransferExpected = 0;
        reg->bits.transferMode = 0;
        reg->bits.readWrite = 0;
    }

    if (HimciFillCmdReg(reg, cmd) != HDF_SUCCESS) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t HimciExecCmd(struct HimciHost *host)
{
    union HimciCmdRegArg cmdRegs;
    int32_t ret;
    struct MmcCmd *cmd = host->cmd;

    HIMCI_WRITEL(cmd->argument, (uintptr_t)host->base + MMC_CMDARG);
    cmdRegs.arg = HIMCI_READL((uintptr_t)host->base + MMC_CMD);
    ret = HimciUpdateCmdReg(&cmdRegs, host);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    HIMCI_WRITEL(cmdRegs.arg, (uintptr_t)host->base + MMC_CMD);
    return HDF_SUCCESS;
}

static int32_t HimciWaitCmd(struct HimciHost *host)
{
    int32_t reties = 0;
    uint32_t val;
    unsigned long flags = 0;

    while (true) {
        /*
         * Check if CMD start_cmd bit is clear.
         * start_cmd = 0 means MMC Host controller has loaded registers and next command can be loaded in.
         */
        val = HIMCI_READL((uintptr_t)host->base + MMC_CMD);
        if ((val & START_CMD) == 0) {
            break;
        }
        /* Check if Raw_Intr_Status HLE bit is set. */
        HIMCI_IRQ_LOCK(&flags);
        val = HIMCI_READL((uintptr_t)host->base + MMC_RINTSTS);
        if ((val & HLE_INT_STATUS) > 0) {
            val |= HLE_INT_STATUS;
            HIMCI_WRITEL(val, (uintptr_t)host->base + MMC_RINTSTS);
            HIMCI_IRQ_UNLOCK(flags);
            HDF_LOGE("host%d: Other CMD is running! please operate cmd again!", host->id);
            return HDF_MMC_ERR_OTHER_CMD_IS_RUNNING;
        }
        HIMCI_IRQ_UNLOCK(flags);
        OsalUDelay(100);

        /* Check if number of retries for this are over. */
        reties++;
        if (reties >= HIMCI_MAX_RETRY_COUNT) {
            if (host->cmd != NULL) {
                HDF_LOGE("wait cmd[%u] complete is timeout!", host->cmd->cmdCode);
            } else {
                HDF_LOGE("timeout!");
            }
            return HDF_FAILURE;
        }
    }
    return HDF_SUCCESS;
}

static void HimciCmdDone(struct HimciHost *host)
{
    uint32_t i;
    struct MmcCmd *cmd = host->cmd;

    if ((cmd->respType & RESP_PRESENT) == 0) {
        return;
    }

    if (MMC_RESP_TYPE(cmd) != MMC_RESP_R2) {
        cmd->resp[0] = HIMCI_READL((uintptr_t)host->base + MMC_RESP0);
        return;
    }

    for (i = 0; i < MMC_CMD_RESP_SIZE; i++) {
        cmd->resp[i] = HIMCI_READL((uintptr_t)host->base + MMC_RESP3 - i * 0x4);
        /* R2 must delay some time here when use UHI card. */
        OsalUDelay(1000);
    }
}

static void HimciDataSync(struct HimciHost *host, struct MmcData *data)
{
    uint32_t sgPhyAddr, sgLength, i;

    if ((data->dataFlags & DATA_READ) > 0) {
        for (i = 0; i < host->dmaSgNum; i++) {
            sgLength = HIMCI_SG_DMA_LEN(&host->sg[i]);
            sgPhyAddr = HIMCI_SG_DMA_ADDRESS(&host->sg[i]);
            HimciDmaCacheInv((void *)(uintptr_t)sgPhyAddr, sgLength);
        }
    }
}

static void HimciDataDone(struct HimciHost *host, uint32_t state)
{
    struct MmcData *data = NULL;

    if (host->cmd == NULL) {
        return;
    }
    if (host->cmd->data == NULL) {
        return;
    }

    data = host->cmd->data;
    if ((state & (HTO_INT_STATUS | DRTO_INT_STATUS | RTO_INT_STATUS)) > 0) {
        data->returnError = HDF_ERR_TIMEOUT;
    } else if ((state & (EBE_INT_STATUS | SBE_INT_STATUS | FRUN_INT_STATUS | DCRC_INT_STATUS)) > 0) {
        data->returnError = HDF_MMC_ERR_ILLEGAL_SEQ;
    }
}

static void HimciWaitCmdComplete(struct HimciHost *host)
{
    struct MmcCmd *cmd = host->cmd;
    uint32_t timeout, status;
    unsigned long flags = 0;

    if (host->isTuning == true) {
        timeout = HIMCI_TUNINT_REQ_TIMEOUT;
    } else {
        timeout = HIMCI_REQUEST_TIMEOUT;
    }

    HIMCI_IRQ_LOCK(&flags);
    host->waitForEvent = true;
    HIMCI_IRQ_UNLOCK(flags);

    status = HIMCI_EVENT_WAIT(&host->himciEvent, (HIMCI_PEND_DTO_M | HIMCI_PEND_ACCIDENT), timeout);
    if (status == LOS_ERRNO_EVENT_READ_TIMEOUT || status == HIMCI_PEND_ACCIDENT) {
        if (status == HIMCI_PEND_ACCIDENT) {
            cmd->returnError = HDF_ERR_IO;
        } else {
            cmd->returnError = HDF_ERR_TIMEOUT;
            if (host->isTuning == false) {
                HimciDumpRegs(host);
                HDF_LOGE("host%d cmd%d(arg 0x%x) timeout!", host->id, cmd->cmdCode, cmd->argument);
            }
        }
        if (host->cmd->data != NULL) {
            HimciDmaStop(host);
            HimciDataDone(host, 0);
        }
    } else if (host->cmd->data != NULL) {
        HimciDataSync(host, host->cmd->data);
    }

    HIMCI_IRQ_LOCK(&flags);
    host->waitForEvent = false;
    HIMCI_IRQ_UNLOCK(flags);
    HimciCmdDone(host);
}

static bool HimciCardPluged(struct MmcCntlr *cntlr)
{
    unsigned int status;
    struct HimciHost *host = NULL;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return false;
    }

    if (cntlr->devType == MMC_DEV_SDIO || cntlr->devType == MMC_DEV_EMMC) {
        return true;
    }

    host = (struct HimciHost *)cntlr->priv;
    status = HIMCI_READL((uintptr_t)host->base + MMC_CDETECT);
    if ((status & CARD_UNPLUGED) == 0) {
        return true;
    }
    return false;
}

static int32_t HimciSendCmd23(struct HimciHost *host, uint32_t blockNum)
{
    int32_t ret;
    struct MmcCmd cmd = {0};

    cmd.cmdCode = SET_BLOCK_COUNT;
    cmd.argument = blockNum;
    host->cmd = &cmd;
    ret = HimciExecCmd(host);
    if (ret != HDF_SUCCESS) {
        host->cmd = NULL;
        HDF_LOGE("cmd23 failed, ret = %d!", ret);
        return ret;
    }

    HimciWaitCmdComplete(host);
    host->cmd = NULL;
    return cmd.returnError;
}

static int32_t HimciFifoReset(struct HimciHost *host)
{
    uint32_t i;

    HIMCI_SETL(host, MMC_CTRL, FIFO_RESET);
    for (i = 0; i < HIMCI_MAX_RETRY_COUNT; i++) {
        if ((HIMCI_READL((uintptr_t)host->base + MMC_CTRL) & FIFO_RESET) == 0) {
            return HDF_SUCCESS;
        }
    }
    return HDF_ERR_TIMEOUT;
}

static int32_t HimciFillDmaSg(struct HimciHost *host, struct MmcData *data)
{
    uint32_t len = data->blockNum * data->blockSize;
    int32_t ret;

    if (len == 0) {
        return HDF_ERR_INVALID_PARAM;
    }

    if (data->scatter != NULL && data->dataBuffer == NULL) {
        host->sg = data->scatter;
        host->dmaSgNum = data->scatterLen;
        return HDF_SUCCESS;
    }
    if (data->dataBuffer == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }

    host->alignedBuff = (uint8_t *)OsalMemAllocAlign(CACHE_ALIGNED_SIZE, ALIGN(len, CACHE_ALIGNED_SIZE));
    if (host->alignedBuff == NULL) {
        HDF_LOGE("HimciFillDmaSg: alloc fail.");
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = memcpy_s(host->alignedBuff, len, data->dataBuffer, len);
    if (ret != EOK) {
        HDF_LOGE("memcpy_s fail ret = %d.", ret);
        OsalMemFree(host->alignedBuff);
        host->alignedBuff = NULL;
        return HDF_FAILURE;
    }
    host->buffLen = len;
    sg_init_one(&host->dmaSg, (const void *)host->alignedBuff, len);
    host->dmaSgNum = 1;
    host->sg = &host->dmaSg;
    return HDF_SUCCESS;
}

static void HimciClearDmaSg(struct HimciHost *host, struct MmcData *data)
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
    host->dmaSgNum = 0;
    host->buffLen = 0;
    host->sg = NULL;
}

static int32_t HimciSetupData(struct HimciHost *host, struct MmcData *data)
{
    int32_t ret;
    uint32_t sgPhyAddr, sgLength, i;
    uint32_t desCnt = 0;
    uint32_t maximum = HIMCI_PAGE_SIZE / sizeof(struct HimciDes);
    struct HimciDes *des = NULL;
    uint32_t dmaDir = DMA_TO_DEVICE;

    if ((data->dataFlags & DATA_READ) > 0) {
        dmaDir = DMA_FROM_DEVICE;
    }

    des = (struct HimciDes *)host->dmaVaddr;
    for (i = 0; (i < host->dmaSgNum) && (desCnt < maximum); i++) {
        sgLength = HIMCI_SG_DMA_LEN(&host->sg[i]);
        sgPhyAddr = HIMCI_SG_DMA_ADDRESS(&host->sg[i]);
        if ((sgPhyAddr & (CACHE_ALIGNED_SIZE - 1)) != 0) {
            HDF_LOGE("host%d:sg_phyaddr:0x%x sg_length:0x%x.", host->id, sgPhyAddr, sgLength);
            return HDF_FAILURE;
        }
        if (dmaDir == DMA_TO_DEVICE) {
            HimciDmaCacheClean((void *)(uintptr_t)sgPhyAddr, sgLength);
        } else {
            HimciDmaCacheInv((void *)(uintptr_t)sgPhyAddr, sgLength);
        }
        while (sgLength && (desCnt < maximum)) {
            des[desCnt].dmaDesCtrl = DMA_DES_OWN | DMA_DES_NEXT_DES;
            des[desCnt].dmaDesBufAddr = sgPhyAddr;
            /* idmac_des_next_addr is paddr for dma */
            des[desCnt].dmaDesNextAddr = host->dmaPaddr + (desCnt + 1) * sizeof(struct HimciDes);
            if (sgLength >= HIMCI_DMA_MAX_BUFF_SIZE) {
                des[desCnt].dmaDesBufSize = HIMCI_DMA_MAX_BUFF_SIZE;
                sgLength -= HIMCI_DMA_MAX_BUFF_SIZE;
                sgPhyAddr += HIMCI_DMA_MAX_BUFF_SIZE;
            } else {
                /* data alignment */
                des[desCnt].dmaDesBufSize = sgLength;
                sgLength = 0;
            }
            desCnt++;
        }
    }
    des[0].dmaDesCtrl |= DMA_DES_FIRST_DES;
    des[desCnt - 1].dmaDesCtrl |= DMA_DES_LAST_DES;
    des[desCnt - 1].dmaDesNextAddr = 0;

    HimciDmaCacheClean((void *)(uintptr_t)host->dmaPaddr, HIMCI_PAGE_SIZE);

    desCnt = data->blockSize * data->blockNum;
    HIMCI_WRITEL(desCnt, (uintptr_t)host->base + MMC_BYTCNT);
    HIMCI_WRITEL(data->blockSize, (uintptr_t)host->base + MMC_BLKSIZ);

    ret = HimciFifoReset(host);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    HimciDmaStart(host);
    return HDF_SUCCESS;
}

static bool HimciWaitCardComplete(struct HimciHost *host)
{
    uint64_t timeout;
    uint32_t cycle, busy;

    timeout = LOS_TickCountGet() + HIMCI_CARD_COMPLETE_TIMEOUT;
    do {
        for (cycle = 0; cycle < HIMCI_MAX_RETRY_COUNT; cycle++) {
            busy = HIMCI_READL((uintptr_t)host->base + MMC_STATUS);
            if ((busy & DATA_BUSY) == 0) {
                return true;
            }
        }
        if (HimciCardPluged(host->mmc) == false) {
            HDF_LOGE("card is unplugged.");
            return false;
        }
        LOS_Schedule();
    } while (LOS_TickCountGet() < timeout);

    return false;
}

static int32_t HimciDoRequest(struct MmcCntlr *cntlr, struct MmcCmd *cmd)
{
    struct HimciHost *host = NULL;
    int32_t ret = HDF_SUCCESS;

    if ((cntlr == NULL) || (cntlr->priv == NULL) || (cmd == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct HimciHost *)cntlr->priv;
    (void)OsalMutexLock(&host->mutex);
    if (HimciCardPluged(cntlr) == false) {
        cmd->returnError = HDF_PLT_ERR_NO_DEV;
        goto _END;
    }

    if (HimciWaitCardComplete(host) == false) {
        HDF_LOGE("card is busy, can not send cmd.");
        cmd->returnError = HDF_ERR_TIMEOUT;
        goto _END;
    }

    host->cmd = cmd;
    if (cmd->data != NULL) {
        if (HimciIsMultiBlock(cmd) == true && HimciNeedAutoStop(cntlr) == false) {
            ret = HimciSendCmd23(host, cmd->data->blockNum);
            if (ret != HDF_SUCCESS) {
                cmd->returnError = ret;
                goto _END;
            }
        }
        host->cmd = cmd;
        ret = HimciFillDmaSg(host, cmd->data);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        ret = HimciSetupData(host, cmd->data);
        if (ret != HDF_SUCCESS) {
            cmd->data->returnError = ret;
            HDF_LOGE("setup data fail, err = %d.", ret);
            goto _END;
        }
    } else {
        HIMCI_WRITEL(0, (uintptr_t)host->base + MMC_BYTCNT);
        HIMCI_WRITEL(0, (uintptr_t)host->base + MMC_BLKSIZ);
    }

    ret = HimciExecCmd(host);
    if (ret != HDF_SUCCESS) {
        cmd->returnError = ret;
        HimciDmaStop(host);
        HDF_LOGE("cmd%d exec fail, err = %d!", cmd->cmdCode, ret);
        goto _END;
    }
    HimciWaitCmdComplete(host);

_END:
    HimciClearDmaSg(host, cmd->data);
    host->cmd = NULL;
    (void)OsalMutexUnlock(&host->mutex);
    return ret;
}

static void HimciControlClock(struct HimciHost *host, bool enableClk)
{
    uint32_t value;
    union HimciCmdRegArg cmdArg;

    value = HIMCI_READL((uintptr_t)host->base + MMC_CLKENA);
    if (enableClk == true) {
        value |= CCLK_ENABLE;
        /* Do not set/clear CCLK_LOW_POWER here,or the cmd18 will timeout. */
    } else {
        value &= (~CCLK_ENABLE);
    }
    if (host->mmc->devType == MMC_DEV_SDIO) {
        value &= (~CCLK_LOW_POWER);
    }
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_CLKENA);

    cmdArg.arg = HIMCI_READL((uintptr_t)host->base + MMC_CMD);
    cmdArg.bits.startCmd = 1;
    cmdArg.bits.cardNumber = 0;
    cmdArg.bits.cmdIndex = 0;
    cmdArg.bits.dataTransferExpected = 0;
    cmdArg.bits.useHoldReg = 1;
    cmdArg.bits.updateClkRegOnly = 1;
    HIMCI_WRITEL(cmdArg.arg, (uintptr_t)host->base + MMC_CMD);

    if (HimciWaitCmd(host) != HDF_SUCCESS) {
        HDF_LOGE("dis/enable clk is err!");
    }
}

static void HimciSetCClk(struct HimciHost *host, uint32_t clock)
{
    uint32_t clk = 0;
    union HimciCmdRegArg cmdArg;
    struct MmcCntlr *mmc = host->mmc;
    struct MmcDevice *dev = mmc->curDev;

    (void)OsalMutexLock(&host->mutex);
    if (host->id < MMC_CNTLR_NR_MAX) {
        clk = HimciClkDiv(host, clock);
    }
    (void)OsalMutexUnlock(&host->mutex);
    dev->workPara.clock = clk;

    cmdArg.arg = HIMCI_READL((uintptr_t)host->base + MMC_CMD);
    cmdArg.bits.startCmd = 1;
    cmdArg.bits.cardNumber = 0;
    cmdArg.bits.updateClkRegOnly = 1;
    cmdArg.bits.cmdIndex = 0;
    cmdArg.bits.dataTransferExpected = 0;
    HIMCI_WRITEL(cmdArg.arg, (uintptr_t)host->base + MMC_CMD);

    if (HimciWaitCmd(host) != HDF_SUCCESS) {
        HDF_LOGE("host%d: set card clk divider is failed!", host->id);
    }
}

static int32_t HimciSetClock(struct MmcCntlr *cntlr, uint32_t clock)
{
    struct HimciHost *host = NULL;
    uint32_t curClock = clock;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }
    /* can not greater than max of host. */
    if (curClock > cntlr->freqMax) {
        curClock = cntlr->freqMax;
    }

    host = (struct HimciHost *)cntlr->priv;
    HimciControlClock(host, false);
    if (curClock > 0) {
        HimciSetCClk(host, curClock);
        HimciControlClock(host, true);
    }
    return HDF_SUCCESS;
}

static void HimciControlPower(struct HimciHost *host, enum HimciPowerStatus status, bool forceEnable)
{
    uint32_t value;

    if (host->powerStatus != status || forceEnable == true) {
        value = HIMCI_READL((uintptr_t)host->base + MMC_PWREN);
        if (status == HOST_POWER_OFF) {
            value &= (~POWER_ENABLE);
        } else {
            value |= POWER_ENABLE;
        }
        HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_PWREN);
        OsalMDelay(50);
        host->powerStatus = status;
    }
}

static int32_t HimciSetPowerMode(struct MmcCntlr *cntlr, enum MmcPowerMode mode)
{
    struct HimciHost *host = NULL;
    uint32_t value;
    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct HimciHost *)cntlr->priv;
    if (mode == MMC_POWER_MODE_POWER_OFF) {
        value = HIMCI_READL((uintptr_t)host->base + MMC_UHS_REG);
        value &= (~HI_SDXC_CTRL_VDD_180);
        HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_UHS_REG);
        HimciControlPower(host, HOST_POWER_OFF, false);
    } else {
        HimciControlPower(host, HOST_POWER_ON, true);
    }
    return HDF_SUCCESS;
}

static int32_t HimciSetBusWidth(struct MmcCntlr *cntlr, enum MmcBusWidth width)
{
    struct HimciHost *host = NULL;
    uint32_t value;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct HimciHost *)cntlr->priv;
    value = HIMCI_READL((uintptr_t)host->base + MMC_CTYPE);
    value &= (~(CARD_WIDTH_0 | CARD_WIDTH_1));

    if (width == BUS_WIDTH8) {
        value |= CARD_WIDTH_0;
        HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_CTYPE);
    } else if (width == BUS_WIDTH4) {
        value |= CARD_WIDTH_1;
        HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_CTYPE);
    } else {
        HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_CTYPE);
    }
    return HDF_SUCCESS;
}

static void HimciCfgPhase(struct HimciHost *host, enum MmcBusTiming timing)
{
    uint32_t value;
    uint32_t phase;
    struct MmcDevice *dev = host->mmc->curDev;

    if (dev->type == MMC_DEV_EMMC) {
        if (timing == BUS_TIMING_MMC_HS200) {
            phase = DRV_PHASE_135 | SMP_PHASE_0;
        } else if (timing == BUS_TIMING_MMC_HS) {
            phase = DRV_PHASE_180 | SMP_PHASE_45;
        } else {
            phase = DRV_PHASE_180 | SMP_PHASE_0;
        }
    } else {
        if (timing == BUS_TIMING_UHS_SDR104) {
            phase = DRV_PHASE_135 | SMP_PHASE_0;
        } else if (timing == BUS_TIMING_UHS_SDR50) {
            phase = DRV_PHASE_90 | SMP_PHASE_0;
        } else if (timing == BUS_TIMING_UHS_SDR25) {
            phase = DRV_PHASE_180 | SMP_PHASE_45;
        } else if (timing == BUS_TIMING_SD_HS) {
            phase = DRV_PHASE_135 | SMP_PHASE_45;
        } else {
            phase = DRV_PHASE_180 | SMP_PHASE_0;
        }
    }
    value = HIMCI_READL((uintptr_t)host->base + MMC_UHS_REG_EXT);
    value &= ~CLK_SMPL_PHS_MASK;
    value &= ~CLK_DRV_PHS_MASK;
    value |= phase;
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_UHS_REG_EXT);
}

static int32_t HimciSetBusTiming(struct MmcCntlr *cntlr, enum MmcBusTiming timing)
{
    struct HimciHost *host = NULL;
    uint32_t value;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct HimciHost *)cntlr->priv;
    value = HIMCI_READL((uintptr_t)host->base + MMC_UHS_REG);
    /* speed mode check ,if it is DDR50 set DDR mode */
    if (timing == BUS_TIMING_UHS_DDR50) {
        if ((value & HI_SDXC_CTRL_DDR_REG) == 0) {
            value |= HI_SDXC_CTRL_DDR_REG;
        }
    } else {
        if ((value & HI_SDXC_CTRL_DDR_REG) > 0) {
            value &= (~HI_SDXC_CTRL_DDR_REG);
        }
    }
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_UHS_REG);
    HimciCfgPhase(host, timing);
    return HDF_SUCCESS;
}

static int32_t HimciSetSdioIrq(struct MmcCntlr *cntlr, bool enable)
{
    struct HimciHost *host = NULL;
    uint32_t value;
    unsigned long flags = 0;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct HimciHost *)cntlr->priv;
    HIMCI_IRQ_LOCK(&flags);
    value = HIMCI_READL((uintptr_t)host->base + MMC_INTMASK);
    if (enable == true) {
        value |= SDIO_INT_MASK;
    } else {
        value &= (~SDIO_INT_MASK);
    }
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_INTMASK);
    HIMCI_IRQ_UNLOCK(flags);
    return HDF_SUCCESS;
}

static int32_t HimciHardwareReset(struct MmcCntlr *cntlr)
{
    uint32_t val;
    struct HimciHost *host = NULL;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct HimciHost *)cntlr->priv;
    val = HIMCI_READL((uintptr_t)host->base + MMC_CARD_RSTN);
    val &= (~CARD_RESET);
    HIMCI_WRITEL(val, (uintptr_t)host->base + MMC_CARD_RSTN);

    /* For eMMC, minimum is 1us but give it 10us for good measure */
    OsalUDelay(10);
    val = HIMCI_READL((uintptr_t)host->base + MMC_CARD_RSTN);
    val |= CARD_RESET;
    HIMCI_WRITEL(val, (uintptr_t)host->base + MMC_CARD_RSTN);
    OsalUDelay(300);
    return HDF_SUCCESS;
}

static int32_t HimciSetEnhanceSrobe(struct MmcCntlr *cntlr, bool enable)
{
    (void)cntlr;
    (void)enable;
    return HDF_SUCCESS;
}

static int32_t HimciVoltageSwitchTo3v3(struct MmcCntlr *cntlr, struct HimciHost *host)
{
    uint32_t ctrl;

    HIMCI_CLEARL(host, MMC_UHS_REG, HI_SDXC_CTRL_VDD_180);
    OsalMSleep(10);
    ctrl = HIMCI_READL((uintptr_t)host->base + MMC_UHS_REG);
    if ((ctrl & HI_SDXC_CTRL_VDD_180) > 0) {
        HDF_LOGD("host%d: Switching to 3.3V failed\n", host->id);
        return HDF_ERR_IO;
    }
    HimciSetDrvCap(cntlr);
    return HDF_SUCCESS;
}

static int32_t HimciVoltageSwitchTo1v8(struct MmcCntlr *cntlr, struct HimciHost *host)
{
    uint32_t ctrl;

    ctrl = HIMCI_READL((uintptr_t)host->base + MMC_UHS_REG);
    if ((ctrl & HI_SDXC_CTRL_VDD_180) > 0) {
        return HDF_SUCCESS;
    }

    HimciControlClock(host, false);
    HIMCI_SETL(host, MMC_UHS_REG, HI_SDXC_CTRL_VDD_180);
    OsalMSleep(10);
    ctrl = HIMCI_READL((uintptr_t)host->base + MMC_UHS_REG);
    if ((ctrl & HI_SDXC_CTRL_VDD_180) > 0) {
        HimciControlClock(host, true);
        OsalMSleep(10);
        if (host->mmc->caps2.bits.hs200Sdr1v8 || host->mmc->caps2.bits.hs200Sdr1v2) {
            /* emmc needn't to check the int status. */
            return HDF_SUCCESS;
        }
        /* If CMD11 return CMD down, then the card was successfully switched to 1.8V signaling. */
        ctrl = HIMCI_EVENT_WAIT(&host->himciEvent, HIMCI_PEND_DTO_M, HIMCI_VOLT_SWITCH_TIMEOUT);
        if ((ctrl & HIMCI_PEND_DTO_M) > 0) {
            /* config Pin drive capability */
            HimciSetDrvCap(cntlr);
            return HDF_SUCCESS;
        }
    }

    ctrl &= (~HI_SDXC_CTRL_VDD_180);
    HIMCI_WRITEL(ctrl, (uintptr_t)host->base + MMC_UHS_REG);
    OsalMSleep(10);
    HimciControlPower(host, HOST_POWER_OFF, false);
    OsalMSleep(10);
    HimciControlPower(host, HOST_POWER_ON, false);
    HimciControlClock(host, false);
    OsalMSleep(1);
    HimciControlClock(host, true);
    ctrl = HIMCI_EVENT_WAIT(&host->himciEvent, HIMCI_PEND_DTO_M, 10);
    if ((ctrl & HIMCI_PEND_DTO_M) > 0) {
        /* config Pin drive capability */
        HimciSetDrvCap(cntlr);
        return HDF_SUCCESS;
    }

    HDF_LOGD("Switching to 1.8V failed, retrying with S18R set to 0.");
    return HDF_FAILURE;
}

static int32_t HimciSwitchVoltage(struct MmcCntlr *cntlr, enum MmcVolt volt)
{
    struct HimciHost *host = NULL;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct HimciHost *)cntlr->priv;
    if (volt == VOLT_3V3) {
        return HimciVoltageSwitchTo3v3(cntlr, host);
    } else if (volt == VOLT_1V8) {
        return HimciVoltageSwitchTo1v8(cntlr, host);
    }
    return HDF_SUCCESS;
}

static bool HimciDevReadOnly(struct MmcCntlr *cntlr)
{
    struct HimciHost *host = NULL;
    uint32_t val;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return false;
    }

    host = (struct HimciHost *)cntlr->priv;
    val = HIMCI_READL((uintptr_t)host->base + MMC_WRTPRT);
    if ((val & CARD_READONLY) > 0) {
        return true;
    }
    return false;
}

static bool HimciDevBusy(struct MmcCntlr *cntlr)
{
    (void)cntlr;
    return false;
}

static void HimciEdgeTuningEnable(struct HimciHost *host)
{
    uint32_t val;
    uint32_t regs[] = { PERI_CRG83, PERI_CRG89, PERI_CRG86 };

    if (host->id >= MMC_CNTLR_NR_MAX) {
        HDF_LOGE("host%d id error", host->id);
        return;
    }

    HIMCI_WRITEL((HIMCI_SAP_DLL_SOFT_RESET | HIMCI_SAP_DLL_DEVICE_DELAY_ENABLE), regs[host->id]);

    val = HIMCI_READL((uintptr_t)host->base + MMC_TUNING_CTRL);
    val |= HW_TUNING_EN;
    HIMCI_WRITEL(val, (uintptr_t)host->base + MMC_TUNING_CTRL);
}

static void HimciSetSapPhase(struct HimciHost *host, uint32_t phase)
{
    uint32_t val;

    val = HIMCI_READL((uintptr_t)host->base + MMC_UHS_REG_EXT);
    val &= ~CLK_SMPL_PHS_MASK;
    val |= (phase << CLK_SMPL_PHS_OFFSET);
    HIMCI_WRITEL(val, (uintptr_t)host->base + MMC_UHS_REG_EXT);
}

static void HimciEdgeTuningDisable(struct HimciHost *host)
{
    uint32_t val;
    uint32_t regs[] = { PERI_CRG83, PERI_CRG89, PERI_CRG86 };

    if (host->id >= MMC_CNTLR_NR_MAX) {
        HDF_LOGE("host%d id error", host->id);
        return;
    }

    val = HIMCI_READL(regs[host->id]);
    val |= HIMCI_SAP_DLL_MODE_DLLSSEL;
    HIMCI_WRITEL(val, regs[host->id]);

    val = HIMCI_READL((uintptr_t)host->base + MMC_TUNING_CTRL);
    val &= ~HW_TUNING_EN;
    HIMCI_WRITEL(val, (uintptr_t)host->base + MMC_TUNING_CTRL);
}

static int32_t HimciSendTuning(struct MmcCntlr *cntlr, uint32_t opcode)
{
    int32_t err;
    uint32_t result;
    struct HimciHost *host = (struct HimciHost *)cntlr->priv;

    (void)OsalMutexLock(&host->mutex);
    HimciControlClock(host, false);
    HimciDmaReset(host);
    HimciControlClock(host, true);
    (void)OsalMutexUnlock(&host->mutex);

    err = MmcSendTuning(cntlr, opcode, true);
    (void)MmcStopTransmission(cntlr, true, &result);
    (void)MmcSendStatus(cntlr, &result);
    return err;
}

static void HimciSysReset(struct HimciHost *host)
{
    uint32_t value;

    value = HIMCI_READL((uintptr_t)host->base + MMC_BMOD);
    value |= BMOD_SWR;
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_BMOD);
    OsalUDelay(10);

    value = HIMCI_READL((uintptr_t)host->base + MMC_BMOD);
    value |= (BURST_INCR | BURST_16);
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_BMOD);

    value = HIMCI_READL((uintptr_t)host->base + MMC_CTRL);
    value |=  (CTRL_RESET | FIFO_RESET | DMA_RESET);
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_CTRL);
}

static void HimciTuningFeedback(struct MmcCntlr *cntlr)
{
    struct HimciHost *host = (struct HimciHost *)cntlr->priv;

    (void)OsalMutexLock(&host->mutex);
    HimciControlClock(host, false);
    OsalMDelay(1);
    HimciSysReset(host);
    OsalMDelay(1);
    HIMCI_WRITEL(ALL_INT_CLR, (uintptr_t)host->base + MMC_RINTSTS);
    HimciControlClock(host, true);
    OsalMDelay(1);
    (void)OsalMutexUnlock(&host->mutex);
}

static uint32_t HimciGetSapDllTaps(struct HimciHost *host)
{
    uint32_t val;
    uint32_t regs[] = { PERI_CRG84, PERI_CRG90, PERI_CRG87 };

    if (host->id >= MMC_CNTLR_NR_MAX) {
        HDF_LOGE("host%d id error", host->id);
        return 0;
    }

    val = HIMCI_READL(regs[host->id]);
    return (val & 0xff);
}

static void HimciSetDllElement(struct HimciHost *host, uint32_t element)
{
    uint32_t val;
    uint32_t regs[] = { PERI_CRG83, PERI_CRG89, PERI_CRG86 };

    if (host->id >= MMC_CNTLR_NR_MAX) {
        HDF_LOGE("host%d id error", host->id);
        return;
    }

    val = HIMCI_READL(regs[host->id]);
    val &= ~(0xFF << HIMCI_SAP_DLL_ELEMENT_SHIFT);
    val |= (element << HIMCI_SAP_DLL_ELEMENT_SHIFT);
    HIMCI_WRITEL(val, regs[host->id]);
}

static void HimciEdgedllModeATuning(struct HimciHost *host, struct HimciTuneParam *param,
    uint32_t phaseDllElements)
{
    uint32_t index, ele, phaseOffset;
    int32_t prevErr = HDF_SUCCESS;
    int32_t err;

    if (host == NULL || param == NULL) {
        return;
    }

    phaseOffset = param->edgeP2f * phaseDllElements;
    for (index = param->edgeP2f; index < param->edgeF2p; index++) {
        HimciSetSapPhase(host, index);
        for (ele = HIMCI_PHASE_DLL_START_ELEMENT; ele <= phaseDllElements; ele++) {
            HimciSetDllElement(host, ele);
            err = HimciSendTuning(host->mmc, param->cmdCode);
            if (prevErr == HDF_SUCCESS && err != HDF_SUCCESS && (param->endp == param->endpInit)) {
                param->endp = phaseOffset + ele;
            }

            if (err != HDF_SUCCESS) {
                param->startp = phaseOffset + ele;
            }
            prevErr = err;
            err = HDF_SUCCESS;
        }
        phaseOffset += phaseDllElements;
    }
}

static void HimciEdgedllModeBTuning(struct HimciHost *host, struct HimciTuneParam *param,
    uint32_t phaseDllElements)
{
    uint32_t index, ele, phaseOffset;
    int32_t prevErr = HDF_SUCCESS;
    int32_t err;

    if (host == NULL || param == NULL) {
        return;
    }

    phaseOffset = param->edgeP2f * phaseDllElements;
    for (index = param->edgeP2f; index < HIMCI_PHASE_SCALE; index++) {
        HimciSetSapPhase(host, index);
        for (ele = HIMCI_PHASE_DLL_START_ELEMENT; ele <= phaseDllElements; ele++) {
            HimciSetDllElement(host, ele);
            err = HimciSendTuning(host->mmc, param->cmdCode);
            if (prevErr == HDF_SUCCESS && err != HDF_SUCCESS && (param->endp == param->endpInit)) {
                param->endp = phaseOffset + ele;
            }
            if (err != HDF_SUCCESS) {
                param->startp = phaseOffset + ele;
            }
            prevErr = err;
            err = HDF_SUCCESS;
        }
        phaseOffset += phaseDllElements;
    }

    phaseOffset = 0;
    for (index = 0; index < param->edgeF2p; index++) {
        HimciSetSapPhase(host, index);
        for (ele = HIMCI_PHASE_DLL_START_ELEMENT; ele <= phaseDllElements; ele++) {
            HimciSetDllElement(host, ele);
            err = HimciSendTuning(host->mmc, param->cmdCode);
            if (prevErr == HDF_SUCCESS && err != HDF_SUCCESS && (param->endp == param->endpInit)) {
                param->endp = phaseOffset + ele;
            }
            if (err != HDF_SUCCESS) {
                param->startp = phaseOffset + ele;
            }
            prevErr = err;
            err = HDF_SUCCESS;
        }
        phaseOffset += phaseDllElements;
    }
}

static int32_t HimciEdgedllModeTuning(struct HimciHost *host,
    uint32_t cmdCode, uint32_t edgeP2f, uint32_t edgeF2p)
{
    uint32_t index, ele, phaseOffset, totalPhases, phaseDllElements;
    struct HimciTuneParam param = {0};

    phaseDllElements = HimciGetSapDllTaps(host) / HIMCI_PHASE_SCALE;
    totalPhases = phaseDllElements * HIMCI_PHASE_SCALE;
    /*
     *  EdgeMode A:
     * |<---- totalphases(ele) ---->|
     *       _____________
     * ______|||||||||||||||_______
     * edge_p2f       edge_f2p
     * (endp)         (startp)
     *
     * EdgeMode B:
     * |<---- totalphases(ele) ---->|
     * ________           _________
     * ||||||||||_________|||||||||||
     * edge_f2p     edge_p2f
     * (startp)     (endp)
     *
     * BestPhase:
     * if(endp < startp)
     * endp = endp + totalphases;
     * Best = ((startp + endp) / 2) % totalphases
     */
    param.cmdCode = cmdCode;
    param.edgeF2p = edgeF2p;
    param.edgeP2f = edgeP2f;
    param.endpInit = edgeP2f * phaseDllElements;
    param.startp = edgeF2p * phaseDllElements;
    param.endp = param.endpInit;
    if (edgeF2p >= edgeP2f) {
        HimciEdgedllModeATuning(host, &param, phaseDllElements);
    } else {
        HimciEdgedllModeBTuning(host, &param, phaseDllElements);
    }

    if (param.endp <= param.startp) {
        param.endp += totalPhases;
    }
    if (totalPhases == 0) {
        HDF_LOGD("host%d:total phases is zero.", host->id);
        return HDF_FAILURE;
    }
    phaseOffset = ((param.startp + param.endp) / 2) % totalPhases;
    index = (phaseOffset / phaseDllElements);
    ele = (phaseOffset % phaseDllElements);
    ele = ((ele > HIMCI_PHASE_DLL_START_ELEMENT) ? ele : HIMCI_PHASE_DLL_START_ELEMENT);
    HimciSetSapPhase(host, index);
    HimciSetDllElement(host, ele);
    HIMCI_WRITEL(ALL_INT_CLR, (uintptr_t)host->base + MMC_RINTSTS);
    return HDF_SUCCESS;
}

static int32_t HimciTune(struct MmcCntlr *cntlr, uint32_t cmdCode)
{
    struct HimciHost *host = NULL;
    uint32_t index, val;
    bool found = false;
    bool prevFound = false;
    uint32_t edgeP2f = 0;
    uint32_t phaseNum = HIMCI_PHASE_SCALE;
    uint32_t edgeF2p = HIMCI_PHASE_SCALE;
    int32_t err;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct HimciHost *)cntlr->priv;
    host->isTuning = true;
    HimciEdgeTuningEnable(host);
    for (index = 0; index < HIMCI_PHASE_SCALE; index++) {
        found = true;
        HimciSetSapPhase(host, index);
        err = HimciSendTuning(cntlr, cmdCode);
        if (err == HDF_SUCCESS) {
            val = HIMCI_READL((uintptr_t)host->base + MMC_TUNING_CTRL);
            found = ((val & FOUND_EDGE) == FOUND_EDGE);
        }

        if (prevFound == true && found == false) {
            edgeF2p = index;
        } else if (prevFound == false && found == true) {
            edgeP2f = index;
        }

        if ((edgeP2f != 0) && (edgeF2p != phaseNum)) {
            break;
        }
        prevFound = found;
    }

    if ((edgeP2f == 0) && (edgeF2p == phaseNum)) {
        host->isTuning = false;
        return HDF_FAILURE;
    }

    index = (edgeF2p + phaseNum + edgeP2f) / 2 % phaseNum;
    if (edgeF2p < edgeP2f) {
        index = (edgeF2p + edgeP2f) / 2 % phaseNum;
    }
    HimciSetSapPhase(host, index);
    err = HimciSendTuning(cntlr, cmdCode);
    HimciEdgeTuningDisable(host);

    err = HimciEdgedllModeTuning(host, cmdCode, edgeP2f, edgeF2p);
    HimciTuningFeedback(cntlr);
    if (err == HDF_SUCCESS) {
        (void)HimciSendTuning(cntlr, cmdCode);
    }

    host->isTuning = false;
    return HDF_SUCCESS;
}

static void HimciClockCfg(uint32_t devId)
{
    uint32_t val;
    uint32_t regs[] = { PERI_CRG82, PERI_CRG88, PERI_CRG85 };

    if (devId < MMC_CNTLR_NR_MAX) {
        val = HIMCI_READL((uintptr_t)regs[devId]);
        val |= HIMCI_CLK_SEL_100M;
        val |= HIMCI_CKEN;
        HIMCI_WRITEL(val, (uintptr_t)regs[devId]);
    }
}

static void HimciSoftReset(uint32_t devId)
{
    uint32_t regs[] = { PERI_CRG82, PERI_CRG88, PERI_CRG85 };
    uint32_t val;

    if (devId < MMC_CNTLR_NR_MAX) {
        val = HIMCI_READL((uintptr_t)regs[devId]);
        OsalUDelay(1000);
        val |= HIMCI_RESET;
        HIMCI_WRITEL(val, (uintptr_t)regs[devId]);
        val &= ~HIMCI_RESET;
        HIMCI_WRITEL(val, (uintptr_t)regs[devId]);
    }
}

static void HimciSysCtrlInit(struct HimciHost *host)
{
    HIMCI_TASK_LOCK();
    HimciClockCfg(host->id);
    HimciSoftReset(host->id);
    HIMCI_TASK_UNLOCK();
}

static void HimciHostRegistersInit(struct HimciHost *host)
{
    uint32_t value;

    HimciSysReset(host);
    HimciControlPower(host, HOST_POWER_OFF, true);
    /* host power on */
    HimciControlPower(host, HOST_POWER_ON, true);

    /*
     * Walkaround: controller config gpio
     * the value of this register should be 0x80a400,
     * but the reset value is 0xa400. 
     */
    value = HIMCI_READL((uintptr_t)host->base + MMC_GPIO);
    value |= DTO_FIX_ENABLE;
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_GPIO);

    value = ((DRV_PHASE_SHIFT << CLK_DRV_PHS_OFFSET)
           | (SMPL_PHASE_SHIFT << CLK_SMPL_PHS_OFFSET));
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_UHS_REG_EXT);

    HIMCI_WRITEL((READ_THRESHOLD_SIZE | BUSY_CLEAR_INT_ENABLE), (uintptr_t)host->base + MMC_CARDTHRCTL);

    /* clear MMC host intr */
    HIMCI_WRITEL(ALL_INT_CLR, (uintptr_t)host->base + MMC_RINTSTS);

    /*
     * data transfer over(DTO) interrupt comes after ACD
     * we'd use DTO with or without auto_cmd is enabled.
     */
    value = ALL_INT_MASK & (~(ACD_INT_STATUS | RXDR_INT_STATUS | TXDR_INT_STATUS | SDIO_INT_MASK));
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_INTMASK);
    HIMCI_WRITEL(DEBNCE_MS, (uintptr_t)host->base + MMC_DEBNCE);

    /* enable inner DMA mode and close intr of MMC host controler */
    value = HIMCI_READL((uintptr_t)host->base + MMC_CTRL);
    value |= USE_INTERNAL_DMA | INTR_EN;
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_CTRL);

    /* set timeout param */
    HIMCI_WRITEL(DATA_TIMEOUT | RESPONSE_TIMEOUT, (uintptr_t)host->base + MMC_TMOUT);

    /* set FIFO param */
    value = BURST_SIZE | RX_WMARK | TX_WMARK;
    HIMCI_WRITEL(value, (uintptr_t)host->base + MMC_FIFOTH);
}

static int32_t HimciRescanSdioDev(struct MmcCntlr *cntlr)
{
    struct HimciHost *host = NULL;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct HimciHost *)cntlr->priv;
    if (host->waitForEvent == true) {
        (void)HIMCI_EVENT_SIGNAL(&host->himciEvent, HIMCI_PEND_ACCIDENT);
    }

    return MmcCntlrAddSdioRescanMsgToQueue(cntlr);
}

static int32_t HimciSystemInit(struct MmcCntlr *cntlr)
{
    struct HimciHost *host = NULL;

    if ((cntlr == NULL) || (cntlr->priv == NULL)) {
        return HDF_ERR_INVALID_OBJECT;
    }

    host = (struct HimciHost *)cntlr->priv;
    HimciSysCtrlInit(host);
    HimciHostRegistersInit(host);
    return HDF_SUCCESS;
}

static struct MmcCntlrOps g_himciHostOps = {
    .request = HimciDoRequest,
    .setClock = HimciSetClock,
    .setPowerMode = HimciSetPowerMode,
    .setBusWidth = HimciSetBusWidth,
    .setBusTiming = HimciSetBusTiming,
    .setSdioIrq = HimciSetSdioIrq,
    .hardwareReset = HimciHardwareReset,
    .systemInit = HimciSystemInit,
    .setEnhanceSrobe = HimciSetEnhanceSrobe,
    .switchVoltage = HimciSwitchVoltage,
    .devReadOnly = HimciDevReadOnly,
    .devPluged = HimciCardPluged,
    .devBusy = HimciDevBusy,
    .tune = HimciTune,
    .rescanSdioDev = HimciRescanSdioDev,
};

static uint32_t HimciCmdIrq(struct HimciHost *host, uint32_t state)
{
    struct MmcCmd *cmd = host->cmd;
    struct MmcData *data = cmd->data;
    uint32_t writeEvent = 0;
    uint32_t mask;
    int32_t error = HDF_SUCCESS;

    if ((state & RTO_INT_STATUS) > 0) {
        error = HDF_ERR_TIMEOUT;
    } else if ((state & (RCRC_INT_STATUS | RE_INT_STATUS)) > 0) {
        error = HDF_MMC_ERR_ILLEGAL_SEQ;
    }

    mask = (CD_INT_STATUS | VOLT_SWITCH_INT_STATUS);
    if (data == NULL && (state & mask) > 0) {
        writeEvent = 1;
    }

    /* If there is a response timeout(RTO) error,
     * then the DWC_mobile_storage does not attempt any data transfer and
     * the “Data Transfer Over” bit is never set.
     */
    mask = (CD_INT_STATUS | RTO_INT_STATUS);
    if ((state & mask) == mask) {
        writeEvent = 1;
    }
    if (cmd != NULL) {
        cmd->returnError = error;
    }
    return writeEvent;
}

static uint32_t HimciDataIrq(struct HimciHost *host, struct MmcData *data, uint32_t state)
{
    uint32_t writeEvent = 0;

    if (host == NULL || data == NULL) {
        return writeEvent;
    }

    if ((data->dataFlags & DATA_READ) == 0) {
        if ((state & SBE_INT_STATUS) > 0) {
            HimciDmaStop(host);
            HimciDataDone(host, state & (~SBE_INT_STATUS));
            writeEvent++;
        }
    } else {
        HimciDmaStop(host);
        HimciDataDone(host, state);
        writeEvent++;
    }
    return writeEvent;
}

static uint32_t HimciIrqHandler(uint32_t irq, void *data)
{
    struct HimciHost *host = (struct HimciHost *)data;
    struct MmcCmd *cmd = NULL;
    uint32_t writeEvent = 0;
    uint32_t state;
    (void)irq;

    if (host == NULL) {
        HDF_LOGE("HimciIrqHandler: data is null!");
        return HDF_SUCCESS;
    }
    state = HIMCI_READL((uintptr_t)host->base + MMC_RINTSTS);
    HIMCI_WRITEL(state, (uintptr_t)host->base + MMC_RINTSTS);
    if ((state & SDIO_INT_STATUS) > 0) {
        HIMCI_CLEARL(host, MMC_INTMASK, SDIO_INT_MASK);
        (void)MmcCntlrNotifySdioIrqThread(host->mmc);
    }

    if ((state & CARD_DETECT_INT_STATUS) > 0) {
        (void)MmcCntlrAddPlugMsgToQueue(host->mmc);
        if (host->waitForEvent == true) {
            (void)HIMCI_EVENT_SIGNAL(&host->himciEvent, HIMCI_PEND_ACCIDENT);
            return HDF_SUCCESS;
        }
    }

    cmd = host->cmd;
    if (cmd == NULL) {
        return HDF_SUCCESS;
    }

    if ((state & CMD_INT_MASK) > 0) {
        writeEvent += HimciCmdIrq(host, state);
    }

    if ((state & DATA_INT_MASK) > 0) {
        /*
         * SBE_INT_STATUS:
         * Busy Clear Interrupt when data is written to the card
         * In this case, we'd wait for it.
         * Error in data start bit when data is read from a card
         * In this case, we don't need to wait for it. if it's triggered, something is wrong
         */
        if (cmd->data != NULL) {
            writeEvent += HimciDataIrq(host, cmd->data, state);
        } else {
            writeEvent += HimciCmdIrq(host, state);
        }
    }
    if (writeEvent != 0) {
        (void)HIMCI_EVENT_SIGNAL(&host->himciEvent, HIMCI_PEND_DTO_M);
    }
    return HDF_SUCCESS;
}

static int32_t HimciHostInit(struct HimciHost *host, struct MmcCntlr *cntlr)
{
    int32_t ret;

    host->id = (uint32_t)cntlr->index;
    host->dmaVaddr = (uint32_t *)LOS_DmaMemAlloc(&host->dmaPaddr, HIMCI_PAGE_SIZE, CACHE_ALIGNED_SIZE, DMA_CACHE);
    if (host->dmaVaddr == NULL) {
        HDF_LOGE("HimciHostInit: no mem for himci dma!");
        return HDF_ERR_MALLOC_FAIL;
    }

    if (HIMCI_EVENT_INIT(&host->himciEvent) != HDF_SUCCESS) {
        HDF_LOGE("HimciHostInit: himciEvent init fail!");
        return HDF_FAILURE;
    }
    if (OsalMutexInit(&host->mutex) != HDF_SUCCESS) {
        HDF_LOGE("HimciHostInit: init mutex lock fail!");
        return HDF_FAILURE;
    }

    HimciSysCtrlInit(host);
    HimciHostRegistersInit(host);

    ret = OsalRegisterIrq(host->irqNum, 0, HimciIrqHandler, "MMC_IRQ", host);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("HimciHostInit: request irq for himci is err.");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t HimciHostParse(struct HimciHost *host, struct HdfDeviceObject *obj)
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
    return ret;
}

static void HimciDeleteHost(struct HimciHost *host)
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
    if (host->dmaVaddr != NULL) {
        LOS_DmaMemFree(host->dmaVaddr);
    }
    if (host->base != NULL) {
        OsalIoUnmap(host->base);
    }

    (void)HIMCI_EVENT_DELETE(&host->himciEvent);
    (void)OsalMutexDestroy(&host->mutex);
    OsalMemFree(host);
}

static int32_t HimciMmcBind(struct HdfDeviceObject *obj)
{
    struct MmcCntlr *cntlr = NULL;
    struct HimciHost *host = NULL;
    int32_t ret;

    if (obj == NULL) {
        HDF_LOGE("HimciMmcBind: Fail, device is NULL.");
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = (struct MmcCntlr *)OsalMemCalloc(sizeof(struct MmcCntlr));
    if (cntlr == NULL) {
        HDF_LOGE("HimciMmcBind: no mem for MmcCntlr.");
        return HDF_ERR_MALLOC_FAIL;
    }
    host = (struct HimciHost *)OsalMemCalloc(sizeof(struct HimciHost));
    if (host == NULL) {
        HDF_LOGE("HimciMmcBind: no mem for HimciHost.");
        OsalMemFree(cntlr);
        return HDF_ERR_MALLOC_FAIL;
    }

    host->mmc = cntlr;
    cntlr->priv = (void *)host;
    cntlr->ops = &g_himciHostOps;
    cntlr->hdfDevObj = obj;
    obj->service = &cntlr->service;
    /* init cntlr. */
    ret = MmcCntlrParse(cntlr, obj);
    if (ret != HDF_SUCCESS) {
        goto _ERR;
    }
    /* init host. */
    ret = HimciHostParse(host, obj);
    if (ret != HDF_SUCCESS) {
        goto _ERR;
    }
    ret = HimciHostInit(host, cntlr);
    if (ret != HDF_SUCCESS) {
        goto _ERR;
    }
    ret = MmcCntlrAdd(cntlr);
    if (ret != HDF_SUCCESS) {
        goto _ERR;
    }

    /* add card detect msg to queue. */
    (void)MmcCntlrAddDetectMsgToQueue(cntlr);

    HDF_LOGD("HimciMmcBind: success.");
    return HDF_SUCCESS;
_ERR:
    HimciDeleteHost(host);
    HDF_LOGD("HimciMmcBind: fail, err = %d.", ret);
    return ret;
}

static int32_t HimciMmcInit(struct HdfDeviceObject *obj)
{
    static bool procInit = false;
    (void)obj;

    if (procInit == false) {
        if (ProcMciInit() == HDF_SUCCESS) {
            procInit = true;
            HDF_LOGD("HimciMmcInit: proc init success.");
        }
    }

    HDF_LOGD("HimciMmcInit: success.");
    return HDF_SUCCESS;
}

static void HimciMmcRelease(struct HdfDeviceObject *obj)
{
    struct MmcCntlr *cntlr = NULL;

    if (obj == NULL) {
        return;
    }

    cntlr = (struct MmcCntlr *)obj->service;
    if (cntlr == NULL) {
        return;
    }
    HimciDeleteHost((struct HimciHost *)cntlr->priv);
}

struct HdfDriverEntry g_mmcDriverEntry = {
    .moduleVersion = 1,
    .Bind = HimciMmcBind,
    .Init = HimciMmcInit,
    .Release = HimciMmcRelease,
    .moduleName = "hi3516_mmc_driver",
};
HDF_INIT(g_mmcDriverEntry);
