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

#include "hifmc100.h"
#include "asm/platform.h"
#include "hisoc/flash.h"
#include "los_vm_phys.h"
#include "reset_shell.h"
#include "securec.h"

#include "device_resource_if.h"
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "mtd_core.h"
#include "mtd_spi_nor.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "osal_mutex.h"
#include "platform_core.h"

#include "hifmc100_spi_nand.h"
#include "hifmc100_spi_nor.h"

#define HDF_LOG_TAG hifmc100 

static struct HifmcCntlr *gHifmc100Cntlr = NULL;

int32_t HifmcCntlrReadSpiOp(struct MtdSpiConfig *cfg, const struct DeviceResourceNode *node)
{
    int32_t ret;
    struct DeviceResourceIface *drsOps = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL) {
        HDF_LOGE("%s: invalid drs ops", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    ret = drsOps->GetUint8(node, "if_type", &cfg->ifType, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read if type failed:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint8(node, "cmd", &cfg->cmd, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read cmd failed:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint8(node, "dummy", &cfg->dummy, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read dummy failed:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint32(node, "size", &cfg->size, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read size failed:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint32(node, "clock", &cfg->clock, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read clock failed:%d", __func__, ret);
        return ret;
    }
    hifmc100_get_best_clock(&cfg->clock);

    return HDF_SUCCESS;
}

const struct DeviceResourceNode *HifmcCntlrGetDevTableNode(struct HifmcCntlr *cntlr)
{
    struct DeviceResourceIface *drsOps = NULL;
    const struct DeviceResourceNode *tableNode = NULL;
    
    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL) {
        HDF_LOGE("%s: invalid drs ops", __func__);
        return NULL;
    }

    if (cntlr->flashType == HIFMC_CFG_TYPE_SPI_NOR) {
        tableNode = drsOps->GetChildNode(cntlr->drsNode, "spi_nor_dev_table");
    } else {
        tableNode = drsOps->GetChildNode(cntlr->drsNode, "spi_nand_dev_table");
    }

    if (tableNode == NULL) {
        HDF_LOGE("%s: dev table not found!", __func__);
    }
    return tableNode;
}

static int32_t HifmcCntlrSearchDevInfo(struct HifmcCntlr *cntlr, struct SpiFlash *spi)
{
    if (cntlr->flashType == HIFMC_CFG_TYPE_SPI_NOR) {
        return HifmcCntlrSearchSpinorInfo(cntlr, spi);
    } else {
        return HifmcCntlrSearchSpinandInfo(cntlr, spi);
    }
}

static int32_t HifmcCntlrReadDrs(struct HifmcCntlr *cntlr)
{
    int32_t ret;
    struct DeviceResourceIface *drsOps = NULL;
    const struct DeviceResourceNode *node = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL) {
        HDF_LOGE("%s: invalid drs ops", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    node = cntlr->drsNode;
    ret = drsOps->GetUint32(node, "regBase", &cntlr->regBasePhy, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read reg base fail:%d", __func__, ret);
        return ret;
    }
    ret = drsOps->GetUint32(node, "regSize", &cntlr->regSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read reg size fail:%d", __func__, ret);
        return ret;
    }
    ret = drsOps->GetUint32(node, "memBase", &cntlr->memBasePhy, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read mem base fail:%d", __func__, ret);
        return ret;
    }
    ret = drsOps->GetUint32(node, "memSize", &cntlr->memSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read mem size fail:%d", __func__, ret);
        return ret;
    }

    cntlr->regBase = OsalIoRemap(cntlr->regBasePhy, cntlr->regSize);
    if (cntlr->regBase == NULL) {
        HDF_LOGE("%s: remap reg base fail", __func__);
        return HDF_ERR_IO;
    }

    cntlr->memBase = OsalIoRemap(cntlr->memBasePhy, cntlr->memSize);
    if (cntlr->memBase == NULL) {
        HDF_LOGE("%s: remap mem base fail", __func__);
        OsalIoUnmap((void *)cntlr->regBase);
        return HDF_ERR_IO;
    }
    return HDF_SUCCESS;
}

static int32_t HifmcCntlrInitFlashType(struct HifmcCntlr *cntlr, unsigned int flashType)
{
    unsigned int reg;

    reg = HIFMC_REG_READ(cntlr, HIFMC_CFG_REG_OFF);
    HDF_LOGD("%s: before init reg = 0x%x, flashType = %u", __func__,
        reg, (reg & HIFMC_CFG_TYPE_MASK) >> HIFMC_CFG_TYPE_SHIFT);
   
    if (flashType != HIFMC_CFG_TYPE_SPI_NOR && flashType != HIFMC_CFG_TYPE_SPI_NAND) {
        HDF_LOGE("%s: invalid flash type:%u", __func__, flashType);
        return HDF_ERR_NOT_SUPPORT;
    }
    reg &= (~HIFMC_CFG_TYPE_MASK);
    reg |= HIFMC_CFG_TYPE_SEL(flashType);

    reg &= (~HIFMC_OP_MODE_MASK);
    reg |= HIFMC_CFG_OP_MODE(HIFMC_OP_MODE_NORMAL);

    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CFG_REG_OFF);
    cntlr->cfg = reg;
  
    // read back and check 
    reg = HIFMC_REG_READ(cntlr, HIFMC_CFG_REG_OFF);
    if ((reg & HIFMC_CFG_TYPE_MASK) != HIFMC_CFG_TYPE_SEL(flashType)) {
        HDF_LOGE("%s: flash select failed, reg = 0x%x", __func__, reg);
        return HDF_ERR_IO;
    }
    if ((reg & HIFMC_OP_MODE_MASK) != HIFMC_CFG_OP_MODE(HIFMC_OP_MODE_NORMAL)) {
        HDF_LOGE("%s: op mode set failed, reg = 0x%x", __func__, reg);
        return HDF_ERR_IO;
    }
    HDF_LOGD("%s: after init reg = 0x%x, flashType = %u", __func__,
        reg, (reg & HIFMC_CFG_TYPE_MASK) >> HIFMC_CFG_TYPE_SHIFT);

    return HDF_SUCCESS;
}

uint8_t HifmcCntlrReadDevReg(struct HifmcCntlr *cntlr, struct SpiFlash *spi, uint8_t cmd)
{
    if (cntlr->flashType == HIFMC_CFG_TYPE_SPI_NOR) {
        return HifmcCntlrReadSpinorReg(cntlr, spi, cmd);
    } else {
        return HifmcCntlrReadSpinandReg(cntlr, spi, cmd);
    }
}

void HifmcCntlrSet4AddrMode(struct HifmcCntlr *cntlr, int enable)
{
    unsigned int reg;

    reg = HIFMC_REG_READ(cntlr, HIFMC_CFG_REG_OFF);
    if (enable == 1) {
        reg |= HIFMC_SPI_NOR_ADDR_MODE_MASK;
    } else {
        reg &= ~HIFMC_SPI_NOR_ADDR_MODE_MASK;
    }

    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CFG_REG_OFF);
}

static int32_t HifmcCntlrInitBeforeScan(struct HifmcCntlr *cntlr)
{
    int32_t ret;
    unsigned int reg;
    unsigned int flashType;

    (void)OsalMutexInit(&cntlr->mutex);

    reg = HIFMC_REG_READ(cntlr, HIFMC_CFG_REG_OFF);
    flashType = (reg & HIFMC_CFG_TYPE_MASK) >> HIFMC_CFG_TYPE_SHIFT;
    ret = HifmcCntlrInitFlashType(cntlr, flashType);

    (void)HifmcCntlrSetSysClock(cntlr, 0, 1); // 0: default clock 1: enable

    if (flashType == HIFMC_CFG_TYPE_SPI_NAND) {
        // close global write protection
        reg = HIFMC_REG_READ(cntlr, HIFMC_GLOBAL_CFG_REG_OFF);
        reg &= ~HIFMC_GLOBAL_CFG_WP_ENABLE;
        HIFMC_REG_WRITE(cntlr, reg, HIFMC_GLOBAL_CFG_REG_OFF);
    }

    reg = HIFMC_SPI_TIMING_CFG_TCSH(HIFMC_SPI_CFG_CS_HOLD)
          | HIFMC_SPI_TIMING_CFG_TCSS(HIFMC_SPI_CFG_CS_SETUP)
          | HIFMC_SPI_TIMING_CFG_TSHSL(HIFMC_SPI_CFG_CS_DESELECT);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_SPI_TIMING_CFG_REG_OFF);

    reg = HIFMC_ALL_BURST_ENABLE;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DMA_AHB_CTRL_REG_OFF);

    cntlr->flashType = flashType;
    HDF_LOGI("%s: cntlr init done, flash type = %u", __func__, flashType);

    return ret;
}

static int32_t HifmcCntlrInitAfterScan(struct HifmcCntlr *cntlr)
{
    int32_t ret;

    if (cntlr->flashType == HIFMC_CFG_TYPE_SPI_NOR) {
        cntlr->buffer = OsalMemAllocAlign(HIFMC_DMA_ALIGN_SIZE,
            ALIGN(HIFMC_DMA_ALIGN_SIZE, HIFMC_DMA_ALIGN_SIZE));
        if (cntlr->buffer == NULL) {
            HDF_LOGE("%s: alloc buffer fail", __func__);
            return HDF_ERR_MALLOC_FAIL;
        }
        cntlr->bufferSize = ALIGN(HIFMC_DMA_ALIGN_SIZE, HIFMC_DMA_ALIGN_SIZE);
    } else {
        ret = HifmcCntlrInitOob(cntlr, cntlr->curDev);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        
        cntlr->buffer = OsalMemAllocAlign(HIFMC_DMA_ALIGN_SIZE,
            ALIGN(cntlr->pageSize + cntlr->oobSize, HIFMC_DMA_ALIGN_SIZE));
        if (cntlr->buffer == NULL) {
            HDF_LOGE("%s: alloc buffer fail", __func__);
            return HDF_ERR_MALLOC_FAIL;
        }
        cntlr->bufferSize = ALIGN(cntlr->pageSize + cntlr->oobSize, HIFMC_DMA_ALIGN_SIZE);
    }
    cntlr->dmaBuffer = cntlr->buffer;
    HDF_LOGD("%s: cntlr flashType = %u", __func__, cntlr->flashType);
    HDF_LOGD("%s: cntlr regBase=%p(phyBase:0x%x)", __func__, cntlr->regBase, cntlr->regBasePhy);
    HDF_LOGD("%s: cntlr memBase=%p)", __func__, cntlr->memBase);
    HDF_LOGD("%s: cntlr buffer=%p, dmaBuffer=0x%p)", __func__, cntlr->buffer, cntlr->dmaBuffer);
    HDF_LOGD("%s: cntlr cfg=0x%x)", __func__, cntlr->cfg);
    HDF_LOGD("%s: cntlr pageSize=%zu)", __func__, cntlr->pageSize);
    HDF_LOGD("%s: cntlr oobSize=%zu)", __func__, cntlr->oobSize);
    HDF_LOGD("%s: cntlr eccType=%d)", __func__, cntlr->eccType);
    return HDF_SUCCESS;
}

static void HifmcCntlrUninit(struct HifmcCntlr *cntlr)
{
    (void)OsalMutexDestroy(&cntlr->mutex);
}

static int32_t HifmcCntlrReadId(struct HifmcCntlr *cntlr, uint8_t cs, uint8_t *id, size_t len)
{
    if (cntlr->flashType == HIFMC_CFG_TYPE_SPI_NOR) {
        return HifmcCntlrReadIdSpiNor(cntlr, cs, id, len);
    } else {
        return HifmcCntlrReadIdSpiNand(cntlr, cs, id, len);
    }
}

static int32_t HifmcCntlrInitSpiFlash(struct HifmcCntlr *cntlr, struct SpiFlash *spi)
{
    if (cntlr->flashType == HIFMC_CFG_TYPE_SPI_NOR) {
        return HifmcCntlrInitSpinorDevice(cntlr, spi);
    } else {
        return HifmcCntlrInitSpinandDevice(cntlr, spi);
    }
}

static int32_t HifmcCntlrMtdScan(struct HifmcCntlr *cntlr)
{
    int32_t ret;
    struct SpiFlash *spi = NULL;

    spi = (struct SpiFlash *)OsalMemCalloc(sizeof(*spi));
    if (spi == NULL) {
        HDF_LOGE("%s: alloc spi flash fail", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }
    spi->cs = 0; // default cs num
    spi->mtd.cntlr = cntlr;

    // read id
    ret = HifmcCntlrReadId(cntlr, spi->cs, spi->mtd.id, sizeof(spi->mtd.id));
    if (ret != HDF_SUCCESS) {
        OsalMemFree(spi);
        return ret;
    }

    // search flash info from ids table or read from hcs
    ret = HifmcCntlrSearchDevInfo(cntlr, spi);
    if (ret != HDF_SUCCESS) {
        OsalMemFree(spi);
        return ret;
    }

    // init spi flash device 
    ret = HifmcCntlrInitSpiFlash(cntlr, spi);
    if (ret != HDF_SUCCESS) {
        OsalMemFree(spi);
        return ret;
    }
    cntlr->curDev = spi;

    return HDF_SUCCESS;
}

void HifmcCntlrShutDown(void)
{
    if (gHifmc100Cntlr != NULL) {
        (void)OsalMutexLock(&gHifmc100Cntlr->mutex);
    }
} 

static int32_t HifmcInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct HifmcCntlr *cntlr = NULL;

    HDF_LOGI("%s: Enter", __func__);
    if (device == NULL || device->property == NULL) {
        HDF_LOGE("%s: device or property is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    cntlr = (struct HifmcCntlr *)OsalMemCalloc(sizeof(*cntlr));
    if (cntlr == NULL) {
        HDF_LOGE("%s: alloc controller fail", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }
    cntlr->drsNode = device->property;

    ret = HifmcCntlrReadDrs(cntlr);
    if (ret != HDF_SUCCESS) {
        goto __OUT;
    }

    ret = HifmcCntlrInitBeforeScan(cntlr);
    if (ret != HDF_SUCCESS) {
        goto __OUT;
    }

    ret = HifmcCntlrMtdScan(cntlr);
    if (ret != HDF_SUCCESS) {
        goto __OUT;
    }

    ret = HifmcCntlrInitAfterScan(cntlr);
    if (ret != HDF_SUCCESS) {
        goto __OUT;
    }

    ret = SpiFlashAdd(cntlr->curDev);
    if (ret != HDF_SUCCESS) {
        goto __OUT;
    }

    gHifmc100Cntlr = cntlr;

#ifdef LOSCFG_SHELL
    (void)osReHookFuncAdd((STORAGE_HOOK_FUNC)HifmcCntlrShutDown, NULL);
#endif

    return HDF_SUCCESS;

__OUT:
    if (cntlr->curDev != NULL) {
        OsalMemFree(cntlr->curDev);
    }
    OsalMemFree(cntlr);
    HDF_LOGI("%s: Exit, ret=%d", __func__, ret);
    return ret;
}

static void HifmcRelease(struct HdfDeviceObject *device)
{
    HDF_LOGI("%s: enter", __func__);

    (void)device;
    if (gHifmc100Cntlr == NULL) {
        return;
    }

    if (gHifmc100Cntlr->curDev != NULL) {
        SpiFlashDel(gHifmc100Cntlr->curDev);
        OsalMemFree(gHifmc100Cntlr->curDev);
    }

    HifmcCntlrUninit(gHifmc100Cntlr);
    OsalMemFree(gHifmc100Cntlr);
    gHifmc100Cntlr = NULL;
}

struct HdfDriverEntry g_hifmc100DriverEntry = {
    .moduleVersion = 1,
    .Init = HifmcInit,
    .Release = HifmcRelease,
    .moduleName = "hifmc100_driver",
};
HDF_INIT(g_hifmc100DriverEntry);
