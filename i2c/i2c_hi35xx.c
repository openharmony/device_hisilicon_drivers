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

#include "i2c_hi35xx.h"
#include "asm/platform.h"
#include "los_hwi.h"
#include "securec.h"
#include "device_resource_if.h"
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "i2c_core.h"
#include "i2c_dev.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "osal_spinlock.h"
#include "osal_time.h"
#include "plat_log.h"

#define HDF_LOG_TAG i2c_hi35xx
#define USER_VFS_SUPPORT

#define HI35XX_I2C_DELAY      50
#define I2C_FREQ_NORMAL      100000
#define HI35XX_SCL_HIGH_CNT   36
#define HI35XX_SCL_LOW_CNT    64
#define HI35XX_SCL_FULL_CNT   100
#define HI35XX_REG_SIZE       4
#define HI35XX_I2C_R_LOOP_ADJ 2
#define HI35XX_I2C_RESCUE_TIMES 9
#define HI35XX_I2C_RESCUE_DELAY 10

struct Hi35xxI2cCntlr {
    struct I2cCntlr cntlr;
    OsalSpinlock spin;
    volatile unsigned char  *regBase;
    uint16_t regSize;
    int16_t bus;
    uint32_t clk;
    uint32_t freq;
    uint32_t irq;
    uint32_t regBasePhy;
};

struct Hi35xxTransferData {
    struct I2cMsg *msgs;
    int16_t index;
    int16_t count;
};

#define WRITE_REG_BIT(value, offset, addr) \
    do {                                   \
        unsigned long t, mask;             \
        mask = 1 << (offset);              \
        t = OSAL_READL(addr);              \
        t &= ~mask;                        \
        t |= ((value) << (offset)) & mask; \
        OSAL_WRITEL(t, (addr));            \
    } while (0)

#define REG_CRG_I2C           (CRG_REG_BASE + 0x01b8)
#define I2C_CRG_RST_OFFSET    19
#define I2C_CRG_CLK_OFFSET    11
static inline void Hi35xxI2cHwInitCfg(struct Hi35xxI2cCntlr *hi35xx)
{
    unsigned long busId = (unsigned long)hi35xx->bus;

    WRITE_REG_BIT(1, I2C_CRG_CLK_OFFSET + busId, REG_CRG_I2C);
    WRITE_REG_BIT(0, I2C_CRG_RST_OFFSET + busId, REG_CRG_I2C);
}


static inline void Hi35xxI2cEnable(const struct Hi35xxI2cCntlr *hi35xx)
{
    unsigned int val;

    val = OSAL_READL(hi35xx->regBase + HI35XX_I2C_GLB);
    val |= GLB_EN_MASK;
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_GLB);
}

static inline void Hi35xxI2cDisable(const struct Hi35xxI2cCntlr *hi35xx)
{
    unsigned int val;

    val = OSAL_READL(hi35xx->regBase + HI35XX_I2C_GLB);
    val &= ~GLB_EN_MASK;
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_GLB);
}

static inline void Hi35xxI2cDisableIrq(const struct Hi35xxI2cCntlr *hi35xx, unsigned int flag)
{
    unsigned int val;

    val = OSAL_READL(hi35xx->regBase + HI35XX_I2C_INTR_EN);
    val &= ~flag;
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_INTR_EN);
}

static inline void Hi35xxI2cCfgIrq(const struct Hi35xxI2cCntlr *hi35xx, unsigned int flag)
{
    OSAL_WRITEL(flag, hi35xx->regBase + HI35XX_I2C_INTR_EN);
}


static inline void Hi35xxI2cClrIrq(const struct Hi35xxI2cCntlr *hi35xx)
{
    (void)OSAL_READL(hi35xx->regBase + HI35XX_I2C_INTR_STAT);
    OSAL_WRITEL(INTR_ALL_MASK, hi35xx->regBase + HI35XX_I2C_INTR_RAW);
}

static void Hi35xxI2cSetFreq(struct Hi35xxI2cCntlr *hi35xx)
{
    unsigned int maxFreq;
    unsigned int freq;
    unsigned int clkRate;
    unsigned int val;

    freq = hi35xx->freq;
    clkRate = hi35xx->clk;
    maxFreq = clkRate >> 1;

    if (freq > maxFreq) {
        hi35xx->freq = maxFreq;
        freq = hi35xx->freq;
    }
    if (freq <= I2C_FREQ_NORMAL) {
        val = clkRate / (freq << 1);
        OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_SCL_H);
        OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_SCL_L);
    } else {
        val = (clkRate * HI35XX_SCL_HIGH_CNT) / (freq * HI35XX_SCL_FULL_CNT);
        OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_SCL_H);
        val = (clkRate * HI35XX_SCL_LOW_CNT) / (freq * HI35XX_SCL_FULL_CNT);
        OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_SCL_L);
    }
    val = OSAL_READL(hi35xx->regBase + HI35XX_I2C_GLB);
    val &= ~GLB_SDA_HOLD_MASK;
    val |= ((0xa << GLB_SDA_HOLD_SHIFT) & GLB_SDA_HOLD_MASK);
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_GLB);
}

static inline void Hi35xxI2cSetWater(const struct Hi35xxI2cCntlr *hi35xx)
{
    OSAL_WRITEL(I2C_TXF_WATER, hi35xx->regBase + HI35XX_I2C_TX_WATER);
    OSAL_WRITEL(I2C_RXF_WATER, hi35xx->regBase + HI35XX_I2C_RX_WATER);
}

/*
 * config i2c slave addr
 */
static void Hi35xxI2cSetAddr(const struct Hi35xxI2cCntlr *hi35xx, const struct Hi35xxTransferData *td)
{
    struct I2cMsg *msg = &td->msgs[td->index];
    unsigned int addr = msg->addr;

    if (msg->flags & I2C_FLAG_ADDR_10BIT) {
        /* First byte is 11110XX0 where XX is upper 2 bits */
        addr = ((msg->addr & 0x300) << 1) | 0xf000;
        if (msg->flags & I2C_FLAG_READ) {
            addr |= 0x100;
        }
        /* Second byte is the remaining 8 bits */
        addr |= msg->addr & 0xff;
    } else {
        addr = (msg->addr & 0x7f) << 1;
        if (msg->flags & I2C_FLAG_READ) {
            addr |= 1;
        }
    }

    OSAL_WRITEL(addr, hi35xx->regBase + HI35XX_I2C_DATA1);
}

static inline void Hi35xxI2cCmdregSet(const struct Hi35xxI2cCntlr *hi35xx, unsigned int cmd, unsigned int *offset)
{
    PLAT_LOGV("%s: offset=0x%x, cmd=0x%x...", __func__, *offset * HI35XX_REG_SIZE, cmd);
    OSAL_WRITEL(cmd, hi35xx->regBase + HI35XX_I2C_CMD_BASE + *offset * HI35XX_REG_SIZE);
    (*offset)++;
}

static void Hi35xxI2cCfgCmd(const struct Hi35xxI2cCntlr *hi35xx, const struct Hi35xxTransferData *td)
{
    unsigned int offset = 0;
    struct I2cMsg *msg = &td->msgs[td->index];

    Hi35xxI2cCmdregSet(hi35xx, (td->index == 0) ? CMD_TX_S : CMD_TX_RS, &offset);

    if (msg->flags & I2C_FLAG_ADDR_10BIT) {
        Hi35xxI2cCmdregSet(hi35xx, CMD_TX_D1_2, &offset);
        if (td->index == 0) {
            Hi35xxI2cCmdregSet(hi35xx, CMD_TX_D1_1, &offset);
        }
    } else {
        Hi35xxI2cCmdregSet(hi35xx, CMD_TX_D1_1, &offset);
    }

    Hi35xxI2cCmdregSet(hi35xx, (msg->flags & I2C_FLAG_IGNORE_NO_ACK) ? CMD_IGN_ACK : CMD_RX_ACK, &offset);
    if (msg->flags & I2C_FLAG_READ) {
        if (msg->len > 1) {
            OSAL_WRITEL(offset, hi35xx->regBase + HI35XX_I2C_DST1);
            OSAL_WRITEL(msg->len - HI35XX_I2C_R_LOOP_ADJ, hi35xx->regBase + HI35XX_I2C_LOOP1);
            Hi35xxI2cCmdregSet(hi35xx, CMD_RX_FIFO, &offset);
            Hi35xxI2cCmdregSet(hi35xx, CMD_TX_ACK, &offset);
            Hi35xxI2cCmdregSet(hi35xx, CMD_JMP1, &offset);
        }
        Hi35xxI2cCmdregSet(hi35xx, CMD_RX_FIFO, &offset);
        Hi35xxI2cCmdregSet(hi35xx, CMD_TX_NACK, &offset);
    } else {
        OSAL_WRITEL(offset, hi35xx->regBase + HI35XX_I2C_DST1);
        OSAL_WRITEL(msg->len - 1, hi35xx->regBase + HI35XX_I2C_LOOP1);
        Hi35xxI2cCmdregSet(hi35xx, CMD_UP_TXF, &offset);
        Hi35xxI2cCmdregSet(hi35xx, CMD_TX_FIFO, &offset);

        Hi35xxI2cCmdregSet(hi35xx, (msg->flags & I2C_FLAG_IGNORE_NO_ACK) ? CMD_IGN_ACK : CMD_RX_ACK, &offset);
        Hi35xxI2cCmdregSet(hi35xx, CMD_JMP1, &offset);
    }

    if ((td->index == (td->count - 1)) || (msg->flags & I2C_FLAG_STOP)) {
        PLAT_LOGV("%s: TX stop, idx:%d, count:%d, flags:%d", __func__,
            td->index, td->count, msg->flags);
        Hi35xxI2cCmdregSet(hi35xx, CMD_TX_P, &offset);
    }

    Hi35xxI2cCmdregSet(hi35xx, CMD_EXIT, &offset);
}

/*
 * Start command sequence
 */
static inline void Hi35xxI2cStartCmd(const struct Hi35xxI2cCntlr *hi35xx)
{
    unsigned int val;

    val = OSAL_READL(hi35xx->regBase + HI35XX_I2C_CTRL1);
    val |= CTRL1_CMD_START_MASK;
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_CTRL1);
}

static void Hi35xxI2cRescure(const struct Hi35xxI2cCntlr *hi35xx)
{
    int index;
    unsigned int val;
    unsigned int timeCnt;

    Hi35xxI2cDisable(hi35xx);
    Hi35xxI2cCfgIrq(hi35xx, 0);
    Hi35xxI2cClrIrq(hi35xx);

    val = (0x1 << GPIO_MODE_SHIFT) | (0x1 << FORCE_SCL_OEN_SHIFT) | (0x1 << FORCE_SDA_OEN_SHIFT);
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_CTRL2);

    timeCnt = 0;
    do {
        for (index = 0; index < HI35XX_I2C_RESCUE_TIMES; index++) {
            val = (0x1 << GPIO_MODE_SHIFT) | 0x1;
            OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_CTRL2);

            OsalUDelay(HI35XX_I2C_RESCUE_DELAY);

            val = (0x1 << GPIO_MODE_SHIFT) | (0x1 << FORCE_SCL_OEN_SHIFT) | (0x1 << FORCE_SDA_OEN_SHIFT);
            OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_CTRL2);

            OsalUDelay(HI35XX_I2C_RESCUE_DELAY);
        }

        timeCnt++;
        if (timeCnt > I2C_WAIT_TIMEOUT) {
            HDF_LOGE("%s: wait Timeout!", __func__);
            goto __DISABLE_RESCURE;
        }

        val = OSAL_READL(hi35xx->regBase + HI35XX_I2C_CTRL2);
    } while (!(val & (0x1 << CHECK_SDA_IN_SHIFT)));

    val = (0x1 << GPIO_MODE_SHIFT) | (0x1 << FORCE_SCL_OEN_SHIFT) | (0x1 << FORCE_SDA_OEN_SHIFT);
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_CTRL2);

    val = (0x1 << GPIO_MODE_SHIFT) | (0x1 << FORCE_SCL_OEN_SHIFT);
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_CTRL2);

    OsalUDelay(HI35XX_I2C_RESCUE_DELAY);

    val = (0x1 << GPIO_MODE_SHIFT) | (0x1 << FORCE_SCL_OEN_SHIFT) | (0x1 << FORCE_SDA_OEN_SHIFT);
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_CTRL2);

__DISABLE_RESCURE:
    val = (0x1 << FORCE_SCL_OEN_SHIFT) | 0x1;
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_I2C_CTRL2);

    HDF_LOGE("%s: done!", __func__);
}

static int Hi35xxI2cWaitRxNoempty(const struct Hi35xxI2cCntlr *hi35xx)
{
    unsigned int timeCnt = 0;
    unsigned int val;

    do {
        val = OSAL_READL(hi35xx->regBase + HI35XX_I2C_STAT);
        if (val & STAT_RXF_NOE_MASK) {
            return 0;
        }
        OsalUDelay(HI35XX_I2C_DELAY);
    } while (timeCnt++ < I2C_TIMEOUT_COUNT);

    Hi35xxI2cRescure(hi35xx);
    HDF_LOGE("%s:wait rx no empty timeout, RIS:0x%x, SR: 0x%x",
        __func__, OSAL_READL(hi35xx->regBase + HI35XX_I2C_INTR_RAW), val);
    return HDF_ERR_IO;
}

static int Hi35xxI2cWaitTxNofull(const struct Hi35xxI2cCntlr *hi35xx)
{
    unsigned int timeCnt = 0;
    unsigned int val;

    do {
        val = OSAL_READL(hi35xx->regBase + HI35XX_I2C_STAT);
        if (val & STAT_TXF_NOF_MASK) {
            return 0;
        }
        OsalUDelay(HI35XX_I2C_DELAY);
    } while (timeCnt++ < I2C_TIMEOUT_COUNT);

    Hi35xxI2cRescure(hi35xx);
    HDF_LOGE("%s: wait rx no empty timeout, RIS: 0x%x, SR: 0x%x",
        __func__, OSAL_READL(hi35xx->regBase + HI35XX_I2C_INTR_RAW), val);
    return HDF_ERR_IO;
}

static int32_t Hi35xxI2cWaitIdle(const struct Hi35xxI2cCntlr *hi35xx)
{
    unsigned int timeCnt = 0;
    unsigned int val;

    do {
        val = OSAL_READL(hi35xx->regBase + HI35XX_I2C_INTR_RAW);
        if (val & (INTR_ABORT_MASK)) {
            HDF_LOGE("%s: wait idle abort!, RIS: 0x%x", __func__, val);
            return HDF_ERR_IO;
        }
        if (val & INTR_CMD_DONE_MASK) {
            return 0;
        }
        OsalUDelay(HI35XX_I2C_DELAY);
    } while (timeCnt++ < I2C_WAIT_TIMEOUT);

    Hi35xxI2cRescure(hi35xx);
    HDF_LOGE("%s: wait idle timeout, RIS: 0x%x, SR: 0x%x",
        __func__, val, OSAL_READL(hi35xx->regBase + HI35XX_I2C_STAT));

    return HDF_ERR_IO;
}

static int HdfCopyFromUser(void *to, const void *from, unsigned long n)
{
    int ret;
    ret = LOS_CopyToKernel(to, n, from, n);
    if (ret != LOS_OK) {
        HDF_LOGE("%s: copy from kernel fail:%d", __func__, ret);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int HdfCopyToUser(void *to, const void *from, unsigned long n)
{
    int ret;
    ret = LOS_CopyFromKernel(to, n, from, n);
    if (ret != LOS_OK) {
        HDF_LOGE("%s: copy from kernel fail:%d", __func__, ret);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t Hi35xxI2cXferOneMsgPolling(const struct Hi35xxI2cCntlr *hi35xx, const struct Hi35xxTransferData *td)
{
    int32_t status;
    uint16_t bufIdx = 0;
    uint8_t val;
    struct I2cMsg *msg = &td->msgs[td->index];

    PLAT_LOGV("%s: msg:%p, addr:%x, flags:0x%x, len=%d", __func__, msg, msg->addr, msg->flags, msg->len);

    Hi35xxI2cEnable(hi35xx);
    Hi35xxI2cDisableIrq(hi35xx, INTR_ALL_MASK);
    Hi35xxI2cClrIrq(hi35xx);
    Hi35xxI2cSetAddr(hi35xx, td);
    Hi35xxI2cCfgCmd(hi35xx, td);
    Hi35xxI2cStartCmd(hi35xx);

    if (msg->flags & I2C_FLAG_READ) {
        while (bufIdx < msg->len) {
            status = Hi35xxI2cWaitRxNoempty(hi35xx);
            if (status) {
                goto end;
            }
            val = (uint8_t)OSAL_READL(hi35xx->regBase + HI35XX_I2C_RXF);
            status = HdfCopyToUser((void *)&msg->buf[bufIdx], (void *)(uintptr_t)&val, sizeof(val));
            if (status != HDF_SUCCESS) {
                HDF_LOGE("%s: HdfCopyFromUser fail:%d", __func__, status);
                goto end;
            }
            bufIdx++;
        }
    } else {
        while (bufIdx < msg->len) {
            status = Hi35xxI2cWaitTxNofull(hi35xx);
            if (status) {
                goto end;
            }
            status = HdfCopyFromUser((void *)&val, (void *)(uintptr_t)&msg->buf[bufIdx], sizeof(val));
            if (status != HDF_SUCCESS) {
                HDF_LOGE("%s: copy to kernel fail:%d", __func__, status);
                goto end;
            }
            OSAL_WRITEL((unsigned int)val, hi35xx->regBase + HI35XX_I2C_TXF);
            bufIdx++;
        }
    }

    status = Hi35xxI2cWaitIdle(hi35xx);
end:
    Hi35xxI2cDisable(hi35xx);

    return status;
}

static void Hi35xxI2cCntlrInit(struct Hi35xxI2cCntlr *hi35xx)
{
    Hi35xxI2cHwInitCfg(hi35xx);
    Hi35xxI2cDisable(hi35xx);
    Hi35xxI2cDisableIrq(hi35xx, INTR_ALL_MASK);
    Hi35xxI2cSetFreq(hi35xx);
    Hi35xxI2cSetWater(hi35xx);
    HDF_LOGI("%s: cntlr:%u init done!", __func__, hi35xx->bus);
}

static int32_t Hi35xxI2cTransfer(struct I2cCntlr *cntlr, struct I2cMsg *msgs, int16_t count)
{
    int32_t ret = HDF_SUCCESS;
    unsigned long irqSave;
    struct Hi35xxI2cCntlr *hi35xx = NULL;
    struct Hi35xxTransferData td;

    if (cntlr == NULL || cntlr->priv == NULL) {
        HDF_LOGE("Hi35xxI2cTransfer: cntlr lor hi35xxis null!");
        return HDF_ERR_INVALID_OBJECT;
    }
    hi35xx = (struct Hi35xxI2cCntlr *)cntlr;

    if (msgs == NULL || count <= 0) {
        HDF_LOGE("Hi35xxI2cTransfer: err parms! count:%d", count);
        return HDF_ERR_INVALID_PARAM;
    }
    td.msgs = msgs;
    td.count = count;
    td.index = 0;

    irqSave = LOS_IntLock();
    while (td.index < td.count) {
        ret = Hi35xxI2cXferOneMsgPolling(hi35xx, &td);
        if (ret != 0) {
            break;
        }
        td.index++;
    }
    LOS_IntRestore(irqSave);
    return (td.index > 0) ? td.index : ret;
}

static const struct I2cMethod g_method = {
    .transfer = Hi35xxI2cTransfer,
};

static int32_t Hi35xxI2cLock(struct I2cCntlr *cntlr)
{
    struct Hi35xxI2cCntlr *hi35xx = (struct Hi35xxI2cCntlr *)cntlr;
    if (hi35xx != NULL) {
        return OsalSpinLock(&hi35xx->spin);
    }
    return HDF_SUCCESS;
}

static void Hi35xxI2cUnlock(struct I2cCntlr *cntlr)
{
    struct Hi35xxI2cCntlr *hi35xx = (struct Hi35xxI2cCntlr *)cntlr;
    if (hi35xx != NULL) {
        (void)OsalSpinUnlock(&hi35xx->spin);
    }
}

static const struct I2cLockMethod g_lockOps = {
    .lock = Hi35xxI2cLock,
    .unlock = Hi35xxI2cUnlock,
};

static int32_t Hi35xxI2cReadDrs(struct Hi35xxI2cCntlr *hi35xx, const struct DeviceResourceNode *node)
{
    int32_t ret;
    struct DeviceResourceIface *drsOps = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL) {
        HDF_LOGE("%s: invalid drs ops fail!", __func__);
        return HDF_FAILURE;
    }

    ret = drsOps->GetUint32(node, "reg_pbase", &hi35xx->regBasePhy, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regBase fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint16(node, "reg_size", &hi35xx->regSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regsize fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "freq", &hi35xx->freq, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read freq fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "irq", &hi35xx->irq, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read irq fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "clk", &hi35xx->clk, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read clk fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint16(node, "bus", (uint16_t *)&hi35xx->bus, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read bus fail!", __func__);
        return ret;
    }

    return HDF_SUCCESS;
}

static int32_t Hi35xxI2cParseAndInit(struct HdfDeviceObject *device, const struct DeviceResourceNode *node)
{
    int32_t ret;
    struct Hi35xxI2cCntlr *hi35xx = NULL;
    (void)device;

    hi35xx = (struct Hi35xxI2cCntlr *)OsalMemCalloc(sizeof(*hi35xx));
    if (hi35xx == NULL) {
        HDF_LOGE("%s: malloc hi35xx fail!", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = Hi35xxI2cReadDrs(hi35xx, node);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read drs fail! ret:%d", __func__, ret);
        goto __ERR__;
    }

    hi35xx->regBase = OsalIoRemap(hi35xx->regBasePhy, hi35xx->regSize);
    if (hi35xx->regBase == NULL) {
        HDF_LOGE("%s: ioremap regBase fail!", __func__);
        ret = HDF_ERR_IO;
        goto __ERR__;
    }

    Hi35xxI2cCntlrInit(hi35xx);

    hi35xx->cntlr.priv = (void *)node;
    hi35xx->cntlr.busId = hi35xx->bus;
    hi35xx->cntlr.ops = &g_method;
    hi35xx->cntlr.lockOps = &g_lockOps;
    (void)OsalSpinInit(&hi35xx->spin);
    ret = I2cCntlrAdd(&hi35xx->cntlr);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: add i2c controller fail:%d!", __func__, ret);
        (void)OsalSpinDestroy(&hi35xx->spin);
        goto __ERR__;
    }

#ifdef USER_VFS_SUPPORT
    (void)I2cAddVfsById(hi35xx->cntlr.busId);
#endif
    return HDF_SUCCESS;
__ERR__:
    if (hi35xx != NULL) {
        if (hi35xx->regBase != NULL) {
            OsalIoUnmap((void *)hi35xx->regBase);
            hi35xx->regBase = NULL;
        }
        OsalMemFree(hi35xx);
        hi35xx = NULL;
    }
    return ret;
}

static int32_t Hi35xxI2cInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    const struct DeviceResourceNode *childNode = NULL;

    HDF_LOGE("%s: Enter", __func__);
    if (device == NULL || device->property == NULL) {
        HDF_LOGE("%s: device or property is NULL", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    ret = HDF_SUCCESS;
    DEV_RES_NODE_FOR_EACH_CHILD_NODE(device->property, childNode) {
        ret = Hi35xxI2cParseAndInit(device, childNode);
        if (ret != HDF_SUCCESS) {
            break;
        }
    }

    return ret;
}

static void Hi35xxI2cRemoveByNode(const struct DeviceResourceNode *node)
{
    int32_t ret;
    int16_t bus;
    struct I2cCntlr *cntlr = NULL;
    struct Hi35xxI2cCntlr *hi35xx = NULL;
    struct DeviceResourceIface *drsOps = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL) {
        HDF_LOGE("%s: invalid drs ops fail!", __func__);
        return;
    }

    ret = drsOps->GetUint16(node, "bus", (uint16_t *)&bus, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read bus fail!", __func__);
        return;
    }

    cntlr = I2cCntlrGet(bus);
    if (cntlr != NULL && cntlr->priv == node) {
        I2cCntlrPut(cntlr);
        I2cCntlrRemove(cntlr);
        hi35xx = (struct Hi35xxI2cCntlr *)cntlr;
        OsalIoUnmap((void *)hi35xx->regBase);
        (void)OsalSpinDestroy(&hi35xx->spin);
        OsalMemFree(hi35xx);
    }
    return;
}

static void Hi35xxI2cRelease(struct HdfDeviceObject *device)
{
    const struct DeviceResourceNode *childNode = NULL;

    HDF_LOGI("%s: enter", __func__);

    if (device == NULL || device->property == NULL) {
        HDF_LOGE("%s: device or property is NULL", __func__);
        return;
    }

    DEV_RES_NODE_FOR_EACH_CHILD_NODE(device->property, childNode) {
        Hi35xxI2cRemoveByNode(childNode);
    }
}

struct HdfDriverEntry g_i2cDriverEntry = {
    .moduleVersion = 1,
    .Init = Hi35xxI2cInit,
    .Release = Hi35xxI2cRelease,
    .moduleName = "hi35xx_i2c_driver",
};
HDF_INIT(g_i2cDriverEntry);
