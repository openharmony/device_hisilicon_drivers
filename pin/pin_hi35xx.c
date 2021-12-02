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

#include "pin_hi35xx.h"
#include "device_resource_if.h"
#include "hdf_log.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "pin_core.h"

#define HDF_LOG_TAG pin_hi35xx

#define HI35XX_PIN_FUNC_MAX  6
#define HI35XX_PIN_REG_SIZE  4

struct Hi35xxPinDesc {
    const char *pinName;
    uint32_t init;
    uint32_t index;
    int32_t pullType;
    int32_t strength;
    const char *func[HI35XX_PIN_FUNC_MAX];
};

struct Hi35xxPinCntlr {
    struct PinCntlr cntlr;
    struct Hi35xxPinDesc *desc;
    volatile unsigned char *regBase;
    uint16_t number;
    uint32_t regStartBasePhy;
    uint32_t regSize;
    uint32_t pinCount;
};

static int32_t Hi35xxPinSetPull(struct PinCntlr *cntlr, uint32_t index, enum PinPullType pullType)
{
    uint32_t value;
    struct Hi35xxPinCntlr *hi35xx = NULL;

    hi35xx = (struct Hi35xxPinCntlr *)cntlr;
    value = OSAL_READL(hi35xx->regBase + index * HI35XX_PIN_REG_SIZE);
    value = (value & ~PIN_PULL_TYPE_MASK) | ((uint32_t)pullType << PIN_PULL_TYPE_OFFSET);
    OSAL_WRITEL(value, hi35xx->regBase + index * HI35XX_PIN_REG_SIZE);

    HDF_LOGD("%s: set pin Pull success.", __func__);
    return HDF_SUCCESS;
}

static int32_t Hi35xxPinGetPull(struct PinCntlr *cntlr, uint32_t index, enum PinPullType *pullType)
{
    uint32_t value;
    struct Hi35xxPinCntlr *hi35xx = NULL;
    hi35xx = (struct Hi35xxPinCntlr *)cntlr;

    value = OSAL_READL(hi35xx->regBase + index * HI35XX_PIN_REG_SIZE);
    *pullType = (enum PinPullType)((value & PIN_PULL_TYPE_MASK) >> PIN_PULL_TYPE_OFFSET);

    HDF_LOGD("%s: get pin Pull success.", __func__);
    return HDF_SUCCESS;
}

static int32_t Hi35xxPinSetStrength(struct PinCntlr *cntlr, uint32_t index, uint32_t strength)
{
    uint32_t value;
    struct Hi35xxPinCntlr *hi35xx = NULL;

    hi35xx = (struct Hi35xxPinCntlr *)cntlr;
    value = OSAL_READL(hi35xx->regBase + index * HI35XX_PIN_REG_SIZE);
    value = (value & ~PIN_STRENGTH_MASK) | (strength << PIN_STRENGTH_OFFSET);
    OSAL_WRITEL(value, hi35xx->regBase + index * HI35XX_PIN_REG_SIZE);
    HDF_LOGD("%s: set pin Strength success.", __func__);
    return HDF_SUCCESS;
}

static int32_t Hi35xxPinGetStrength(struct PinCntlr *cntlr, uint32_t index, uint32_t *strength)
{
    uint32_t value;
    struct Hi35xxPinCntlr *hi35xx = NULL;
    hi35xx = (struct Hi35xxPinCntlr *)cntlr;

    value = OSAL_READL(hi35xx->regBase + index * HI35XX_PIN_REG_SIZE);
    *strength = (value & PIN_STRENGTH_MASK) >> PIN_STRENGTH_OFFSET;
    HDF_LOGD("%s: get pin Strength success.", __func__);
    return HDF_SUCCESS;
}

static int32_t Hi35xxPinSetFunc(struct PinCntlr *cntlr, uint32_t index, const char *funcName)
{
    uint32_t value;
    int ret;
    uint32_t funcNum;
    struct Hi35xxPinCntlr *hi35xx = NULL;

    hi35xx = (struct Hi35xxPinCntlr *)cntlr;

    for (funcNum = 0; funcNum < HI35XX_PIN_FUNC_MAX; funcNum++) {
        ret = strcmp(funcName, hi35xx->desc[index].func[funcNum]);
        if (ret == 0) {
            value = OSAL_READL(hi35xx->regBase + index * HI35XX_PIN_REG_SIZE);
            value = (value & ~PIN_FUNC_MASK) | funcNum;
            OSAL_WRITEL(value, hi35xx->regBase + index * HI35XX_PIN_REG_SIZE);
            HDF_LOGD("%s: set pin function success.", __func__);
            return HDF_SUCCESS; 
        }
    }
    HDF_LOGE("%s: set pin Function failed.", __func__);
    return HDF_ERR_IO;
}

static int32_t Hi35xxPinGetFunc(struct PinCntlr *cntlr, uint32_t index, const char **funcName)
{
    uint32_t value;
    uint32_t funcNum;
    struct Hi35xxPinCntlr *hi35xx = NULL;

    hi35xx = (struct Hi35xxPinCntlr *)cntlr;

    value = OSAL_READL(hi35xx->regBase + index * HI35XX_PIN_REG_SIZE);
    funcNum = value & PIN_FUNC_MASK;
    *funcName = hi35xx->desc[index].func[funcNum];
    HDF_LOGD("%s: get pin function success.", __func__);
    return HDF_SUCCESS;
}

static struct PinCntlrMethod g_method = {
    .SetPinPull = Hi35xxPinSetPull,
    .GetPinPull = Hi35xxPinGetPull,
    .SetPinStrength = Hi35xxPinSetStrength,
    .GetPinStrength = Hi35xxPinGetStrength,
    .SetPinFunc = Hi35xxPinSetFunc,
    .GetPinFunc = Hi35xxPinGetFunc,
};

static int32_t Hi35xxPinReadFunc(struct Hi35xxPinDesc *desc,
                                 const struct DeviceResourceNode *node,
                                 struct DeviceResourceIface *drsOps)
{
    int32_t ret;
    uint32_t funcNum = 0;
    
    ret = drsOps->GetString(node, "F0", &desc->func[funcNum], "NULL");
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read F0 failed", __func__);
        return ret;
    }

    funcNum++;
    ret = drsOps->GetString(node, "F1", &desc->func[funcNum], "NULL");
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read F1 failed", __func__);
        return ret;
    }

    funcNum++;
    ret = drsOps->GetString(node, "F2", &desc->func[funcNum], "NULL");
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read F2 failed", __func__);
        return ret;
    }

    funcNum++;
    ret = drsOps->GetString(node, "F3", &desc->func[funcNum], "NULL");
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read F3 failed", __func__);
        return ret;
    }

    funcNum++;
    ret = drsOps->GetString(node, "F4", &desc->func[funcNum], "NULL");
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read F4 failed", __func__);
        return ret;
    }

    funcNum++;
    ret = drsOps->GetString(node, "F5", &desc->func[funcNum], "NULL");
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read F5 failed", __func__);
        return ret;
    }
    HDF_LOGD("%s:Pin Read Func succe. F0:%s", __func__, desc->func[0]);

    return HDF_SUCCESS;
}

static int32_t Hi35xxPinParsePinNode(const struct DeviceResourceNode *node,
                                     struct Hi35xxPinCntlr *hi35xx,
                                     int32_t index)
{
    int32_t ret;
    struct DeviceResourceIface *drsOps = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL || drsOps->GetString == NULL) {
        HDF_LOGE("%s: invalid drs ops fail!", __func__);
        return HDF_FAILURE;
    }
    ret = drsOps->GetString(node, "pinName", &hi35xx->desc[index].pinName, "NULL");
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read pinName failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "init", &hi35xx->desc[index].init, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read init failed", __func__);
        return ret;
    }

    ret = Hi35xxPinReadFunc(&hi35xx->desc[index], node, drsOps);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s:Pin read Func failed", __func__);
        return ret;
    }
    hi35xx->cntlr.pins[index].pinName = hi35xx->desc[index].pinName;
    hi35xx->cntlr.pins[index].priv = (void *)node;
    HDF_LOGD("%s:Pin Parse Pin Node success.", __func__);
    return HDF_SUCCESS;
}

static int32_t Hi35xxPinCntlrInit(struct HdfDeviceObject *device, struct Hi35xxPinCntlr *hi35xx)
{
    struct DeviceResourceIface *drsOps = NULL;
    int32_t ret;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL || drsOps->GetUint16 == NULL) {
        HDF_LOGE("%s: invalid drs ops fail!", __func__);
        return HDF_FAILURE;
    }
    ret = drsOps->GetUint16(device->property, "number", &hi35xx->number, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read number failed", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(device->property, "regStartBasePhy", &hi35xx->regStartBasePhy, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regStartBasePhy failed", __func__);
        return ret;
    }
    ret = drsOps->GetUint32(device->property, "regSize", &hi35xx->regSize, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regSize failed", __func__);
        return ret;
    }
    ret = drsOps->GetUint32(device->property, "pinCount", &hi35xx->pinCount, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read pinCount failed", __func__);
        return ret;
    }
    hi35xx->cntlr.pinCount = hi35xx->pinCount;
    hi35xx->cntlr.number = hi35xx->number;
    hi35xx->regBase = OsalIoRemap(hi35xx->regStartBasePhy, hi35xx->regSize);
    if (hi35xx->regBase == NULL) {
        HDF_LOGE("%s: remap Pin base failed", __func__);
        return HDF_ERR_IO;
    }
    hi35xx->desc = (struct Hi35xxPinDesc *)OsalMemCalloc(sizeof(struct Hi35xxPinDesc) * hi35xx->pinCount);
    hi35xx->cntlr.pins = (struct PinDesc *)OsalMemCalloc(sizeof(struct PinDesc) * hi35xx->pinCount);
    HDF_LOGD("%s: Pin Cntlr Init success", __func__);
    return HDF_SUCCESS;
}

static int32_t Hi35xxPinBind(struct HdfDeviceObject *device)
{
    (void)device;
    HDF_LOGD("%s: success", __func__);
    return HDF_SUCCESS;
}

static int32_t Hi35xxPinInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    int32_t index;
    const struct DeviceResourceNode *childNode = NULL;
    struct Hi35xxPinCntlr *hi35xx = NULL;

    HDF_LOGI("%s: Enter", __func__);
    hi35xx = (struct Hi35xxPinCntlr *)OsalMemCalloc(sizeof(*hi35xx));
    if (hi35xx == NULL) {
        HDF_LOGE("%s: alloc hi35xx failed", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = Hi35xxPinCntlrInit(device, hi35xx);
    index = 0;

    DEV_RES_NODE_FOR_EACH_CHILD_NODE(device->property, childNode) {
        ret = Hi35xxPinParsePinNode(childNode, hi35xx, index);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        index++;
    }

    hi35xx->cntlr.method = &g_method;
    ret = PinCntlrAdd(&hi35xx->cntlr);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: add Pin cntlr: failed", __func__);
        ret = HDF_FAILURE;
    }
    HDF_LOGD("%s: Pin Init success", __func__);
    return HDF_SUCCESS;
}

static void Hi35xxPinRelease(struct HdfDeviceObject *device)
{
    int32_t ret;
    uint16_t number;
    struct PinCntlr *cntlr = NULL;
    struct Hi35xxPinCntlr *hi35xx = NULL;
    struct DeviceResourceIface *drsOps = NULL;

    HDF_LOGI("%s: Enter", __func__);
    if (device == NULL || device->property == NULL) {
        HDF_LOGE("%s: device or property is null", __func__);
        return;
    }
    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL || drsOps->GetString == NULL) {   
        HDF_LOGE("%s: invalid drs ops", __func__);
        return;
    }
   
    ret = drsOps->GetUint16(device->property, "number", &number, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read cntlr number failed", __func__);
        return;
    }

    cntlr = PinCntlrGetByNumber(number);
    PinCntlrRemove(cntlr);
    hi35xx = (struct Hi35xxPinCntlr *)cntlr;
    if (hi35xx != NULL) {
        if (hi35xx->regBase != NULL) {
            OsalIoUnmap((void *)hi35xx->regBase);
        }
        OsalMemFree(hi35xx);
    }
}

static struct HdfDriverEntry g_hi35xxPinDriverEntry = {
    .moduleVersion = 1,
    .Bind = Hi35xxPinBind,
    .Init = Hi35xxPinInit,
    .Release = Hi35xxPinRelease,
    .moduleName = "hi35xx_pin_driver",
};
HDF_INIT(g_hi35xxPinDriverEntry);