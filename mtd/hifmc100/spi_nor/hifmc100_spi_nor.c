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

#include "hifmc100_spi_nor.h"
#include "asm/platform.h"
#include "hisoc/flash.h"
#include "los_vm_phys.h"
#include "securec.h"

#include "device_resource_if.h"
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "mtd_spi_nor.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "osal_mutex.h"
#include "platform_core.h"

#include "hifmc100.h"

static int32_t HifmcCntlrReadSpinorInfo(struct SpinorInfo *info, const struct DeviceResourceNode *node)
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
    if (ret <= 0) {
        HDF_LOGE("%s: get id len failed:%d", __func__, ret);
        return ret;
    }
    info->idLen = ret;

    ret = drsOps->GetUint8Array(node, "id", info->id, info->idLen, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read reg base fail:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint32(node, "block_size", &info->blockSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read block size failed:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint32(node, "chip_size", &info->chipSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read block size failed:%d", __func__, ret);
        return ret;
    }

    ret = drsOps->GetUint32(node, "addr_cycle", &info->addrCycle, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read addr cycle failed:%d", __func__, ret);
        return ret;
    }

    if ((ret = HifmcCntlrReadSpiOp(&info->eraseCfg, drsOps->GetChildNode(node, "erase_op"))) != HDF_SUCCESS ||
        (ret = HifmcCntlrReadSpiOp(&info->writeCfg, drsOps->GetChildNode(node, "write_op"))) != HDF_SUCCESS ||
        (ret = HifmcCntlrReadSpiOp(&info->readCfg, drsOps->GetChildNode(node, "read_op"))) != HDF_SUCCESS) {
        return ret;
    }

    return HDF_SUCCESS;
}

int32_t HifmcCntlrSearchSpinorInfo(struct HifmcCntlr *cntlr, struct SpiFlash *spi)
{
    unsigned int i;
    int32_t ret;
    struct SpinorInfo spinorInfo;
    const struct DeviceResourceNode *childNode = NULL;
    const struct DeviceResourceNode *tableNode = NULL;

    tableNode = HifmcCntlrGetDevTableNode(cntlr);
    if (tableNode == NULL) {
        return HDF_ERR_NOT_SUPPORT;
    }

    DEV_RES_NODE_FOR_EACH_CHILD_NODE(tableNode, childNode) {
        ret = HifmcCntlrReadSpinorInfo(&spinorInfo, childNode);
        if (ret != HDF_SUCCESS) {
            return HDF_ERR_IO;
        }
        if (memcmp(spinorInfo.id, spi->mtd.id, spinorInfo.idLen) == 0) {
            spi->mtd.chipName = spinorInfo.name;
            spi->mtd.idLen = spinorInfo.idLen;
            spi->mtd.capacity = spinorInfo.chipSize;
            spi->mtd.eraseSize = spinorInfo.blockSize;
            spi->addrCycle = spinorInfo.addrCycle;
            spi->eraseCfg = spinorInfo.eraseCfg;
            spi->writeCfg = spinorInfo.writeCfg;
            spi->readCfg = spinorInfo.readCfg;
            return SpinorGetSpiOps(spi);
        }
    }

    HDF_LOGE("%s: dev id not support", __func__);
    for (i = 0; i < sizeof(spi->mtd.id); i++) {
        HDF_LOGE("%s: mtd->id[%i] = %x", __func__, i, spi->mtd.id[i]);
    }
    return HDF_ERR_NOT_SUPPORT;
}

uint8_t HifmcCntlrReadSpinorReg(struct HifmcCntlr *cntlr, struct SpiFlash *spi, uint8_t cmd)
{
    uint8_t status;
    unsigned long reg;

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

int32_t HifmcCntlrReadIdSpiNor(struct HifmcCntlr *cntlr, uint8_t cs, uint8_t *id, size_t len)
{
    int32_t ret;
    unsigned long reg;

    if (len < MTD_FLASH_ID_LEN_MAX) {
        HDF_LOGE("%s: buf not enough(len: %u, expected %u)", __func__, len, MTD_FLASH_ID_LEN_MAX);
        return HDF_ERR_INVALID_PARAM;
    }

    reg = HIFMC_CMD_CMD1(MTD_SPI_CMD_RDID);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = HIFMC_OP_CFG_FM_CS(cs);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_DATA_NUM_CNT(MTD_FLASH_ID_LEN_MAX);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DATA_NUM_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) | HIFMC_OP_READ_DATA_EN(1) | HIFMC_OP_REG_OP_START;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

    ret = memcpy_s(id, MTD_FLASH_ID_LEN_MAX, (const void *)cntlr->memBase, MTD_FLASH_ID_LEN_MAX);
    if (ret != EOK) {
        HDF_LOGE("%s: copy id buf failed : %d", __func__, ret);
        return HDF_PLT_ERR_OS_API;
    }

    return HDF_SUCCESS;
}

static int32_t HifmcCntlrEraseOneBlock(struct HifmcCntlr *cntlr, struct SpiFlash *spi, off_t from)
{
    int32_t ret;
    unsigned long reg;

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

    reg = HIFMC_CMD_CMD1(spi->eraseCfg.cmd);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_CMD_REG_OFF);

    reg = from;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_ADDRL_REG_OFF);

    reg =  HIFMC_OP_CFG_FM_CS(spi->cs) |
           HIFMC_OP_CFG_MEM_IF_TYPE(spi->eraseCfg.ifType) |
           HIFMC_OP_CFG_ADDR_NUM(spi->addrCycle) |
           HIFMC_OP_CFG_DUMMY_NUM(spi->eraseCfg.dummy);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_OP_CMD1_EN(1) |
          HIFMC_OP_ADDR_EN(1) |
          HIFMC_OP_REG_OP_START;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_REG_OFF);

    HIFMC_CMD_WAIT_CPU_FINISH(cntlr);

    return HDF_SUCCESS;
}

static int32_t HifmcSpinorErase(struct MtdDevice *mtdDevice, off_t from, size_t len, off_t *failAddr)
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
    // lock cntlr
    (void)OsalMutexLock(&cntlr->mutex);
    while (len) {
#ifdef MTD_DEBUG
        HDF_LOGD("%s: start erase one block, addr=[%jd]", __func__, from);
#endif
        ret = HifmcCntlrEraseOneBlock(cntlr, spi, from);
        if (ret != HDF_SUCCESS) {
            *failAddr = from;
            (void)OsalMutexUnlock(&cntlr->mutex);
            return ret;
        }
        from += mtdDevice->eraseSize;
        len -= mtdDevice->eraseSize;
    }
    // unlock cntlr
    (void)OsalMutexUnlock(&cntlr->mutex);

    return HDF_SUCCESS;
}

static int32_t HifmcCntlrDmaTransfer(struct HifmcCntlr *cntlr, struct SpiFlash *spi,
    off_t offset, uint8_t *buf, size_t len, int wr)
{
    uint8_t ifType;
    uint8_t dummy;
    uint8_t wCmd;
    uint8_t rCmd;
    unsigned long reg;

#ifdef MTD_DEBUG
    if (wr == 1)
    HDF_LOGD("%s: start dma transfer => [%jd], len[%zu], wr=%d, buf=%p", __func__, offset, len, wr, buf);
#endif
    if (wr == 1) {
        MtdDmaCacheClean((void *)buf, len);
    } else {
        MtdDmaCacheInv((void *)buf, len);
    }

    reg = HIFMC_INT_CLR_ALL;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_INT_CLR_REG_OFF);

    reg = offset;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_ADDRL_REG_OFF);

    ifType = (wr == 1) ? spi->writeCfg.ifType : spi->readCfg.ifType;
    dummy = (wr == 1) ? spi->writeCfg.dummy : spi->readCfg.dummy;
    wCmd = (wr == 1) ? spi->writeCfg.cmd : 0;
    rCmd = (wr == 0) ? spi->readCfg.cmd : 0;

    reg = HIFMC_OP_CFG_FM_CS(spi->cs) |
          HIFMC_OP_CFG_MEM_IF_TYPE(ifType) |
          HIFMC_OP_CFG_ADDR_NUM(spi->addrCycle) |
          HIFMC_OP_CFG_DUMMY_NUM(dummy);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CFG_REG_OFF);

    reg = HIFMC_DMA_LEN_SET(len);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DMA_LEN_REG_OFF);

    reg = (unsigned long)((uintptr_t)buf);
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_DMA_SADDR_D0_REG_OFF);

    reg = HIFMC_OP_CTRL_RD_OPCODE(rCmd) |
          HIFMC_OP_CTRL_WR_OPCODE(wCmd) |
          HIFMC_OP_CTRL_RW_OP(wr) |
          HIFMC_OP_CTRL_DMA_OP_READY;
    HIFMC_REG_WRITE(cntlr, reg, HIFMC_OP_CTRL_REG_OFF);

    HIFMC_DMA_WAIT_INT_FINISH(cntlr);
    if (wr == 0) {
        MtdDmaCacheInv((void *)buf, len);
    }

#ifdef MTD_DEBUG
    HDF_LOGD("%s: end dma transfer", __func__);
#endif
    return HDF_SUCCESS;
}

static int32_t HifmcCntlrDmaWriteReadOnce(struct SpiFlash *spi, off_t offset, uint8_t *buf, size_t num, int wr)
{
    int32_t ret;
    struct HifmcCntlr *cntlr = (struct HifmcCntlr *)spi->mtd.cntlr;

    if (num == 0) {
        return HDF_SUCCESS;
    }

    if (num > HIFMC_DMA_ALIGN_MASK) {
        if (((uintptr_t)buf & HIFMC_DMA_ALIGN_MASK) != 0) {
            HDF_LOGE("%s: block buf not aligned by : %u", __func__, HIFMC_DMA_ALIGN_MASK);
            return HDF_ERR_INVALID_PARAM;
        }
        ret = HifmcCntlrDmaTransfer(cntlr, spi, offset,
            (uint8_t *)(uintptr_t)LOS_PaddrQuery((void *)(buf)), num, wr);
        return ret;
    }

    if (wr == 1) { // write
        if (LOS_CopyToKernel((void *)cntlr->dmaBuffer, num, (void *)buf, num) != 0) {
            HDF_LOGE("%s: copy from user failed, num = %zu", __func__, num);
            return HDF_ERR_IO;
        }
    }

    ret = HifmcCntlrDmaTransfer(cntlr, spi, offset,
        (uint8_t *)(uintptr_t)LOS_PaddrQuery((void *)(cntlr->dmaBuffer)), num, wr);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: dma transfer failed : %d(num = %zu)", __func__, ret, num);
        return ret;
    }

    if (wr == 0) { // read
        if (LOS_CopyFromKernel((void *)buf, num, (void *)cntlr->dmaBuffer, num) != 0) {
            HDF_LOGE("%s: copy to user failed, num = %zu", __func__, num);
            return HDF_ERR_IO;
        }
    }

    return ret;    
}

static int32_t HifmcCntlrDmaWriteRead(struct MtdDevice *mtdDevice, off_t offset, size_t len, uint8_t *buf, int wr)
{
    unsigned int i;
    int32_t ret;
    size_t num;
    size_t sizeL;
    size_t sizeM;
    size_t sizeR;
    size_t *sizeArray[] = {&sizeL, &sizeM, &sizeR};
    struct HifmcCntlr *cntlr = (struct HifmcCntlr *)mtdDevice->cntlr;
    struct SpiFlash *spi = CONTAINER_OF(mtdDevice, struct SpiFlash, mtd);

    if ((ret = SpiFlashWaitReady(spi)) != HDF_SUCCESS) {
        return ret;
    }

    if (wr == 1 && (ret = SpiFlashWriteEnable(spi)) != HDF_SUCCESS) {
        return ret;
    }

    // set system clock for erase 
    ret = HifmcCntlrSetSysClock(cntlr, (wr == 1) ? spi->writeCfg.clock : spi->readCfg.clock, 1);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    sizeL = HIFMC_DMA_ALIGN_SIZE - ((uintptr_t)buf & HIFMC_DMA_ALIGN_MASK); // be careful!
    if (sizeL > len) {
        sizeL = len;
    }
    len -= sizeL;

    sizeM = len & (~HIFMC_DMA_ALIGN_MASK);
    len -= sizeM;

    sizeR = len;

    for (i = 0, num = *sizeArray[0]; i < (sizeof(sizeArray) / sizeof(sizeArray[0])); i++) {
        num = *sizeArray[i];
        ret = HifmcCntlrDmaWriteReadOnce(spi, offset, buf, num, wr);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%s: dma trans failed(num = %zu)", __func__, num);
            return HDF_ERR_IO;
        }
        offset += num;
        buf += num;
    }

    return HDF_SUCCESS;
}

static int32_t HifmcSpinorWrite(struct MtdDevice *mtdDevice, off_t to, size_t len, const uint8_t *buf)
{
    int32_t ret;
    struct HifmcCntlr *cntlr = NULL;

    if (mtdDevice == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = mtdDevice->cntlr;

#ifdef MTD_DEBUG
    HDF_LOGD("%s: start write buf(%p) to=%jd len=%zu", __func__, buf, to, len);
#endif
    // lock cntlr
    (void)OsalMutexLock(&cntlr->mutex);
    ret = HifmcCntlrDmaWriteRead(mtdDevice, to, len, (uint8_t *)buf, 1);
    // unlock cntlr
    (void)OsalMutexUnlock(&cntlr->mutex);

#ifdef MTD_DEBUG
    HDF_LOGD("%s: start write buf(%p) to=%jd len=%zu, done!", __func__, buf, to, len);
#endif
    return ret;
}

static int32_t HifmcSpinorRead(struct MtdDevice *mtdDevice, off_t from, size_t len, uint8_t *buf)
{
    int32_t ret;
    struct HifmcCntlr *cntlr = NULL;

    if (mtdDevice == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    cntlr = mtdDevice->cntlr;

#ifdef MTD_DEBUG
    HDF_LOGD("%s: start read buf:%p from=%jd len=%zu", __func__, buf, from, len);
#endif
    // lock cntlr
    (void)OsalMutexLock(&cntlr->mutex);
    ret = HifmcCntlrDmaWriteRead(mtdDevice, from, len, buf, 0);
    // unlock cntlr
    (void)OsalMutexUnlock(&cntlr->mutex);

    return ret;
}

struct MtdDeviceMethod g_hifmcMtdMethodSpinor = {
    .read = HifmcSpinorRead,
    .write = HifmcSpinorWrite,
    .erase = HifmcSpinorErase,
};

int32_t HifmcCntlrInitSpinorDevice(struct HifmcCntlr *cntlr, struct SpiFlash *spi)
{
    int32_t ret;

    (void)cntlr;
    spi->mtd.index = 0;
    spi->mtd.name = "mtd0";
    spi->mtd.ops = &g_hifmcMtdMethodSpinor;
    spi->mtd.type = MTD_TYPE_SPI_NOR;
    spi->mtd.writeSize = 1;
    spi->mtd.readSize = 1;

    ret = SpiFlashQeEnable(spi);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = SpiFlashEntry4Addr(spi, (GET_FMC_BOOT_MODE == MTD_SPI_ADDR_3BYTE));
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    return HDF_SUCCESS;
}
