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

#include "adc_hi35xx.h"
#include "adc_core.h"
#include "asm/platform.h"
#include "device_resource_if.h"
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "los_hwi.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "osal_time.h"
#include "plat_log.h"

#define HDF_LOG_TAG adc_hi35xx
#define HI35XX_ADC_READ_DELAY    10

struct Hi35xxAdcDevice {
    struct AdcDevice device;
    volatile unsigned char *regBase;
    volatile unsigned char *pinCtrlBase;
    uint32_t regBasePhy;
    uint32_t regSize;
    uint32_t deviceNum;
    uint32_t dataWidth;
    uint32_t validChannel;
    uint32_t scanMode;
    uint32_t delta;
    uint32_t deglitch;
    uint32_t glitchSample;
    uint32_t rate;
};

static inline void Hi35xxAdcSetIrq(const struct Hi35xxAdcDevice *hi35xx)
{
    OSAL_WRITEL(0, hi35xx->regBase + HI35XX_ADC_INTR_EN);
}

static void Hi35xxAdcSetAccuracy(const struct Hi35xxAdcDevice *hi35xx)
{
    uint32_t dataWidth;
    uint32_t val;

    if (hi35xx->dataWidth != 0 && hi35xx->dataWidth <= MAX_DATA_WIDTH) {
        dataWidth = hi35xx->dataWidth;
    } else {
        dataWidth = DEFAULT_DATA_WIDTH;
    }

    val = ((DATA_WIDTH_MASK << (MAX_DATA_WIDTH - dataWidth)) & DATA_WIDTH_MASK);
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_ADC_ACCURACY);   
}

static void Hi35xxAdcSetGlitchSample(const struct Hi35xxAdcDevice *hi35xx)
{
    uint32_t val;

    if (hi35xx->scanMode == CYCLE_MODE) {
        val = (hi35xx->glitchSample == CYCLE_MODE) ? DEFAULT_GLITCHSAMPLE : hi35xx->glitchSample;
        OSAL_WRITEL(val, hi35xx->regBase + HI35XX_ADC_START);
        val = OSAL_READL(hi35xx->regBase + HI35XX_ADC_START);
        HDF_LOGD("%s: glichSample reg val:%x", __func__, val);
    }
}

static void Hi35xxAdcSetTimeScan(const struct Hi35xxAdcDevice *hi35xx)
{
    uint32_t timeScan;
    uint32_t rate;

    rate = hi35xx->rate;
    if (hi35xx->scanMode == CYCLE_MODE) {
        timeScan = (TIME_SCAN_CALCULATOR / rate);
        if (timeScan < TIME_SCAN_MINIMUM) {
            timeScan = TIME_SCAN_MINIMUM;
        }
        OSAL_WRITEL(timeScan, hi35xx->regBase + HI35XX_ADC_START);
        timeScan = OSAL_READL(hi35xx->regBase + HI35XX_ADC_START);
        HDF_LOGD("%s: tiemScan reg val:%x", __func__, timeScan);
    }
}

static void Hi35xxAdcConfig(const struct Hi35xxAdcDevice *hi35xx)
{
    uint32_t validChannel;
    uint32_t scanMode;
    uint32_t delta;
    uint32_t deglitch;
    uint32_t val;

    validChannel = hi35xx->validChannel;
    scanMode = hi35xx->scanMode;
    delta = hi35xx->delta;
    deglitch = hi35xx->deglitch;

    if (scanMode == CYCLE_MODE) {
        val = (delta & DELTA_MASK) << DELTA_OFFSET;
        val |= ((~deglitch) & 1) << DEGLITCH_OFFSET;
    }
    val = ((~scanMode) & 1) << SCAN_MODE_OFFSET;
    val |= (validChannel & VALID_CHANNEL_MASK) << VALID_CHANNEL_OFFSET;
    OSAL_WRITEL(val, hi35xx->regBase + HI35XX_ADC_CONFIG);
}

static inline void Hi35xxAdcStartScan(const struct Hi35xxAdcDevice *hi35xx)
{
    OSAL_WRITEL(1, hi35xx->regBase + HI35XX_ADC_START);
}

static inline void Hi35xxAdcReset(const struct Hi35xxAdcDevice *hi35xx)
{
    OSAL_WRITEL(CONFIG_REG_RESET_VALUE, hi35xx->regBase + HI35XX_ADC_CONFIG);
}

static void Hi35xxAdcSetPinCtrl(const struct Hi35xxAdcDevice *hi35xx)
{
    uint32_t val;
    uint32_t validChannel;

    validChannel = (hi35xx->validChannel & VALID_CHANNEL_MASK);
    if ((validChannel & 0x1) == 1) {
        val = OSAL_READL(hi35xx->pinCtrlBase + HI35XX_ADC_IO_CONFIG_0);
        val &= ~PINCTRL_MASK;
        OSAL_WRITEL(val, hi35xx->pinCtrlBase + HI35XX_ADC_IO_CONFIG_0);
    }
    validChannel = validChannel >> 1;
    if ((validChannel & 0x1) == 1) {
        val = OSAL_READL(hi35xx->pinCtrlBase + HI35XX_ADC_IO_CONFIG_1);
        val &= ~PINCTRL_MASK;
        OSAL_WRITEL(val, hi35xx->pinCtrlBase + HI35XX_ADC_IO_CONFIG_1);
    }
}

static inline int32_t Hi35xxAdcStart(struct AdcDevice *device)
{
    struct Hi35xxAdcDevice *hi35xx = NULL;

    if (device == NULL || device->priv == NULL) {
        HDF_LOGE("%s: device or priv is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    hi35xx = (struct Hi35xxAdcDevice *)device;
    if (hi35xx->scanMode == CYCLE_MODE) {
        Hi35xxAdcStartScan(hi35xx);
    }
    return HDF_SUCCESS;
}

static int32_t Hi35xxAdcRead(struct AdcDevice *device, uint32_t channel, uint32_t *val)
{
    uint32_t value;
    uint32_t dataWidth;
    struct Hi35xxAdcDevice *hi35xx = NULL;

    if (device == NULL || device->priv == NULL) {
        HDF_LOGE("%s: device or priv is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    hi35xx = (struct Hi35xxAdcDevice *)device;
    Hi35xxAdcStartScan(hi35xx);

    if (hi35xx->scanMode != CYCLE_MODE) {
        OsalUDelay(HI35XX_ADC_READ_DELAY);
    }

    switch (channel) {
        case 0:
            value = OSAL_READL(hi35xx->regBase + HI35XX_ADC_DATA0);
            break;
        case 1:
            value = OSAL_READL(hi35xx->regBase + HI35XX_ADC_DATA1);
            break;
        default:
            value = 0;
            HDF_LOGE("%s: invalid channel:%u", __func__, channel);
            return HDF_ERR_INVALID_PARAM;
    }

    dataWidth = hi35xx->dataWidth;
    value = value >> (MAX_DATA_WIDTH - dataWidth);
    *val = value;
    return HDF_SUCCESS;
}

static inline int32_t Hi35xxAdcStop(struct AdcDevice *device)
{
    struct Hi35xxAdcDevice *hi35xx = NULL;

    if (device == NULL || device->priv == NULL) {
        HDF_LOGE("%s: device or priv is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    hi35xx = (struct Hi35xxAdcDevice *)device;
    if (hi35xx->scanMode == CYCLE_MODE) {
        OSAL_WRITEL(1, hi35xx->regBase + HI35XX_ADC_STOP);
    }
    return HDF_SUCCESS;
}

static const struct AdcMethod g_method = {
    .read = Hi35xxAdcRead,
    .stop = Hi35xxAdcStop,
    .start = Hi35xxAdcStart,
};

static void Hi35xxAdcDeviceInit(struct Hi35xxAdcDevice *hi35xx)
{
    Hi35xxAdcReset(hi35xx);
    Hi35xxAdcConfig(hi35xx);
    Hi35xxAdcSetAccuracy(hi35xx);
    Hi35xxAdcSetIrq(hi35xx);
    Hi35xxAdcSetGlitchSample(hi35xx);
    Hi35xxAdcSetTimeScan(hi35xx);
    Hi35xxAdcSetPinCtrl(hi35xx);
    HDF_LOGI("%s: device:%u init done", __func__, hi35xx->deviceNum);
}

static int32_t Hi35xxAdcReadDrs(struct Hi35xxAdcDevice *hi35xx, const struct DeviceResourceNode *node)
{
    int32_t ret;
    struct DeviceResourceIface *drsOps = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL) {
        HDF_LOGE("%s: invalid drs ops", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    ret = drsOps->GetUint32(node, "regBasePhy", &hi35xx->regBasePhy, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regBasePhy failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "regSize", &hi35xx->regSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regSize failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "deviceNum", &hi35xx->deviceNum, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read deviceNum failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "dataWidth", &hi35xx->dataWidth, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read dataWidth failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "validChannel", &hi35xx->validChannel, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read validChannel failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "scanMode", &hi35xx->scanMode, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read scanMode failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "delta", &hi35xx->delta, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read delta failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "deglitch", &hi35xx->deglitch, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read deglitch failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "glitchSample", &hi35xx->glitchSample, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read glitchSample failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "rate", &hi35xx->rate, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read rate failed", __func__);
        return ret;
    }
    return HDF_SUCCESS;
}

static int32_t Hi35xxAdcParseInit(struct HdfDeviceObject *device, struct DeviceResourceNode *node)
{
    int32_t ret;
    struct Hi35xxAdcDevice *hi35xx = NULL;
    (void)device;

    hi35xx = (struct Hi35xxAdcDevice *)OsalMemCalloc(sizeof(*hi35xx));
    if (hi35xx == NULL) {
        HDF_LOGE("%s: alloc hi35xx failed", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = Hi35xxAdcReadDrs(hi35xx, node);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read drs failed:%d", __func__, ret);
        goto __ERR__;
    }

    hi35xx->regBase = OsalIoRemap(hi35xx->regBasePhy, hi35xx->regSize);
    if (hi35xx->regBase == NULL) {
        HDF_LOGE("%s: remap regbase failed", __func__);
        ret = HDF_ERR_IO;
        goto __ERR__;
    }

    hi35xx->pinCtrlBase = OsalIoRemap(HI35XX_ADC_IO_CONFIG_BASE, HI35XX_ADC_IO_CONFIG_SIZE);
    if (hi35xx->pinCtrlBase == NULL) {
        HDF_LOGE("%s: remap pinctrl base failed", __func__);
        ret = HDF_ERR_IO;
        goto __ERR__;
    }

    Hi35xxAdcDeviceInit(hi35xx);
    hi35xx->device.priv = (void *)node;
    hi35xx->device.devNum = hi35xx->deviceNum;
    hi35xx->device.ops = &g_method;
    ret = AdcDeviceAdd(&hi35xx->device);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: add adc device:%u failed", __func__, hi35xx->deviceNum);
        goto __ERR__;
    }
    return HDF_SUCCESS;

__ERR__:
    if (hi35xx != NULL) {
        if (hi35xx->regBase != NULL) {
            OsalIoUnmap((void *)hi35xx->regBase);
            hi35xx->regBase = NULL;
        }
        AdcDeviceRemove(&hi35xx->device);
        OsalMemFree(hi35xx);
    }
    return ret;
}

static int32_t Hi35xxAdcInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct DeviceResourceNode *childNode = NULL;

    HDF_LOGI("%s: Enter", __func__);
    if (device == NULL || device->property == NULL) {
        HDF_LOGE("%s: device or property is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    ret = HDF_SUCCESS;
    DEV_RES_NODE_FOR_EACH_CHILD_NODE(device->property, childNode) {
        ret = Hi35xxAdcParseInit(device, childNode);
        if (ret != HDF_SUCCESS) {
            break;
        }
    }
    return ret;
}

static void Hi35xxAdcRemoveByNode(const struct DeviceResourceNode *node)
{
    int32_t ret;
    int32_t deviceNum;
    struct AdcDevice *device = NULL;
    struct Hi35xxAdcDevice *hi35xx = NULL;
    struct DeviceResourceIface *drsOps = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL) {
        HDF_LOGE("%s: invalid drs ops", __func__);
        return;
    }

    ret = drsOps->GetUint32(node, "deviceNum", (uint32_t *)&deviceNum, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read deviceNum failed", __func__);
        return;
    }

    device = AdcDeviceGet(deviceNum);
    if (device != NULL && device->priv == node) {
        AdcDevicePut(device);
        AdcDeviceRemove(device);
        hi35xx = (struct Hi35xxAdcDevice *)device;
        OsalIoUnmap((void *)hi35xx->regBase);
        OsalMemFree(hi35xx);
    }
    return;
}

static void Hi35xxAdcRelease(struct HdfDeviceObject *device)
{
    const struct DeviceResourceNode *childNode = NULL;

    HDF_LOGI("%s: enter", __func__);
    if (device == NULL || device->property == NULL) {
        HDF_LOGE("%s: device or property is null", __func__);
        return;
    }
    DEV_RES_NODE_FOR_EACH_CHILD_NODE(device->property, childNode) {
        Hi35xxAdcRemoveByNode(childNode);
    }
}

static struct HdfDriverEntry g_hi35xxAdcDriverEntry = {
    .moduleVersion = 1,
    .Init = Hi35xxAdcInit,
    .Release = Hi35xxAdcRelease,
    .moduleName = "hi35xx_adc_driver",
};
HDF_INIT(g_hi35xxAdcDriverEntry);
