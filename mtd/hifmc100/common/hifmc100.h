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

#ifndef HIFMC100_H
#define HIFMC100_H

#include "hdf_base.h"
#include "mtd_spi_common.h"
#include "osal_mutex.h"
#include "osal_io.h"
#include "hisoc/flash.h"

struct HifmcCntlr {
    volatile void *regBase;
    uint32_t regBasePhy;
    uint32_t regSize;
    volatile uint8_t *memBase;
    uint32_t memBasePhy;
    uint32_t memSize;
    uint8_t *buffer;
    uint8_t *dmaBuffer;
    size_t bufferSize;
    int32_t chipNum;
    unsigned int flashType;
    struct OsalMutex mutex;
    size_t pageSize;
    size_t oobSize;
    int eccType;
    struct SpiFlash *curDev;
    unsigned int cfg;
    const struct DeviceResourceNode *drsNode;
};

#define HIFMC_SPI_NOR_CS_NUM                     0

#define HIFMC_DMA_ALIGN_SIZE                     (CACHE_ALIGNED_SIZE)
#define HIFMC_DMA_ALIGN_MASK                     (HIFMC_DMA_ALIGN_SIZE - 1)
#define HIFMC_DMA_MAX_SIZE                       4096
#define HIFMC_DMA_MAX_MASK                       (HIFMC_DMA_MAX_SIZE - 1)

#define HIFMC_REG_READ(_cntlr, _reg) \
    OSAL_READL((uintptr_t)((uint8_t *)(_cntlr)->regBase + (_reg)))

#define HIFMC_REG_WRITE(_cntlr, _value, _reg) \
    OSAL_WRITEL((unsigned long)(_value), (uintptr_t)((uint8_t *)(_cntlr)->regBase + (_reg)))

// ********* 0x0000 FMC_CFG ****************************************************************
#define HIFMC_CFG_REG_OFF                            0x00
#define HIFMC_CFG_SPI_NAND_SEL(type)                 (((type) & 0x3) << 11)
#define HIFMC_CFG_SPI_NOR_ADDR_MODE(mode)            ((mode) << 10)
#define HIFMC_CFG_BLOCK_SIZE(size)                   (((size) & 0x3) << 8)
#define HIFMC_CFG_OP_MODE(mode)                      ((mode) & 0x1)

#define HIFMC_SPI_NOR_ADDR_MODE_SHIFT                10
#define HIFMC_SPI_NOR_ADDR_MODE_MASK                 (0x1 << HIFMC_SPI_NOR_ADDR_MODE_SHIFT)

// option mode
#define HIFMC_OP_MODE_BOOT                           0x0
#define HIFMC_OP_MODE_NORMAL                         0x1
#define HIFMC_OP_MODE_MASK                           0x1

// page config
#define HIFMC_PAGE_SIZE_SHIFT                        3
#define HIFMC_PAGE_SIZE_MASK                         (0x3 << HIFMC_PAGE_SIZE_SHIFT)
#define HIFMC_CFG_PAGE_SIZE(size)                    (((size) & 0x3) << HIFMC_PAGE_SIZE_SHIFT)

// ecc config
#define HIFMC_ECC_TYPE_SHIFT                         5
#define HIFMC_ECC_TYPE_MASK                          (0x7 << HIFMC_ECC_TYPE_SHIFT)
#define HIFMC_CFG_ECC_TYPE(type)                     (((type) & 0x7) << 5)

// others
#define HIFMC_CFG_TYPE_SHIFT                         1
#define HIFMC_CFG_TYPE_MASK                          (0x3 << HIFMC_CFG_TYPE_SHIFT)
#define HIFMC_CFG_TYPE_SEL(type)                     (((type) & 0x3) << HIFMC_CFG_TYPE_SHIFT)
#define HIFMC_CFG_TYPE_SPI_NOR                       0x0
#define HIFMC_CFG_TYPE_SPI_NAND                      0x1
#define HIFMC_CFG_TYPE_NAND                          0x2
#define HIFMC_CFG_TYPE_DEFAULT                       0x3

enum HifmcPageSizeRegConfig {
    HIFMC_PAGE_SIZE_2K    = 0x0,
    HIFMC_PAGE_SIZE_4K    = 0x1,
    HIFMC_PAGE_SIZE_8K    = 0x2,
    HIFMC_PAGE_SIZE_16K   = 0x3,
};

enum HifmcEccTypeRegConfig {
    HIFMC_ECC_0BIT     = 0x00,
    HIFMC_ECC_8BIT     = 0x01,
    HIFMC_ECC_16BIT    = 0x02,
    HIFMC_ECC_24BIT    = 0x03,
    HIFMC_ECC_28BIT    = 0x04,
    HIFMC_ECC_40BIT    = 0x05,
    HIFMC_ECC_64BIT    = 0x06,
};

// ********* 0x0004 GLOBAL_CFG ************************************************************
#define HIFMC_GLOBAL_CFG_REG_OFF                     0x4
#define HIFMC_GLOBAL_CFG_WP_ENABLE                   (1 << 6)

// ********* 0x0008 TIMING_SPI_CFG ********************************************************
#define HIFMC_SPI_TIMING_CFG_REG_OFF                 0x08
#define HIFMC_SPI_TIMING_CFG_TCSH(n)                 (((n) & 0xf) << 8)
#define HIFMC_SPI_TIMING_CFG_TCSS(n)                 (((n) & 0xf) << 4)
#define HIFMC_SPI_TIMING_CFG_TSHSL(n)                ((n) & 0xf)
#define HIFMC_SPI_CFG_CS_HOLD                        0x6
#define HIFMC_SPI_CFG_CS_SETUP                       0x6
#define HIFMC_SPI_CFG_CS_DESELECT                    0xf

// ********* 0x0018 FMC_INT ***************************************************************
#define HIFMC_INT_REG_OFF                            0x18
#define HIFMC_INT_AHB_OP                             (1 << 7)
#define HIFMC_INT_WR_LOCK                            (1 << 6)
#define HIFMC_INT_DMA_ERR                            (1 << 5)
#define HIFMC_INT_ERR_ALARM                          (1 << 4)
#define HIFMC_INT_ERR_INVALID                        (1 << 3)
#define HIFMC_INT_ERR_VALID                          (1 << 2)
#define HIFMC_INT_OP_FAIL                            (1 << 1)
#define HIFMC_INT_OP_DONE                            (1 << 0)

// ********* 0x001c FMC_INT_EN ************************************************************
#define HIFMC_INT_EN_REG_OFF                         0x1c
#define HIFMC_INT_EN_AHB_OP                          (1 << 7)
#define HIFMC_INT_EN_WR_LOCK                         (1 << 6)
#define HIFMC_INT_EN_DMA_ERR                         (1 << 5)
#define HIFMC_INT_EN_ERR_ALARM                       (1 << 4)
#define HIFMC_INT_EN_ERR_INVALID                     (1 << 3)
#define HIFMC_INT_EN_ERR_VALID                       (1 << 2)
#define HIFMC_INT_EN_OP_FAIL                         (1 << 1)
#define HIFMC_INT_EN_OP_DONE                         (1 << 0)

// ********* 0x0020 FMC_INT_CLR ***********************************************************
#define HIFMC_INT_CLR_REG_OFF                        0x20
#define HIFMC_INT_CLR_AHB_OP                         (1 << 7)
#define HIFMC_INT_CLR_WR_LOCK                        (1 << 6)
#define HIFMC_INT_CLR_DMA_ERR                        (1 << 5)
#define HIFMC_INT_CLR_ERR_ALARM                      (1 << 4)
#define HIFMC_INT_CLR_ERR_INVALID                    (1 << 3)
#define HIFMC_INT_CLR_ERR_VALID                      (1 << 2)
#define HIFMC_INT_CLR_OP_FAIL                        (1 << 1)
#define HIFMC_INT_CLR_OP_DONE                        (1 << 0)
#define HIFMC_INT_CLR_ALL                            0xff

// ********* 0x0024 FMC_CMD ***************************************************************
#define HIFMC_CMD_REG_OFF                            0x24
#define HIFMC_CMD_CMD1(cmd)                          ((cmd) & 0xff)
#define HIFMC_CMD_CMD2(cmd)                          (((cmd) & 0xff) << 8)

// ********* 0x0028 FMC_ADDRH *************************************************************
#define HIFMC_ADDRH_REG_OFF                          0x28
// ********* 0x002c FMC_ADDRL *************************************************************
#define HIFMC_ADDRL_REG_OFF                          0x2c

// ********* 0x0030 FMC_OP_CFG ************************************************************
#define HIFMC_OP_CFG_REG_OFF                         0x30
#define HIFMC_OP_CFG_FM_CS(cs)                       ((cs) << 11)
#define HIFMC_OP_CFG_FORCE_CS_EN(en)                 ((en) << 10)
#define HIFMC_OP_CFG_MEM_IF_TYPE(type)               (((type) & 0x7) << 7)
#define HIFMC_OP_CFG_ADDR_NUM(addr)                  (((addr) & 0x7) << 4)
#define HIFMC_OP_CFG_DUMMY_NUM(dummy)                ((dummy) & 0xf)

// ********* 0x0034 SPI_OP_ADDR ***********************************************************
#define HIFMC_SPI_OP_ADDR_REG_OFF                    0x34

// ********* 0x0038 FMC_DATA_NUM **********************************************************
#define HIFMC_DATA_NUM_REG_OFF                       0x38
#define HIFMC_DATA_NUM_CNT(n)                        ((n) & 0x3fff)

// ********* 0x003c FMC_OP ****************************************************************
#define HIFMC_OP_REG_OFF                             0x3c
#define HIFMC_OP_DUMMY_EN(en)                        ((en) << 8)
#define HIFMC_OP_CMD1_EN(en)                         ((en) << 7)
#define HIFMC_OP_ADDR_EN(en)                         ((en) << 6)
#define HIFMC_OP_WRITE_DATA_EN(en)                   ((en) << 5)
#define HIFMC_OP_CMD2_EN(en)                         ((en) << 4)
#define HIFMC_OP_WAIT_READY_EN(en)                   ((en) << 3)
#define HIFMC_OP_READ_DATA_EN(en)                    ((en) << 2)
#define HIFMC_OP_READ_STATUS_EN(en)                  ((en) << 1)
#define HIFMC_OP_REG_OP_START                        1

// ********* 0x0040 FMC_DATA_LEN **********************************************************
#define HIFMC_DMA_LEN_REG_OFF                        0x40
#define HIFMC_DMA_LEN_SET(len)                       ((len) & 0x0fffffff)

// ********* 0x0048 FMC_DMA_AHB_CTRL ******************************************************
#define HIFMC_DMA_AHB_CTRL_REG_OFF                   0x48
#define HIFMC_DMA_AHB_CTRL_DMA_PP_EN                 (1 << 3)
#define HIFMC_DMA_AHB_CTRL_BURST16_EN                (1 << 2)
#define HIFMC_DMA_AHB_CTRL_BURST8_EN                 (1 << 1)
#define HIFMC_DMA_AHB_CTRL_BURST4_EN                 (1 << 0)
#define HIFMC_ALL_BURST_ENABLE                       (HIFMC_DMA_AHB_CTRL_BURST16_EN \
                                                     | HIFMC_DMA_AHB_CTRL_BURST8_EN \
                                                     | HIFMC_DMA_AHB_CTRL_BURST4_EN)

// ********* 0x004c FMC_DMA_SADDR_D0 ******************************************************
#define HIFMC_DMA_SADDR_D0_REG_OFF                   0x4c
#define HIFMC_DMA_SADDRH_D0                          0x200
#define HIFMC_DMA_SADDRH_OOB_OOFSET                  0x210
#define HIFMC_DMA_ADDR_OFFSET                        4096

// ********* 0x005c FMC_DMA_SADDR_OOB *****************************************************
#define HIFMC_DMA_SADDR_OOB_REG_OFF                  0x5c

// ********* 0x0068 FMC_OP_CTRL ***********************************************************
#define HIFMC_OP_CTRL_REG_OFF                        0x68
#define HIFMC_OP_CTRL_RD_OPCODE(code)                (((code) & 0xff) << 16)
#define HIFMC_OP_CTRL_WR_OPCODE(code)                (((code) & 0xff) << 8)
#define HIFMC_OP_CTRL_RW_OP(op)                      ((op) << 1)
#define HIFMC_OP_CTRL_RD_OP_SEL(op)                  (((op) & 0x3) << 4)
#define HIFMC_OP_CTRL_DMA_OP(type)                   ((type) << 2)
#define HIFMC_OP_CTRL_DMA_OP_READY                   1
#define HIFMC_OP_CTRL_TYPE_DMA                       0
#define HIFMC_OP_CTRL_TYPE_REG                       1
#define HIFMC_OP_CTRL_OP_READ                        0
#define HIFMC_OP_CTRL_OP_WRITE                       1

// ********* 0x006c FMC_TIMEOUT_WR ********************************************************

// ********* 0x0070 FMC_OP_PARA ***********************************************************
#define HIFMC_OP_PARA_REG_OFF                        0x70
#define HIFMC_OP_PARA_RD_OOB_ONLY                    (1 << 1)

// ********* 0x0074 FMC_BOOT_SEL **********************************************************
#define HIFMC_BOOT_SET_REG_OFF                       0x74
#define HIFMC_BOOT_SET_DEVICE_ECC_EN                 (1 << 3)
#define HIFMC_BOOT_SET_BOOT_QUAD_EN                  (1 << 1)

// ********* 0x0078 FMC_LP_CTRL ***********************************************************

// ********* 0x00a8 FMC_ERR_THD ***********************************************************

// ********* 0x00ac FMC_FLASH_INFO ********************************************************
#define HIFMC_FLASH_INFO_REG_OFF                     0xac

// ********* 0x00bc FMC_VERSION ***********************************************************
#define HIFMC_VERSION_REG_OFF                        0xbc

// ********* 0x00c0 FMC_ERR_NUM0_BUF0 *****************************************************

// ********* 0x00d0 FMC_ERR_ALARM_ADDRH ***************************************************

// ********* 0x00d4 FMC_ERR_ALARM_ADDRL ***************************************************

// ********* 0x00d8 FMC_ECC_INVALID_ADDRH *************************************************

// ********* 0x00dc FMC_ECC_INVALID_ADDRL *************************************************

#define HIFMC_CPU_WAIT_TIMEOUT                       0x800000
#define HIFMC_DMA_WAIT_TIMEOUT                       0xf0000000

#define HIFMC_CMD_WAIT_CPU_FINISH(cntlr) \
    do { \
        unsigned int regval, timeout = HIFMC_CPU_WAIT_TIMEOUT; \
        do { \
            regval = HIFMC_REG_READ((cntlr), HIFMC_OP_REG_OFF); \
            --timeout; \
        } while ((regval & HIFMC_OP_REG_OP_START) && timeout); \
        if (!timeout) \
            HDF_LOGE("%s: wait cmd cpu finish timeout(0x%x)", __func__, regval); \
    } while (0)


#define HIFMC_DMA_WAIT_INT_FINISH(cntlr) \
    do { \
        unsigned int regval, timeout = HIFMC_DMA_WAIT_TIMEOUT; \
        do { \
            regval = HIFMC_REG_READ((cntlr), HIFMC_INT_REG_OFF); \
            --timeout; \
        } while (!(regval & HIFMC_INT_OP_DONE) && timeout); \
        if (!timeout) \
            HDF_LOGE("%s: wait dma int finish timeout(0x%x)", __func__, regval); \
    } while (0)

#define HIFMC_DMA_WAIT_CPU_FINISH(cntlr) \
    do { \
        unsigned long regval, timeout = HIFMC_CPU_WAIT_TIMEOUT; \
        do { \
            regval = HIFMC_REG_READ((cntlr), HIFMC_OP_CTRL_REG_OFF); \
            --timeout; \
        } while ((regval & HIFMC_OP_CTRL_DMA_OP_READY) && timeout); \
        if (!timeout) \
            HDF_LOGE("%s: wait dma cpu finish timeout(0x%x)", __func__, regval); \
    } while (0)

uint8_t HifmcCntlrReadDevReg(struct HifmcCntlr *cntlr, struct SpiFlash *spi, uint8_t cmd);

void HifmcCntlrSet4AddrMode(struct HifmcCntlr *cntlr, int enable);

const struct DeviceResourceNode *HifmcCntlrGetDevTableNode(struct HifmcCntlr *cntlr);

int32_t HifmcCntlrReadSpiOp(struct MtdSpiConfig *cfg, const struct DeviceResourceNode *node);

static inline int32_t HifmcCntlrSetSysClock(struct HifmcCntlr *cntlr, unsigned int clock, int clkEn)
{
    (void)cntlr;
    hifmc100_set_system_clock(clock, clkEn);
    return HDF_SUCCESS;
}

void MtdDmaCacheClean(void *addr, size_t size);

void MtdDmaCacheInv(void *addr, size_t size);

#endif /* HIFMC100_H */
