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

#include "hdf_log.h"
#include "hifmc100.h"
#include "mtd_core.h"
#include "mtd_spi_nor.h"

#define HDF_LOG_TAG mx25l_c

int32_t HifmcCntlrSpinorQeEnableMx25l(struct SpiFlash *spi)
{
    uint8_t status;
    unsigned long reg;
    int enable;
    struct HifmcCntlr *cntlr = NULL;

    if (spi == NULL || spi->mtd.cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = spi->mtd.cntlr;

    enable = ((spi->writeCfg.ifType >= MTD_SPI_IF_QUAD) ||
             (spi->readCfg.ifType >= MTD_SPI_IF_QUAD)) ? 1 : 0;

    status = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR);
    if ((!!(status & MTD_SPI_SR_QE_MASK)) == enable) {
        HDF_LOGI("%s: qe status:%d, qe enable:%d", __func__, status, enable);
        return HDF_SUCCESS;
    }

    SpiFlashWriteEnable(spi);

    if (enable == 1) {
        status |= MTD_SPI_SR_QE_MASK;
    } else {
        status &= ~(MTD_SPI_SR_QE_MASK);
    }
    OSAL_WRITEB(status, cntlr->memBase);

    reg = HIFMC_CMD_CMD1(MTD_SPI_CMD_WRSR);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(spi->cs);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_DATA_NUM_CNT(1);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DATA_NUM_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) |
          HIFMC_OP_WRITE_DATA_EN(1) |
          HIFMC_OP_REG_OP_START;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    SpiFlashWaitReady(spi);

    status = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR);
    if ((!!(status & MTD_SPI_SR_QE_MASK)) == enable) {
        HDF_LOGI("%s: qe enable:%d set success", __func__, enable);
        return HDF_SUCCESS;
    } else {
        HDF_LOGE("%s: qe enable:%d set failed", __func__, enable);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}
