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
#include "osal_time.h"
#include "plat_log.h"

#define HDF_LOG_TAG i2c_hi35xx
#define USER_VFS_SUPPORT

#define HIBVT_I2C_DELAY      50
#define I2C_FREQ_NORMAL      100000
#define HIBVT_SCL_HIGH_CNT   36
#define HIBVT_SCL_LOW_CNT    64
#define HIBVT_SCL_FULL_CNT   100
#define HIBVT_REG_SIZE       4
#define HIBVT_I2C_R_LOOP_ADJ 2
#define HIBVT_I2C_RESCUE_TIMES 9
#define HIBVT_I2C_RESCUE_DELAY 10

struct HibvtI2cCntlr {
    struct I2cCntlr cntlr;
    volatile unsigned char  *regBase;
    uint16_t regSize;
    int16_t bus;
    uint32_t clk;
    uint32_t freq;
    uint32_t irq;
    uint32_t regBasePhy;
};

struct HibvtTransferData {
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
static inline void HibvtI2cHwInitCfg(struct HibvtI2cCntlr *hibvt)
{
    unsigned long busId = (unsigned long)hibvt->bus;

    WRITE_REG_BIT(1, I2C_CRG_CLK_OFFSET + busId, REG_CRG_I2C);
    WRITE_REG_BIT(0, I2C_CRG_RST_OFFSET + busId, REG_CRG_I2C);
}


static inline void HibvtI2cEnable(const struct HibvtI2cCntlr *hibvt)
{
    unsigned int val;

    val = OSAL_READL(hibvt->regBase + HIBVT_I2C_GLB);
    val |= GLB_EN_MASK;
    OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_GLB);
}

static inline void HibvtI2cDisable(const struct HibvtI2cCntlr *hibvt)
{
    unsigned int val;

    val = OSAL_READL(hibvt->regBase + HIBVT_I2C_GLB);
    val &= ~GLB_EN_MASK;
    OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_GLB);
}

static inline void HibvtI2cDisableIrq(const struct HibvtI2cCntlr *hibvt, unsigned int flag)
{
    unsigned int val;

    val = OSAL_READL(hibvt->regBase + HIBVT_I2C_INTR_EN);
    val &= ~flag;
    OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_INTR_EN);
}

static inline void HibvtI2cCfgIrq(const struct HibvtI2cCntlr *hibvt, unsigned int flag)
{
    OSAL_WRITEL(flag, hibvt->regBase + HIBVT_I2C_INTR_EN);
}


static inline void HibvtI2cClrIrq(const struct HibvtI2cCntlr *hibvt)
{
    unsigned int val;

    val = OSAL_READL(hibvt->regBase + HIBVT_I2C_INTR_STAT);
    OSAL_WRITEL(INTR_ALL_MASK, hibvt->regBase + HIBVT_I2C_INTR_RAW);
}

static void HibvtI2cSetFreq(struct HibvtI2cCntlr *hibvt)
{
    unsigned int maxFreq;
    unsigned int freq;
    unsigned int clkRate;
    unsigned int val;

    freq = hibvt->freq;
    clkRate = hibvt->clk;
    maxFreq = clkRate >> 1;

    if (freq > maxFreq) {
        hibvt->freq = maxFreq;
        freq = hibvt->freq;
    }
    if (freq <= I2C_FREQ_NORMAL) {
        val = clkRate / (freq << 1);
        OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_SCL_H);
        OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_SCL_L);
    } else {
        val = (clkRate * HIBVT_SCL_HIGH_CNT) / (freq * HIBVT_SCL_FULL_CNT);
        OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_SCL_H);
        val = (clkRate * HIBVT_SCL_LOW_CNT) / (freq * HIBVT_SCL_FULL_CNT);
        OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_SCL_L);
    }
    val = OSAL_READL(hibvt->regBase + HIBVT_I2C_GLB);
    val &= ~GLB_SDA_HOLD_MASK;
    val |= ((0xa << GLB_SDA_HOLD_SHIFT) & GLB_SDA_HOLD_MASK);
    OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_GLB);
}

static inline void HibvtI2cSetWater(const struct HibvtI2cCntlr *hibvt)
{
    OSAL_WRITEL(I2C_TXF_WATER, hibvt->regBase + HIBVT_I2C_TX_WATER);
    OSAL_WRITEL(I2C_RXF_WATER, hibvt->regBase + HIBVT_I2C_RX_WATER);
}

/*
 * config i2c slave addr
 */
static void HibvtI2cSetAddr(const struct HibvtI2cCntlr *hibvt, const struct HibvtTransferData *td)
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

    OSAL_WRITEL(addr, hibvt->regBase + HIBVT_I2C_DATA1);
}

static inline void HibvtI2cCmdregSet(const struct HibvtI2cCntlr *hibvt, unsigned int cmd, unsigned int *offset)
{
    PLAT_LOGV("%s: offset=0x%x, cmd=0x%x...", __func__, *offset * HIBVT_REG_SIZE, cmd);
    OSAL_WRITEL(cmd, hibvt->regBase + HIBVT_I2C_CMD_BASE + *offset * HIBVT_REG_SIZE);
    (*offset)++;
}

static void HibvtI2cCfgCmd(const struct HibvtI2cCntlr *hibvt, const struct HibvtTransferData *td)
{
    unsigned int offset = 0;
    struct I2cMsg *msg = &td->msgs[td->index];

    HibvtI2cCmdregSet(hibvt, (td->index == 0) ? CMD_TX_S : CMD_TX_RS, &offset);

    if (msg->flags & I2C_FLAG_ADDR_10BIT) {
        HibvtI2cCmdregSet(hibvt, CMD_TX_D1_2, &offset);
        if (td->index == 0) {
            HibvtI2cCmdregSet(hibvt, CMD_TX_D1_1, &offset);
        }
    } else {
        HibvtI2cCmdregSet(hibvt, CMD_TX_D1_1, &offset);
    }

    HibvtI2cCmdregSet(hibvt, (msg->flags & I2C_FLAG_IGNORE_NO_ACK) ? CMD_IGN_ACK : CMD_RX_ACK, &offset);
    if (msg->flags & I2C_FLAG_READ) {
        if (msg->len > 1) {
            OSAL_WRITEL(offset, hibvt->regBase + HIBVT_I2C_DST1);
            OSAL_WRITEL(msg->len - HIBVT_I2C_R_LOOP_ADJ, hibvt->regBase + HIBVT_I2C_LOOP1);
            HibvtI2cCmdregSet(hibvt, CMD_RX_FIFO, &offset);
            HibvtI2cCmdregSet(hibvt, CMD_TX_ACK, &offset);
            HibvtI2cCmdregSet(hibvt, CMD_JMP1, &offset);
        }
        HibvtI2cCmdregSet(hibvt, CMD_RX_FIFO, &offset);
        HibvtI2cCmdregSet(hibvt, CMD_TX_NACK, &offset);
    } else {
        OSAL_WRITEL(offset, hibvt->regBase + HIBVT_I2C_DST1);
        OSAL_WRITEL(msg->len - 1, hibvt->regBase + HIBVT_I2C_LOOP1);
        HibvtI2cCmdregSet(hibvt, CMD_UP_TXF, &offset);
        HibvtI2cCmdregSet(hibvt, CMD_TX_FIFO, &offset);

        HibvtI2cCmdregSet(hibvt, (msg->flags & I2C_FLAG_IGNORE_NO_ACK) ? CMD_IGN_ACK : CMD_RX_ACK, &offset);
        HibvtI2cCmdregSet(hibvt, CMD_JMP1, &offset);
    }

    if ((td->index == (td->count - 1)) || (msg->flags & I2C_FLAG_STOP)) {
        PLAT_LOGV("%s: TX stop, idx:%d, count:%d, flags:%d", __func__,
            td->index, td->count, msg->flags);
        HibvtI2cCmdregSet(hibvt, CMD_TX_P, &offset);
    }

    HibvtI2cCmdregSet(hibvt, CMD_EXIT, &offset);
}

/*
 * Start command sequence
 */
static inline void HibvtI2cStartCmd(const struct HibvtI2cCntlr *hibvt)
{
    unsigned int val;

    val = OSAL_READL(hibvt->regBase + HIBVT_I2C_CTRL1);
    val |= CTRL1_CMD_START_MASK;
    OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_CTRL1);
}

static void HibvtI2cRescure(const struct HibvtI2cCntlr *hibvt)
{
    int index;
    unsigned int val;
    unsigned int timeCnt;

    HibvtI2cDisable(hibvt);
    HibvtI2cCfgIrq(hibvt, 0);
    HibvtI2cClrIrq(hibvt);

    val = (0x1 << GPIO_MODE_SHIFT) | (0x1 << FORCE_SCL_OEN_SHIFT) | (0x1 << FORCE_SDA_OEN_SHIFT);
    OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_CTRL2);

    timeCnt = 0;
    do {
        for (index = 0; index < HIBVT_I2C_RESCUE_TIMES; index++) {
            val = (0x1 << GPIO_MODE_SHIFT) | 0x1;
            OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_CTRL2);

            OsalUDelay(HIBVT_I2C_RESCUE_DELAY);

            val = (0x1 << GPIO_MODE_SHIFT) | (0x1 << FORCE_SCL_OEN_SHIFT) | (0x1 << FORCE_SDA_OEN_SHIFT);
            OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_CTRL2);

            OsalUDelay(HIBVT_I2C_RESCUE_DELAY);
        }

        timeCnt++;
        if (timeCnt > I2C_WAIT_TIMEOUT) {
            HDF_LOGE("%s: wait Timeout!", __func__);
            goto __DISABLE_RESCURE;
        }

        val = OSAL_READL(hibvt->regBase + HIBVT_I2C_CTRL2);
    } while (!(val & (0x1 << CHECK_SDA_IN_SHIFT)));

    val = (0x1 << GPIO_MODE_SHIFT) | (0x1 << FORCE_SCL_OEN_SHIFT) | (0x1 << FORCE_SDA_OEN_SHIFT);
    OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_CTRL2);

    val = (0x1 << GPIO_MODE_SHIFT) | (0x1 << FORCE_SCL_OEN_SHIFT);
    OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_CTRL2);

    OsalUDelay(HIBVT_I2C_RESCUE_DELAY);

    val = (0x1 << GPIO_MODE_SHIFT) | (0x1 << FORCE_SCL_OEN_SHIFT) | (0x1 << FORCE_SDA_OEN_SHIFT);
    OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_CTRL2);

__DISABLE_RESCURE:
    val = (0x1 << FORCE_SCL_OEN_SHIFT) | 0x1;
    OSAL_WRITEL(val, hibvt->regBase + HIBVT_I2C_CTRL2);

    HDF_LOGE("%s: done!", __func__);
}

static int HibvtI2cWaitRxNoempty(const struct HibvtI2cCntlr *hibvt)
{
    unsigned int timeCnt = 0;
    unsigned int val;

    do {
        val = OSAL_READL(hibvt->regBase + HIBVT_I2C_STAT);
        if (val & STAT_RXF_NOE_MASK) {
            return 0;
        }
        OsalUDelay(HIBVT_I2C_DELAY);
    } while (timeCnt++ < I2C_TIMEOUT_COUNT);

    HibvtI2cRescure(hibvt);
    HDF_LOGE("%s:wait rx no empty timeout, RIS:0x%x, SR: 0x%x",
        __func__, OSAL_READL(hibvt->regBase + HIBVT_I2C_INTR_RAW), val);
    return HDF_ERR_IO;
}

static int HibvtI2cWaitTxNofull(const struct HibvtI2cCntlr *hibvt)
{
    unsigned int timeCnt = 0;
    unsigned int val;

    do {
        val = OSAL_READL(hibvt->regBase + HIBVT_I2C_STAT);
        if (val & STAT_TXF_NOF_MASK) {
            return 0;
        }
        OsalUDelay(HIBVT_I2C_DELAY);
    } while (timeCnt++ < I2C_TIMEOUT_COUNT);

    HibvtI2cRescure(hibvt);
    HDF_LOGE("%s: wait rx no empty timeout, RIS: 0x%x, SR: 0x%x",
        __func__, OSAL_READL(hibvt->regBase + HIBVT_I2C_INTR_RAW), val);
    return HDF_ERR_IO;
}

static int32_t HibvtI2cWaitIdle(const struct HibvtI2cCntlr *hibvt)
{
    unsigned int timeCnt = 0;
    unsigned int val;

    do {
        val = OSAL_READL(hibvt->regBase + HIBVT_I2C_INTR_RAW);
        if (val & (INTR_ABORT_MASK)) {
            HDF_LOGE("%s: wait idle abort!, RIS: 0x%x", __func__, val);
            return HDF_ERR_IO;
        }
        if (val & INTR_CMD_DONE_MASK) {
            return 0;
        }
        OsalUDelay(HIBVT_I2C_DELAY);
    } while (timeCnt++ < I2C_WAIT_TIMEOUT);

    HibvtI2cRescure(hibvt);
    HDF_LOGE("%s: wait idle timeout, RIS: 0x%x, SR: 0x%x",
        __func__, val, OSAL_READL(hibvt->regBase + HIBVT_I2C_STAT));

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

static int32_t HibvtI2cXferOneMsgPolling(const struct HibvtI2cCntlr *hibvt, const struct HibvtTransferData *td)
{
    int32_t status;
    uint16_t bufIdx = 0;
    uint8_t val;
    struct I2cMsg *msg = &td->msgs[td->index];

    PLAT_LOGV("%s: msg:%p, addr:%x, flags:0x%x, len=%d", __func__, msg, msg->addr, msg->flags, msg->len);

    HibvtI2cEnable(hibvt);
    HibvtI2cDisableIrq(hibvt, INTR_ALL_MASK);
    HibvtI2cClrIrq(hibvt);
    HibvtI2cSetAddr(hibvt, td);
    HibvtI2cCfgCmd(hibvt, td);
    HibvtI2cStartCmd(hibvt);

    if (msg->flags & I2C_FLAG_READ) {
        while (bufIdx < msg->len) {
            status = HibvtI2cWaitRxNoempty(hibvt);
            if (status) {
                goto end;
            }
            val = (uint8_t)OSAL_READL(hibvt->regBase + HIBVT_I2C_RXF);
            status = HdfCopyToUser((void *)&msg->buf[bufIdx], (void *)(uintptr_t)&val, sizeof(val));
            if (status != HDF_SUCCESS) {
                HDF_LOGE("%s: HdfCopyFromUser fail:%d", __func__, status);
                goto end;
            }
            bufIdx++;
        }
    } else {
        while (bufIdx < msg->len) {
            status = HibvtI2cWaitTxNofull(hibvt);
            if (status) {
                goto end;
            }
            status = HdfCopyFromUser((void *)&val, (void *)(uintptr_t)&msg->buf[bufIdx], sizeof(val));
            if (status != HDF_SUCCESS) {
                HDF_LOGE("%s: copy to kernel fail:%d", __func__, status);
                goto end;
            }
            OSAL_WRITEL((unsigned int)val, hibvt->regBase + HIBVT_I2C_TXF);
            bufIdx++;
        }
    }

    status = HibvtI2cWaitIdle(hibvt);
end:
    HibvtI2cDisable(hibvt);

    return status;
}

static void HibvtI2cCntlrInit(struct HibvtI2cCntlr *hibvt)
{
    HibvtI2cHwInitCfg(hibvt);
    HibvtI2cDisable(hibvt);
    HibvtI2cDisableIrq(hibvt, INTR_ALL_MASK);
    HibvtI2cSetFreq(hibvt);
    HibvtI2cSetWater(hibvt);
    HDF_LOGI("%s: cntlr:%u init done!", __func__, hibvt->bus);
}

static int32_t Hi35xxI2cTransfer(struct I2cCntlr *cntlr, struct I2cMsg *msgs, int16_t count)
{
    int32_t ret = HDF_SUCCESS;
    unsigned long irqSave;
    struct HibvtI2cCntlr *hibvt = NULL;
    struct HibvtTransferData td;

    if (cntlr == NULL || cntlr->priv == NULL) {
        HDF_LOGE("Hi35xxI2cTransfer: cntlr lor hibvtis null!");
        return HDF_ERR_INVALID_OBJECT;
    }
    hibvt = (struct HibvtI2cCntlr *)cntlr;

    if (msgs == NULL || count <= 0) {
        HDF_LOGE("Hi35xxI2cTransfer: err parms! count:%d", count);
        return HDF_ERR_INVALID_PARAM;
    }
    td.msgs = msgs;
    td.count = count;
    td.index = 0;

    irqSave = LOS_IntLock();
    while (td.index < td.count) {
        ret = HibvtI2cXferOneMsgPolling(hibvt, &td);
        if (ret != 0) {
            break;
        }
        td.index++;
    }
    LOS_IntRestore(irqSave);
    return (td.index > 0) ? td.index : ret;
}

static struct I2cMethod g_method = {
    .transfer = Hi35xxI2cTransfer,
};

static int32_t HibvtI2cReadDrs(struct HibvtI2cCntlr *hibvt, const struct DeviceResourceNode *node)
{
    int32_t ret;
    struct DeviceResourceIface *drsOps = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL) {
        HDF_LOGE("%s: invalid drs ops fail!", __func__);
        return HDF_FAILURE;
    }

    ret = drsOps->GetUint32(node, "reg_pbase", &hibvt->regBasePhy, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regBase fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint16(node, "reg_size", &hibvt->regSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regsize fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "freq", &hibvt->freq, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read freq fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "irq", &hibvt->irq, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read irq fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "clk", &hibvt->clk, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read clk fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint16(node, "bus", (uint16_t *)&hibvt->bus, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read bus fail!", __func__);
        return ret;
    }

    return HDF_SUCCESS;
}

static int32_t Hi35xxI2cParseAndInit(struct HdfDeviceObject *device, const struct DeviceResourceNode *node)
{
    int32_t ret;
    struct HibvtI2cCntlr *hibvt = NULL;
    (void)device;

    hibvt = (struct HibvtI2cCntlr *)OsalMemCalloc(sizeof(*hibvt));
    if (hibvt == NULL) {
        HDF_LOGE("%s: malloc hibvt fail!", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = HibvtI2cReadDrs(hibvt, node);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read drs fail! ret:%d", __func__, ret);
        goto __ERR__;
    }

    hibvt->regBase = OsalIoRemap(hibvt->regBasePhy, hibvt->regSize);
    if (hibvt->regBase == NULL) {
        HDF_LOGE("%s: ioremap regBase fail!", __func__);
        ret = HDF_ERR_IO;
        goto __ERR__;
    }

    HibvtI2cCntlrInit(hibvt);

    hibvt->cntlr.priv = (void *)node;
    hibvt->cntlr.busId = hibvt->bus;
    hibvt->cntlr.ops = &g_method;
    ret = I2cCntlrAdd(&hibvt->cntlr);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: add i2c controller fail:%d!", __func__, ret);
        goto __ERR__;
    }

#ifdef USER_VFS_SUPPORT
    (void)I2cAddVfsById(hibvt->cntlr.busId);
#endif
    return HDF_SUCCESS;
__ERR__:
    if (hibvt != NULL) {
        if (hibvt->regBase != NULL) {
            OsalIoUnmap((void *)hibvt->regBase);
            hibvt->regBase = NULL;
        }
        OsalMemFree(hibvt);
        hibvt = NULL;
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
    struct HibvtI2cCntlr *hibvt = NULL;
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
        hibvt = (struct HibvtI2cCntlr *)cntlr;
        OsalIoUnmap((void *)hibvt->regBase);
        OsalMemFree(hibvt);
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
