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

#define HDF_LOG_TAG w25qh_c

#define MTD_SPI_CMD_FIRST_RESET_4ADDR 0x66
#define MTD_SPI_CMD_SECOND_RESET_4ADDR 0x99
int32_t HifmcCntlrSpinorEntry4AddrW25qh(struct SpiFlash *spi, int enable)
{
    uint8_t status;
    unsigned long reg;
    struct HifmcCntlr *cntlr = NULL;

    if (spi == NULL || spi->mtd.cntlr == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = spi->mtd.cntlr;

    enable = !!enable; // make it 0 or 1
    HDF_LOGD("%s: start spinor flash 4-byte mode %s", __func__, (enable == 1) ? "enable" : "disbale");

    if (spi->addrCycle != MTD_SPI_ADDR_4BYTE) {
        HDF_LOGD("%s: 4byte addr noty support", __func__);
        return HDF_SUCCESS;
    }

    status = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR3);
    HDF_LOGD("%s: read status register 3[%#x]:%#x", __func__, MTD_SPI_CMD_RDSR3, status);
    if ((status & 0x1) == enable) {
        HDF_LOGD("%s: 4byte status:%#x, enable:%d", __func__, status, enable);
        return HDF_SUCCESS;
    }

    reg = (enable == 1) ? MTD_SPI_CMD_EN4B : MTD_SPI_CMD_FIRST_RESET_4ADDR;
    reg = HIFMC_CMD_CMD1(reg);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(spi->cs);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) | HIFMC_OP_REG_OP_START;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

    if (enable == 1) {
        status = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR3);
        HDF_LOGD("%s: read status register 3[%#x]:%#x", __func__, MTD_SPI_CMD_RDSR3, status);
        if ((status & 0x1) == enable) {
            HDF_LOGD("%s: enable 4byte success, status:%#x", __func__, status);
            return HDF_SUCCESS;
        } else {
            HDF_LOGE("%s: enable 4byte failed, status:%#x", __func__, status);
            return HDF_ERR_IO;
        }
    } else {
        reg = HIFMC_CMD_CMD1(MTD_SPI_CMD_SECOND_RESET_4ADDR);
        HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

        reg = HIFMC_OP_CFG_FM_CS(spi->cs);
        HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

        reg = HIFMC_OP_CMD1_EN(1) | HIFMC_OP_REG_OP_START;
        HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

        HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

        status = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR3);
        HDF_LOGE("%s: disable 4byte done, status:%#x", __func__, status);
    }

    HifmcCntlrSet4AddrMode(cntlr, enable);
    HDF_LOGD("%s: end spinor flash 4-byte mode %s", __func__, (enable == 1) ? "enable" : "disbale");
    return HDF_SUCCESS;
}

int32_t HifmcCntlrSpinorQeEnableW25qh(struct SpiFlash *spi)
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

    status = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR2);
    if ((!!(status & MTD_SPI_SR_QE_MASK)) == enable) {
        HDF_LOGI("%s: qe status:%d, qe enable:%d", __func__, status, enable);
        return HDF_SUCCESS;
    }

    SpiFlashWriteEnable(spi);

    if (enable == 1) {
        status |= MTD_SPI_CR_QE_MASK;
    } else {
        status &= ~(MTD_SPI_CR_QE_MASK);
    }
    OSAL_WRITEB(status, cntlr->memBase);

    reg = HIFMC_CMD_CMD1(MTD_SPI_CMD_WRSR2);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(spi->cs);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_DATA_NUM_CNT(1);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DATA_NUM_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) |
          HIFMC_OP_WRITE_DATA_EN(1) |
          HIFMC_OP_REG_OP_START;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

    status = HifmcCntlrReadDevReg(cntlr, spi, MTD_SPI_CMD_RDSR2);
    if ((!!(status & MTD_SPI_SR_QE_MASK)) != enable) {
        HDF_LOGI("%s: failed, qe status:%d, qe enable:%d", __func__, status, enable);
        return HDF_ERR_IO;
    }

    return HDF_SUCCESS;
}
