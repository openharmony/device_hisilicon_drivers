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

#include "asm/platform.h"
#include "hisoc/flash.h"
#include "los_vm_phys.h"
#include "securec.h"

#include "device_resource_if.h"
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "mtd_core.h"
#include "mtd_spi_nand.h"
#include "mtd_spi_nor.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "osal_mutex.h"
#include "platform_core.h"

#define SPI_NAND_ADDR_REG_SHIFT 16

static int32_t HifmcCntlrReadSpinandInfo(struct SpinandInfo *info, const struct DeviceResourceNode *node)
{
    int32_t ret;
    struct DeviceResourceIface *drsOps = NULL;
    
    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL) {
        HDF_LOGE("%s: invalid drs ops", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    ret = drsOps->GetString(node, "name", &info->name, "unkown");
    if (ret != HDF_SUCCESS || info->name == NULL) {
        HDF_LOGE("%s: read reg base fail:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetElemNum(node, "id");
    if (ret <= 0 || ret > MTD_FLASH_ID_LEN_MAX) {
        HDF_LOGE("%s: get id len failed:%d", __func__, ret);
        return ret;
    }
    info->idLen = ret;

    ret = drsOps->GetUint8Array(node, "id", info->id, info->idLen, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read reg base fail:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint32(node, "chip_size", &info->chipSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read block size failed:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint32(node, "block_size", &info->blockSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read block size failed:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint32(node, "page_size", &info->pageSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read page size failed:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint32(node, "oob_size", &info->oobSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGI("%s: no oob size found", __func__);
        info->oobSize = 0;
    }

    if ((ret = HifmcCntlrReadSpiOp(&info->eraseCfg, drsOps->GetChildNode(node, "erase_op"))) != HDF_SUCCESS ||
        (ret = HifmcCntlrReadSpiOp(&info->writeCfg, drsOps->GetChildNode(node, "write_op"))) != HDF_SUCCESS ||
        (ret = HifmcCntlrReadSpiOp(&info->readCfg, drsOps->GetChildNode(node, "read_op"))) != HDF_SUCCESS) {
        return ret;
    }

    return HDF_SUCCESS;
}

static void HifmcCntlrFillSpiNand(struct SpiFlash *spi, struct SpinandInfo *spinandInfo)
{
    spi->mtd.chipName = spinandInfo->name;
    spi->mtd.idLen = spinandInfo->idLen;
    spi->mtd.capacity = spinandInfo->chipSize;
    spi->mtd.eraseSize = spinandInfo->blockSize;
    spi->mtd.writeSize = spinandInfo->pageSize;
    spi->mtd.writeSizeShift = MtdFfs(spinandInfo->pageSize) - 1;
    spi->mtd.readSize = spinandInfo->pageSize;
    spi->mtd.oobSize = spinandInfo->oobSize;
    spi->eraseCfg = spinandInfo->eraseCfg;
    spi->writeCfg = spinandInfo->writeCfg;
    spi->readCfg = spinandInfo->readCfg;
}

int32_t HifmcCntlrSearchSpinandInfo(struct HifmcCntlr *cntlr, struct SpiFlash *spi)
{
    unsigned int i;
    int32_t ret;
    struct SpinandInfo spinandInfo;
    const struct DeviceResourceNode *childNode = NULL;
    const struct DeviceResourceNode *tableNode = NULL;

    tableNode = HifmcCntlrGetDevTableNode(cntlr);
    if (tableNode == NULL) {
        return HDF_ERR_NOT_SUPPORT;
    }

    DEV_RES_NODE_FOR_EACH_CHILD_NODE(tableNode, childNode) {
        ret = HifmcCntlrReadSpinandInfo(&spinandInfo, childNode);
        if (ret != HDF_SUCCESS) {
            return HDF_ERR_IO;
        }
        if (memcmp(spinandInfo.id, spi->mtd.id, spinandInfo.idLen) == 0) {
            HifmcCntlrFillSpiNand(spi, &spinandInfo);
            return HDF_SUCCESS;
        }
    }

    HDF_LOGE("%s: dev id not support", __func__);
    for (i = 0; i < sizeof(spi->mtd.id); i++) {
        HDF_LOGE("%s: mtd->id[%i] = 0x%x", __func__, i, spi->mtd.id[i]);
    }
    return HDF_ERR_NOT_SUPPORT;
}

uint8_t HifmcCntlrReadSpinandReg(struct HifmcCntlr *cntlr, struct SpiFlash *spi, uint8_t cmd)
{
    uint8_t status;
    unsigned int reg;

#ifdef MTD_DEBUG
    HDF_LOGD("%s: start read spi register:%#x", __func__, cmd);
#endif
    reg = HIFMC_OP_CFG_FM_CS(spi->cs);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    if (cmd == MTD_SPI_CMD_RDSR) {
        reg = HIFMC_OP_READ_STATUS_EN(1) | HIFMC_OP_REG_OP_START;
        goto __CMD_CFG_DONE;
    }

    reg = cmd;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = HIFMC_DATA_NUM_CNT(1);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DATA_NUM_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) |
          HIFMC_OP_READ_DATA_EN(1) |
          HIFMC_OP_REG_OP_START;

__CMD_CFG_DONE:
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

    if (cmd == MTD_SPI_CMD_RDSR) {
        status = HIFMC_REG_READ(cntlr, HIFMC_FLASH_INFO_REG_OFF);
    } else {
        status = OSAL_READB(cntlr->memBase);
    }
#ifdef MTD_DEBUG
    HDF_LOGD("%s: end read spi register:%#x, val:%#x", __func__, cmd, status);
#endif

    return status;
}

static void HifmcCntlrEcc0Switch(struct HifmcCntlr *cntlr, int enable)
{
    unsigned int config;

    if (enable == 0) {
        config = cntlr->cfg;
    } else {
        config = cntlr->cfg & (~HIFMC_ECC_TYPE_MASK);
    }
    HIFMC_REG_WRITE(cntlr, config, HIFMC_CFG_REG_OFF);
}

int32_t HifmcCntlrDevFeatureOp(struct HifmcCntlr *cntlr, struct SpiFlash *spi,
    bool isGet, uint8_t addr, uint8_t *val)
{
    unsigned int reg;

    if (isGet && (addr == MTD_SPI_NAND_STATUS_ADDR)) {
#ifdef MTD_DEBUG
        HDF_LOGD("%s: start get status", __func__);
#endif
        reg = HIFMC_OP_CFG_FM_CS(spi->cs);
        HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

        reg = HIFMC_OP_READ_STATUS_EN(1) | HIFMC_OP_REG_OP_START;
        HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

        HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

        *val = HIFMC_REG_READ(cntlr, HIFMC_FLASH_INFO_REG_OFF);        
#ifdef MTD_DEBUG
        HDF_LOGD("%s: end get status:%#x", __func__, *val);
#endif
        return HDF_SUCCESS;
    }

#ifdef MTD_DEBUG
    HDF_LOGD("%s: start %s feature(addr:0x%x)", __func__, isGet ? "get" : "set", addr);
#endif
    HifmcCntlrEcc0Switch(cntlr, 1);

    reg = HIFMC_CMD_CMD1(isGet ? MTD_SPI_CMD_GET_FEATURE : MTD_SPI_CMD_SET_FEATURE);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    HIFMC_REG_WRITE(cntlr, addr, HIFMC_ADDRL_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(spi->cs) | HIFMC_OP_CFG_ADDR_NUM(1);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_DATA_NUM_CNT(1);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DATA_NUM_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) | HIFMC_OP_ADDR_EN(1) | HIFMC_OP_REG_OP_START;

    if (!isGet) {
        reg |= HIFMC_OP_WRITE_DATA_EN(1);
        OSAL_WRITEB(*val, cntlr->memBase);
    } else {
        reg |= HIFMC_OP_READ_DATA_EN(1);
    }
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

    if (isGet) {
        *val = OSAL_READB(cntlr->memBase);
    }
    HifmcCntlrEcc0Switch(cntlr, 0);
#ifdef MTD_DEBUG
    HDF_LOGD("%s: end %s feature:%#x(addr:0x%x)", __func__, isGet ? "get" : "set", *val, addr);
#endif
    return HDF_SUCCESS;
}

int32_t HifmcCntlrReadIdSpiNand(struct HifmcCntlr *cntlr, uint8_t cs, uint8_t *id, size_t len)
{
    int32_t ret;
    unsigned int reg;

    if (len > MTD_FLASH_ID_LEN_MAX) {
        HDF_LOGE("%s: buf not enough(len: %u, expected %u)", __func__, len, MTD_FLASH_ID_LEN_MAX);
        return HDF_ERR_INVALID_PARAM;
    }

    HifmcCntlrEcc0Switch(cntlr, 1);

    reg = HIFMC_CMD_CMD1(MTD_SPI_CMD_RDID);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = MTD_SPI_NAND_RDID_ADDR;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_ADDRL_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(cs) | 
          HIFMC_OP_CFG_ADDR_NUM(1);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_DATA_NUM_CNT(MTD_FLASH_ID_LEN_MAX);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DATA_NUM_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) |
          HIFMC_OP_ADDR_EN(1) |
          HIFMC_OP_READ_DATA_EN(1) |
          HIFMC_OP_REG_OP_START;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

    ret = memcpy_s(id, MTD_FLASH_ID_LEN_MAX, (const void *)cntlr->memBase, MTD_FLASH_ID_LEN_MAX);
    if (ret != EOK) {
        HDF_LOGE("%s: copy id buf failed : %d", __func__, ret);
        return HDF_PLT_ERR_OS_API;
    }
    HifmcCntlrEcc0Switch(cntlr, 0);
    return HDF_SUCCESS;
}

static int32_t HifmcCntlrEraseOneBlock(struct HifmcCntlr *cntlr, struct SpiFlash *spi, off_t addr)
{
    int32_t ret;
    uint8_t status;
    unsigned int reg;

    ret = SpiFlashWaitReady(spi);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = SpiFlashWriteEnable(spi);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    // set system clock for erase 
    ret = HifmcCntlrSetSysClock(cntlr, spi->eraseCfg.clock, 1);

    reg = HIFMC_INT_CLR_ALL;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_INT_CLR_REG_OFF);

    reg = HIFMC_CMD_CMD1(spi->eraseCfg.cmd);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = addr >> spi->mtd.writeSizeShift;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_ADDRL_REG_OFF);

    reg =  HIFMC_OP_CFG_FM_CS(spi->cs) |
           HIFMC_OP_CFG_MEM_IF_TYPE(spi->eraseCfg.ifType) |
           HIFMC_OP_CFG_ADDR_NUM(MTD_SPI_STD_OP_ADDR_NUM) |
           HIFMC_OP_CFG_DUMMY_NUM(spi->eraseCfg.dummy);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) |
          HIFMC_OP_ADDR_EN(1) |
          HIFMC_OP_REG_OP_START;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

    ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_STATUS_ADDR, &status);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    if (status & MTD_SPI_NAND_ERASE_FAIL) {
        HDF_LOGE("%s: erase fail, status = 0x%0x", __func__, status);
        return HDF_ERR_IO;
    }

    return HDF_SUCCESS;
}

static inline uint16_t HifmcCntlrReadBuf(struct HifmcCntlr *cntlr, uint8_t *buf, size_t len, off_t offset)
{
    int32_t ret;

    if (len == 0) {
        return HDF_ERR_INVALID_PARAM;
    }
    ret = memcpy_s((void *)buf, len, (void *)(cntlr->buffer + offset), len);
    if (ret != 0) {
        HDF_LOGE("%s: memcpy failed, ret = %d", __func__, ret);
        return HDF_PLT_ERR_OS_API;
    }
    return HDF_SUCCESS;
}

struct HifmcEccInfo {
    unsigned int pageSize;
    unsigned int eccType;
    unsigned int oobSize;
};

static struct HifmcEccInfo g_hifmcEccInfoTable[] = {
    {MTD_NAND_PAGE_SIZE_4K, MTD_NAND_ECC_24BIT_1K,  200},
    {MTD_NAND_PAGE_SIZE_4K, MTD_NAND_ECC_16BIT_1K,  144},
    {MTD_NAND_PAGE_SIZE_4K, MTD_NAND_ECC_8BIT_1K,   88},
    {MTD_NAND_PAGE_SIZE_4K, MTD_NAND_ECC_0BIT,      32},
    {MTD_NAND_PAGE_SIZE_2K, MTD_NAND_ECC_24BIT_1K,  128},
    {MTD_NAND_PAGE_SIZE_2K, MTD_NAND_ECC_16BIT_1K,  88},
    {MTD_NAND_PAGE_SIZE_2K, MTD_NAND_ECC_8BIT_1K,   64},
    {MTD_NAND_PAGE_SIZE_2K, MTD_NAND_ECC_0BIT,      32},
    {0, 0, 0},
};

static struct HifmcEccInfo *HifmcCntlrGetEccInfo(struct SpiFlash *spi)
{
    struct HifmcEccInfo *best = NULL;
    struct HifmcEccInfo *tab = g_hifmcEccInfoTable;

    for (; tab->pageSize != 0; tab++) {
        if (tab->pageSize != spi->mtd.writeSize) {
            continue;
        }
        if (tab->oobSize > spi->mtd.oobSize) {
            continue;
        }
        if (best == NULL || best->eccType < tab->eccType) {
            best = tab;
        }
    }
    if (best == NULL) {
        HDF_LOGW("%s: Not support pagesize:%zu, oobsize:%zu",
            __func__, spi->mtd.writeSize, spi->mtd.oobSize);
    }
    return best;
}

static int HifmcGetPageSizeConfig(size_t pageSize)
{
    switch (pageSize) {
        case MTD_NAND_PAGE_SIZE_2K:
            return HIFMC_PAGE_SIZE_2K;
        case MTD_NAND_PAGE_SIZE_4K:
            return HIFMC_PAGE_SIZE_4K;
        case MTD_NAND_PAGE_SIZE_8K:
            return HIFMC_PAGE_SIZE_8K;
        case MTD_NAND_PAGE_SIZE_16K:
            return HIFMC_PAGE_SIZE_16K;
        default:
            return HDF_ERR_NOT_SUPPORT;
    }
}

static int HifmcGetEccTypeConfig(unsigned int eccType)
{
    static unsigned int eccCfgTab[] = {
        [MTD_NAND_ECC_0BIT] = HIFMC_ECC_0BIT,    
        [MTD_NAND_ECC_8BIT_1K] = HIFMC_ECC_8BIT,    
        [MTD_NAND_ECC_16BIT_1K] = HIFMC_ECC_16BIT,    
        [MTD_NAND_ECC_24BIT_1K] = HIFMC_ECC_24BIT,    
        [MTD_NAND_ECC_28BIT_1K] = HIFMC_ECC_28BIT,    
        [MTD_NAND_ECC_40BIT_1K] = HIFMC_ECC_40BIT,    
        [MTD_NAND_ECC_64BIT_1K] = HIFMC_ECC_64BIT,    
    };

    if ((size_t)eccType >= (sizeof(eccCfgTab) / sizeof(eccCfgTab[0]))) {
        HDF_LOGE("%s: ecc type:%u not support", __func__, eccType);
        return HDF_ERR_NOT_SUPPORT;
    }
    return eccCfgTab[eccType];
}

int32_t HifmcCntlrInitOob(struct HifmcCntlr *cntlr, struct SpiFlash *spi)
{
    unsigned int reg;
    uint8_t pageCfg;
    uint8_t eccCfg;
    struct HifmcEccInfo *ecc = NULL;

#ifdef MTD_DEBUG
    HDF_LOGD("%s: start oob resize", __func__);
#endif

    ecc = HifmcCntlrGetEccInfo(spi);
    if (ecc == NULL) {
        return HDF_ERR_NOT_SUPPORT;
    }

    pageCfg = HifmcGetPageSizeConfig(ecc->pageSize);
    reg = HIFMC_REG_READ(cntlr, HIFMC_CFG_REG_OFF);
    reg &= ~HIFMC_PAGE_SIZE_MASK;
    reg |= HIFMC_CFG_PAGE_SIZE(pageCfg);

    eccCfg = HifmcGetEccTypeConfig(ecc->eccType);
    reg &= ~HIFMC_ECC_TYPE_MASK;
    reg |= HIFMC_CFG_ECC_TYPE(eccCfg);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CFG_REG_OFF);
    cntlr->cfg = reg;

#ifdef MTD_DEBUG
    HDF_LOGD("%s: config pagesize:%u ecctype:%u oobsize:%u",
        __func__, ecc->pageSize, ecc->eccType, ecc->oobSize);
#endif
    if (ecc->eccType != MTD_NAND_ECC_0BIT) {
        cntlr->oobSize = spi->mtd.oobSize = ecc->oobSize;
    }
    cntlr->pageSize = ecc->pageSize;
    cntlr->eccType = ecc->eccType;

#ifdef MTD_DEBUG
    HDF_LOGD("%s: end oob resize", __func__);
#endif

    return HDF_SUCCESS;
}

static int32_t HifmcMtdEraseSpinand(struct MtdDevice *mtdDevice, off_t addr, size_t len, off_t *faddr)
{
    int32_t ret;
    struct HifmcCntlr *cntlr = NULL;
    struct SpiFlash *spi = NULL; 

    if (mtdDevice == NULL || mtdDevice->cntlr == NULL) {
        HDF_LOGE("%s: mtd or cntlr is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = mtdDevice->cntlr;
    spi = CONTAINER_OF(mtdDevice, struct SpiFlash, mtd);

    if (addr % mtdDevice->eraseSize != 0 || len % mtdDevice->eraseSize != 0) {
        HDF_LOGE("%s: not aligned by page:%zu(addr:%jd, len:%zu)", __func__, mtdDevice->eraseSize, addr, len);
        return HDF_ERR_NOT_SUPPORT;
    }
    if ((len + addr) > mtdDevice->capacity) {
        HDF_LOGE("%s: out of range, addr:%jd, len:%zu", __func__, addr, len);
        return HDF_ERR_NOT_SUPPORT;
    }
    if (len < mtdDevice->eraseSize) {
        HDF_LOGE("%s: erase size:%zu < one block: 0x%zu", __func__, len, mtdDevice->eraseSize);
        return HDF_ERR_NOT_SUPPORT;
    }

    // lock cntlr
    (void)OsalMutexLock(&cntlr->mutex);
    while (len) {
#ifdef MTD_DEBUG
        HDF_LOGD("%s: start erase one block, addr=[%jd]", __func__, addr);
#endif
        ret = HifmcCntlrEraseOneBlock(cntlr, spi, addr);
        if (ret != HDF_SUCCESS) {
            if (faddr != NULL) {
                *faddr = addr;
            }
            (void)OsalMutexUnlock(&cntlr->mutex);
            return ret;
        }
        addr += mtdDevice->eraseSize;
        len -= mtdDevice->eraseSize;
    }
    // unlock cntlr
    (void)OsalMutexUnlock(&cntlr->mutex);
    return HDF_SUCCESS;
}

static int32_t HifmcCntlrWriteBuf(struct HifmcCntlr *cntlr,
    struct SpiFlash *spi, const uint8_t *buf, size_t len, off_t offset)
{
    int32_t ret;

    ret = memset_s((void *)(cntlr->buffer + spi->mtd.writeSize), spi->mtd.oobSize, 0xff, spi->mtd.oobSize);
    if (ret != 0) {
        HDF_LOGE("%s: memset_s failed!", __func__);
        return HDF_PLT_ERR_OS_API;
    }

    if ((offset + len) > cntlr->bufferSize) {
        HDF_LOGE("%s: invalid parms, offset:%jd, len:%zu(buffer size:%zu)",
            __func__, offset, len, cntlr->bufferSize);
        return HDF_ERR_INVALID_PARAM;
    }

    ret = memcpy_s((void *)(cntlr->buffer + offset), len, (void *)buf, len);
    if (ret != 0) {
        HDF_LOGE("%s: memcpy_s failed!", __func__);
        return HDF_PLT_ERR_OS_API;
    }

    return HDF_SUCCESS;
}

static int32_t HifmcCntlrPageProgram(struct HifmcCntlr *cntlr, struct SpiFlash *spi, uint32_t page)
{
    int32_t ret;
    unsigned int reg;

#ifdef MTD_DEBUG
    HDF_LOGD("%s: start program page @0x%x", __func__, page);
#endif

    ret = SpiFlashWaitReady(spi);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = SpiFlashWriteEnable(spi);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = HifmcCntlrSetSysClock(cntlr, spi->writeCfg.clock, 1);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    // enter normal mode
    reg = HIFMC_REG_READ(cntlr, HIFMC_CFG_REG_OFF);
    reg &= (~HIFMC_OP_MODE_MASK);
    reg |= HIFMC_CFG_OP_MODE(HIFMC_OP_MODE_NORMAL);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CFG_REG_OFF);

    reg = HIFMC_INT_CLR_ALL;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_INT_CLR_REG_OFF);

    reg = (page >> SPI_NAND_ADDR_REG_SHIFT) & 0xff;   // higher bits
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_ADDRH_REG_OFF);
    reg = (page & 0xffff) << SPI_NAND_ADDR_REG_SHIFT; // lower bits
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_ADDRL_REG_OFF);

    MtdDmaCacheClean(cntlr->dmaBuffer, spi->mtd.writeSize + spi->mtd.oobSize); 
    reg = (uintptr_t)LOS_PaddrQuery(cntlr->dmaBuffer);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DMA_SADDR_D0_REG_OFF);
    reg = (uintptr_t)LOS_PaddrQuery(cntlr->dmaBuffer + spi->mtd.writeSize);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DMA_SADDR_OOB_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(spi->cs);
    reg |= HIFMC_OP_CFG_MEM_IF_TYPE(spi->writeCfg.ifType);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_OP_CTRL_WR_OPCODE(spi->writeCfg.cmd);
    reg |= HIFMC_OP_CTRL_DMA_OP(HIFMC_OP_CTRL_TYPE_DMA);
    reg |= HIFMC_OP_CTRL_RW_OP(HIFMC_OP_CTRL_OP_WRITE);
    reg |= HIFMC_OP_CTRL_DMA_OP_READY;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CTRL_REG_OFF);

    HIFMC_DMA_WAIT_INT_FINISH(cntlr);
#ifdef MTD_DEBUG
    HDF_LOGD("%s: end program page @0x%x", __func__, page);
#endif
    return HDF_SUCCESS;
}

static int32_t HifmcCntlrReadOnePageToBuf(struct HifmcCntlr *cntlr, struct SpiFlash *spi, size_t page)
{
    int32_t ret;
    unsigned int reg;

#ifdef MTD_DEBUG
    HDF_LOGD("%s: start read page:0x%x", __func__, page);
#endif

    ret = SpiFlashWaitReady(spi);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = HifmcCntlrSetSysClock(cntlr, spi->readCfg.clock, 1);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    reg = HIFMC_INT_CLR_ALL;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_INT_CLR_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(spi->cs);
    reg |= HIFMC_OP_CFG_MEM_IF_TYPE(spi->readCfg.ifType);
    reg |= HIFMC_OP_CFG_DUMMY_NUM(spi->readCfg.dummy);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = (page >> 16) & 0xff; // write higher bits
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_ADDRH_REG_OFF);
    reg = (page & 0xffff) << 16; // write lower bits
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_ADDRL_REG_OFF);

    MtdDmaCacheInv(cntlr->dmaBuffer, spi->mtd.writeSize + spi->mtd.oobSize); 

    reg = (uintptr_t)LOS_PaddrQuery(cntlr->dmaBuffer);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DMA_SADDR_D0_REG_OFF);
    reg += spi->mtd.writeSize;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DMA_SADDR_OOB_REG_OFF);

    reg = HIFMC_OP_CTRL_RD_OPCODE(spi->readCfg.cmd);
    reg |= HIFMC_OP_CTRL_DMA_OP(HIFMC_OP_CTRL_TYPE_DMA);
    reg |= HIFMC_OP_CTRL_RW_OP(HIFMC_OP_CTRL_OP_READ);
    reg |= HIFMC_OP_CTRL_DMA_OP_READY;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CTRL_REG_OFF);

    HIFMC_DMA_WAIT_INT_FINISH(cntlr);

    MtdDmaCacheInv(cntlr->dmaBuffer, spi->mtd.writeSize + spi->mtd.oobSize); 

#ifdef MTD_DEBUG
    HDF_LOGD("%s: end read page:0x%x", __func__, page);
#endif
    return HDF_SUCCESS;
}

static bool HifmcMtdIsBadBlockSpinand(struct MtdDevice *mtdDevice, off_t addr)
{
    int32_t ret;
    uint8_t bb[MTD_NAND_BB_SIZE];
    size_t page;
    struct SpiFlash *spi = CONTAINER_OF(mtdDevice, struct SpiFlash, mtd);
    struct HifmcCntlr *cntlr = (struct HifmcCntlr *)spi->mtd.cntlr;

    page = (addr >> mtdDevice->writeSizeShift);
    ret = HifmcCntlrReadOnePageToBuf(cntlr, spi, page);
    if (ret != HDF_SUCCESS) {
        return false;
    }

    ret = HifmcCntlrReadBuf(cntlr, bb, MTD_NAND_BB_SIZE, spi->mtd.writeSize);
    if (ret != HDF_SUCCESS) {
        return false;
    }

#ifdef MTD_DEBUG
    HDF_LOGD("%s: bb[0] = 0x%x, bb[1] = 0x%x", __func__, bb[0], bb[1]);
#endif
    if (bb[0] != (uint8_t)0xff || bb[1] != (uint8_t)0xff) {
        return true;
    }
    return false;
}

static int32_t HifmcMtdMarkBadBlockSpinand(struct MtdDevice *mtdDevice, off_t addr)
{
    int32_t ret;
    uint8_t bb[MTD_NAND_BB_SIZE];
    uint8_t status;
    off_t page;
    struct HifmcCntlr *cntlr = NULL;
    struct SpiFlash *spi = CONTAINER_OF(mtdDevice, struct SpiFlash, mtd);

    page = addr >> mtdDevice->writeSizeShift;

    ret = memset_s(bb, MTD_NAND_BB_SIZE, 0x00, MTD_NAND_BB_SIZE);
    if (ret != 0) {
        HDF_LOGE("%s: memset_s failed!", __func__);
        return HDF_PLT_ERR_OS_API;
    }

    cntlr = (struct HifmcCntlr *)spi->mtd.cntlr;
    ret = HifmcCntlrWriteBuf(cntlr, spi, bb, MTD_NAND_BB_SIZE, spi->mtd.writeSize);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = HifmcCntlrPageProgram(cntlr, spi, page);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_STATUS_ADDR, &status);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    if ((status & MTD_SPI_NAND_PROG_FAIL) != 0) {
        HDF_LOGE("%s: page[0x%jx] program failed, status:0x%x", __func__, page, status);
        return HDF_ERR_IO;
    }

    return HDF_SUCCESS;
}

static int32_t HifmcCntlrWriteOnePage(struct HifmcCntlr *cntlr, struct SpiFlash *spi, struct MtdPage *mtdPage)
{
    int32_t ret;
    uint8_t status;
    uint8_t badFlag[MTD_NAND_BB_SIZE];
    uint8_t *oobBuf = NULL;
    size_t oobLen;
    size_t page;
    size_t to = mtdPage->addr;

    if ((to & (spi->mtd.writeSize - 1)) != 0) {
        HDF_LOGE("%s: addr:%zu not aligned by page size:%zu", __func__, to, spi->mtd.writeSize);
        return HDF_ERR_INVALID_PARAM;
    }
    page = (to >> spi->mtd.writeSizeShift);

    ret = HifmcCntlrWriteBuf(cntlr, spi, mtdPage->dataBuf, mtdPage->dataLen, 0);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    if (mtdPage->oobLen == 0 || mtdPage->oobBuf == NULL) {
        ret = memset_s(badFlag, MTD_NAND_BB_SIZE, 0xff, MTD_NAND_BB_SIZE);
        if (ret != 0) {
            HDF_LOGE("%s: memset_s failed!", __func__);
            return HDF_PLT_ERR_OS_API;
        }
        oobBuf = badFlag;
        oobLen = MTD_NAND_BB_SIZE;
    } else {
        oobBuf = mtdPage->oobBuf;
        oobLen = mtdPage->oobLen;
    }

    ret = HifmcCntlrWriteBuf(cntlr, spi, oobBuf, oobLen, spi->mtd.writeSize);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = HifmcCntlrPageProgram(cntlr, spi, page);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_STATUS_ADDR, &status);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    if ((status & MTD_SPI_NAND_PROG_FAIL) != 0) {
        HDF_LOGE("%s: page[0x%0x] program failed, status:0x%x", __func__, page, status);
        return HDF_ERR_IO;
    }

    return HDF_SUCCESS;
}

static int32_t HifmcCntlrReadOnePage(struct HifmcCntlr *cntlr, struct SpiFlash *spi, struct MtdPage *mtdPage)
{
    int32_t ret;
    size_t page;
    size_t offset;
    size_t from = mtdPage->addr;

    page = (from >> spi->mtd.writeSizeShift);
    offset = from & (spi->mtd.writeSize - 1);

    ret = HifmcCntlrReadOnePageToBuf(cntlr, spi, page);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = HifmcCntlrReadBuf(cntlr, mtdPage->dataBuf, mtdPage->dataLen, offset);
    if (ret != HDF_SUCCESS) {
        return false;
    }

    if (mtdPage->oobLen != 0 && mtdPage->oobBuf != 0) {
        ret = HifmcCntlrReadBuf(cntlr, mtdPage->oobBuf, mtdPage->oobLen, spi->mtd.writeSize);
        if (ret != HDF_SUCCESS) {
            return false;
        }
    }

    return HDF_SUCCESS;
}

static int32_t HifmcMtdPageTransfer(struct MtdDevice *mtdDevice, struct MtdPage *mtdPage)
{
    struct SpiFlash *spi = NULL;
    struct HifmcCntlr *cntlr = NULL;

    spi = CONTAINER_OF(mtdDevice, struct SpiFlash, mtd);
    cntlr = (struct HifmcCntlr *)mtdDevice->cntlr;
    if (mtdPage->type == MTD_MSG_TYPE_READ) {
        return HifmcCntlrReadOnePage(cntlr, spi, mtdPage);
    } else if (mtdPage->type == MTD_MSG_TYPE_WRITE) {
        return HifmcCntlrWriteOnePage(cntlr, spi, mtdPage);
    }
    return HDF_ERR_NOT_SUPPORT;
}

struct MtdDeviceMethod g_hifmcMtdMethodSpinand = {
    .erase = HifmcMtdEraseSpinand,
    .pageTransfer = HifmcMtdPageTransfer,
    .isBadBlock = HifmcMtdIsBadBlockSpinand,
    .markBadBlock = HifmcMtdMarkBadBlockSpinand,
};

static int32_t HifmcCntlrDevWpDisable(struct HifmcCntlr *cntlr, struct SpiFlash *spi)
{
    int32_t ret;
    uint8_t protect;

    ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_PROTECT_ADDR, &protect);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    if (SPI_NAND_ANY_BP_ENABLE(protect)) {
        protect &= ~SPI_NAND_ALL_BP_MASK;
        ret = HifmcCntlrDevFeatureOp(cntlr, spi, false, MTD_SPI_NAND_PROTECT_ADDR, &protect);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        ret = SpiFlashWaitReady(spi);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_PROTECT_ADDR, &protect);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        if (SPI_NAND_ANY_BP_ENABLE(protect)) {
            HDF_LOGE("%s: disable write protection failed, protect=0x%x", __func__, protect);
            return HDF_ERR_IO;
        }
    }
    return HDF_SUCCESS;
}

static int32_t HifmcCntlrDevEccDisable(struct HifmcCntlr *cntlr, struct SpiFlash *spi)
{
    int32_t ret;
    uint8_t feature;

    ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_FEATURE_ADDR, &feature);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    if ((feature & MTD_SPI_FEATURE_ECC_ENABLE) != 0) {
        feature &= ~MTD_SPI_FEATURE_ECC_ENABLE;
        ret = HifmcCntlrDevFeatureOp(cntlr, spi, false, MTD_SPI_NAND_FEATURE_ADDR, &feature);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        ret = SpiFlashWaitReady(spi);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        ret = HifmcCntlrDevFeatureOp(cntlr, spi, true, MTD_SPI_NAND_FEATURE_ADDR, &feature);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        if ((feature & MTD_SPI_FEATURE_ECC_ENABLE) != 0) {
            HDF_LOGE("%s: disable chip internal ecc failed, feature=0x%x", __func__, feature);
            return HDF_ERR_IO;
        }
    }
    return HDF_SUCCESS;
}

int32_t HifmcCntlrInitSpinandDevice(struct HifmcCntlr *cntlr, struct SpiFlash *spi)
{
    int32_t ret;

    (void)cntlr;

    spi->mtd.index = 0;
    spi->mtd.name = "spinand0";
    spi->mtd.ops = &g_hifmcMtdMethodSpinand;
    spi->mtd.type = MTD_TYPE_SPI_NAND;

    ret = SpinandGetSpiOps(spi);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = SpiFlashQeEnable(spi);
    if (ret != HDF_SUCCESS) {
        return ret; 
    }

    ret = HifmcCntlrDevWpDisable(cntlr, spi);
    if (ret != HDF_SUCCESS) {
        return ret; 
    }
    
    ret = HifmcCntlrDevEccDisable(cntlr, spi);
    if (ret != HDF_SUCCESS) {
        return ret; 
    }
    
    return HDF_SUCCESS;
}
