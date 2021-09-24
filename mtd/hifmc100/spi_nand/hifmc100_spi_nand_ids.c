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

#include "hifmc100_spi_nand.h"

#include "hdf_base.h"
#include "hdf_log.h"

#define HIFMC_WAIT_DEV_READY_TIMEOUT 0x10000
static int32_t HifmcCntlrSpinandWaitReadyDefault(struct SpiFlash *spi)
{
    struct HifmcCntlr *cntlr = NULL;
    unsigned char status;
    int32_t timeout = HIFMC_WAIT_DEV_READY_TIMEOUT;
    int32_t ret;

    if (spi == NULL || spi->mtd.cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = spi->mtd.cntlr;

    while (timeout >= 0) {
        ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_STATUS_ADDR, &status);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        if (!(status & MTD_SPI_SR_WIP_MASK)) {
            return HDF_SUCCESS;
        }
        timeout--;
    }

    return HDF_ERR_TIMEOUT;
}

static int32_t HifmcCntlrSpinandWriteEnableDefault(struct SpiFlash *spi)
{
    struct HifmcCntlr *cntlr = NULL;
    unsigned int reg;
    uint8_t status;
    int32_t ret;

    if (spi == NULL || spi->mtd.cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = spi->mtd.cntlr;

    ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_STATUS_ADDR, &status);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
#ifdef MTD_DEBUG
    HDF_LOGD("%s: read status before we addr[%#x]:0x%x", __func__, MTD_SPI_NAND_STATUS_ADDR, status); 
#endif
    if ((status & MTD_SPI_SR_WEL_MASK) != 0) {
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

    ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_STATUS_ADDR, &status);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
#ifdef MTD_DEBUG
    HDF_LOGD("%s: read status after we addr[%#x]:0x%x", __func__, MTD_SPI_NAND_STATUS_ADDR, status);
#endif

#ifdef MTD_DEBUG
    HDF_LOGD("%s: write enabled", __func__);
#endif
    return HDF_SUCCESS;
}

static int32_t HifmcCntlrSpinandQeEnableDefault(struct SpiFlash *spi)
{
    struct HifmcCntlr *cntlr = NULL;
    uint8_t feature;
    int32_t ret;
    int enable;

    if (spi == NULL || spi->mtd.cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = spi->mtd.cntlr;

    enable = ((spi->writeCfg.ifType >= MTD_SPI_IF_QUAD) ||
                (spi->readCfg.ifType >= MTD_SPI_IF_QUAD)) ? 1 : 0;
    ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_FEATURE_ADDR, &feature);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    if (!!(feature & MTD_SPI_FEATURE_QE_ENABLE) == enable) {
        HDF_LOGI("%s: qe feature:%d, qe enable:%d", __func__, feature, enable);
        return HDF_SUCCESS;
    }

    if (enable == 1) {
        feature |= MTD_SPI_FEATURE_QE_ENABLE;
    } else {
        feature &= ~MTD_SPI_FEATURE_QE_ENABLE;
    }
#ifdef MTD_DEBUG
    HDF_LOGD("%s: spi nand %s quad", __func__, (enable == 1) ? "enable" : "disable");
#endif
    ret = HifmcCntlrDevFeatureOp(cntlr, spi, false, MTD_SPI_NAND_FEATURE_ADDR, &feature);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    SpiFlashWaitReady(spi);
    ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_FEATURE_ADDR, &feature);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    if (!!(feature & MTD_SPI_FEATURE_QE_ENABLE) == enable) {
        HDF_LOGD("%s: spi nand %s quad success", __func__, (enable == 1) ? "enable" : "disable");
        return HDF_SUCCESS;
    } else {
        HDF_LOGE("%s: spi nand %s quad failed", __func__, (enable == 1) ? "enable" : "disable");
        return HDF_ERR_IO;
    }
}

static int32_t HifmcCntlrSpinandQeNotEnable(struct SpiFlash *spi)
{
    (void)spi;
    return HDF_SUCCESS;
}

static struct MtdSpiOps g_spiOpsDefault = {
    .waitReady = HifmcCntlrSpinandWaitReadyDefault,
    .writeEnable = HifmcCntlrSpinandWriteEnableDefault,
    .qeEnable = HifmcCntlrSpinandQeEnableDefault,
}; 

static struct SpiOpsInfo g_spiInfoTable[] = {
    {
        .id    = {0x2c, 0x14}, // MT29f2G01ABA 
        .idLen = 2,
        .spiOps = {
            .qeEnable = HifmcCntlrSpinandQeNotEnable,
        },
    },
    {
        .id    = {0x2c, 0x25}, // MT29f2G01ABB 
        .idLen = 2,
        .spiOps = {
            .qeEnable = HifmcCntlrSpinandQeNotEnable,
        },
    },
};

int32_t SpinandGetSpiOps(struct SpiFlash *spi)
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
        break;
    }
    return HDF_SUCCESS;
}
