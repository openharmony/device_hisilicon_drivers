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

#include "hdf_base.h"
#include "hdf_log.h"
#include "hifmc100.h"
#include "mtd_spi_nor.h"

#define HIFMC_WAIT_DEV_READY_TIMEOUT 0xf0000000
static int32_t HifmcCntlrSpinorWaitReadyDefault(struct SpiFlash *spi)
{
    struct HifmcCntlr *cntlr = NULL;
    unsigned char status;
    uint32_t timeout = HIFMC_WAIT_DEV_READY_TIMEOUT;

    if (spi == NULL || spi->mtd.cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = spi->mtd.cntlr;

    while (timeout > 0) {
        status = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR);
        if (!(status & MTD_SPI_SR_WIP_MASK)) {
            return HDF_SUCCESS;
        }
        timeout--;
    }

    return HDF_ERR_TIMEOUT;
}

static int32_t HifmcCntlrSpinorWriteEnableDefault(struct SpiFlash *spi)
{
    struct HifmcCntlr *cntlr = NULL;
    unsigned long reg;

    if (spi == NULL || spi->mtd.cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = spi->mtd.cntlr;

    reg = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR);
#ifdef MTD_DEBUG
    HDF_LOGD("%s: enter, read status register[%#x]:%#lx", __func__, MTD_SPI_CMD_RDSR, reg);
#endif

    if ((reg & MTD_SPI_SR_WEL_MASK) != 0) {
        HDF_LOGD("%s: write enable already set", __func__);
        return HDF_SUCCESS;
    }

    reg = HIFMC_REG_READ(cntlr, HIFMC_GLOBAL_CFG_REG_OFF);
    if ((reg & HIFMC_GLOBAL_CFG_WP_ENABLE) != 0) {
        reg &= ~HIFMC_GLOBAL_CFG_WP_ENABLE;
        HIFMC_REG_WRITE(cntlr, reg, HIFMC_GLOBAL_CFG_REG_OFF);
    }

    reg = HIFMC_CMD_CMD1(MTD_SPI_CMD_WREN);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(spi->cs);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) | HIFMC_OP_REG_OP_START;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

    reg = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR);
#ifdef MTD_DEBUG
    HDF_LOGD("%s: exit, read status register[%#x]:%#lx", __func__, MTD_SPI_CMD_RDSR, reg);
#endif
    return HDF_SUCCESS;
}

static int32_t HifmcCntlrSpinorQeEnableDefault(struct SpiFlash *spi)
{
    struct HifmcCntlr *cntlr = NULL;
    uint8_t status;
    uint8_t config;
    unsigned long reg;
    int enable;

    if (spi == NULL || spi->mtd.cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = spi->mtd.cntlr;

    enable = ((spi->writeCfg.ifType >= MTD_SPI_IF_QUAD) ||
                (spi->readCfg.ifType >= MTD_SPI_IF_QUAD)) ? 1 : 0;
    config = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDCR);
    if ((!!(config & MTD_SPI_CR_QE_MASK)) == enable) {
        HDF_LOGI("%s: qe config:%d, qe enable:%d", __func__, config, enable);
        return HDF_SUCCESS;
    }

    status = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR);

    SpiFlashWriteEnable(spi);

    if (enable == 1) {
        config |= MTD_SPI_CR_QE_MASK;
    } else {
        config &= ~(MTD_SPI_CR_QE_MASK);
    }
    OSAL_WRITEB(status, cntlr->memBase);
    OSAL_WRITEB(config, cntlr->memBase + 1);

    reg = HIFMC_CMD_CMD1(MTD_SPI_CMD_WRSR);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(spi->cs);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_DATA_NUM_CNT(0x2);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DATA_NUM_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) |
          HIFMC_OP_WRITE_DATA_EN(1) |
          HIFMC_OP_REG_OP_START;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    SpiFlashWaitReady(spi);

    config = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDCR);
    if ((!!(config & MTD_SPI_CR_QE_MASK)) == enable) {
        HDF_LOGI("%s: qe enable:%d set success", __func__, enable);
        return HDF_SUCCESS;
    } else {
        HDF_LOGE("%s: qe enable:%d set failed", __func__, enable);
        return HDF_FAILURE; 
    }
    return HDF_SUCCESS;
}

static int32_t HifmcCntlrSpinorQeNone(struct SpiFlash *spi)
{
    if (spi != NULL) {
        HDF_LOGI("%s: flash chip:%s not support qe", __func__, spi->mtd.chipName);
    }
    return HDF_SUCCESS;
}

static int32_t HifmcCntlrSpinorEntry4AddrDefault(struct SpiFlash *spi, int enable)
{
    struct HifmcCntlr *cntlr = NULL;
    unsigned long reg;
    uint8_t status;
    const char *str[] = {"disable", "enable"};

    enable = !!enable; // make it 0 or 1
    HDF_LOGD("%s: start spinor flash 4-byte mode %s", __func__, str[enable]);

    if (spi == NULL || spi->mtd.cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = spi->mtd.cntlr;

    if (spi->addrCycle != MTD_SPI_ADDR_4BYTE) {
        HDF_LOGD("%s: 4byte addr not support", __func__);
        return HDF_SUCCESS;
    }

    status = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR3);
    HDF_LOGD("%s: read status register 3[%#x]:%#x", __func__, MTD_SPI_CMD_RDSR3, status);

    if (MTD_SPI_SR3_IS_4BYTE(status) == enable) {
        HDF_LOGD("%s: 4byte status:%#x, enable:%d", __func__, status, enable);
        return HDF_SUCCESS;
    }

    reg = (enable == 1) ? MTD_SPI_CMD_EN4B : MTD_SPI_CMD_EX4B;
    reg = HIFMC_CMD_CMD1(reg);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(spi->cs);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) | HIFMC_OP_REG_OP_START;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

    HifmcCntlrSet4AddrMode(cntlr, enable);
    HDF_LOGD("%s: end spinor flash 4-byte mode %s", __func__, str[enable]);
    return HDF_SUCCESS;
}

extern int32_t HifmcCntlrSpinorQeEnableMx25l(struct SpiFlash *spi);
extern int32_t HifmcCntlrSpinorQeEnableW25qh(struct SpiFlash *spi);
extern int32_t HifmcCntlrSpinorEntry4AddrW25qh(struct SpiFlash *spi, int enable);

static struct MtdSpiOps g_spiOpsDefault = {
    .waitReady = HifmcCntlrSpinorWaitReadyDefault,
    .writeEnable = HifmcCntlrSpinorWriteEnableDefault,
    .qeEnable = HifmcCntlrSpinorQeEnableDefault,
    .entry4Addr = HifmcCntlrSpinorEntry4AddrDefault,
}; 

static struct SpiOpsInfo g_spiInfoTable[] = {
    {
        .id    = {0xef, 0x60, 0x16}, // w25q32fw 
        .idLen = 0x3,
        .spiOps = {
            .qeEnable = HifmcCntlrSpinorQeEnableW25qh,
            .entry4Addr = HifmcCntlrSpinorEntry4AddrW25qh,
        },
    },
    {
        .id    = {0xc2, 0x20, 0x18}, // mx25l18 
        .idLen = 0x3,
        .spiOps = {
            .qeEnable = HifmcCntlrSpinorQeEnableMx25l,
        },
    },
    {
        .id    = {0x20, 0x70, 0x18}, // xm25qh128a
        .idLen = 0x3,
        .spiOps = {
            .qeEnable = HifmcCntlrSpinorQeNone,
        },
    },
};

int32_t SpinorGetSpiOps(struct SpiFlash *spi)
{
    size_t i;

    if (spi == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    spi->spiOps = g_spiOpsDefault;

    for (i = 0; i < (sizeof(g_spiInfoTable) / sizeof(g_spiInfoTable[0])); i++) {
        if (memcmp(spi->mtd.id, g_spiInfoTable[i].id, g_spiInfoTable[i].idLen) != 0) {
            continue;
        }
        if (g_spiInfoTable[i].spiOps.waitReady != NULL) {
            spi->spiOps.waitReady = g_spiInfoTable[i].spiOps.waitReady;
        }
        if (g_spiInfoTable[i].spiOps.writeEnable != NULL) {
            spi->spiOps.writeEnable = g_spiInfoTable[i].spiOps.writeEnable;
        }
        if (g_spiInfoTable[i].spiOps.qeEnable != NULL) {
            spi->spiOps.qeEnable = g_spiInfoTable[i].spiOps.qeEnable;
        }
        if (g_spiInfoTable[i].spiOps.entry4Addr != NULL) {
            spi->spiOps.entry4Addr = g_spiInfoTable[i].spiOps.entry4Addr;
        }
        break;
    }
    return HDF_SUCCESS;
}
