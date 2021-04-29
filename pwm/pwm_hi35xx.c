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

#include "device_resource_if.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "pwm_core.h"
#include "pwm_hi35xx.h"
#include "pwm_if.h"

struct HiPwm {
    struct PwmDev dev;
    volatile unsigned char *base;
    struct HiPwmRegs *reg;
    bool supportPolarity;
};

int32_t HiPwmSetConfig(struct PwmDev *pwm, struct PwmConfig *config)
{
    struct HiPwm *hp = (struct HiPwm *)pwm;

    if (hp == NULL || hp->reg == NULL || config == NULL) {
        HDF_LOGE("%s: hp reg or config is null", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (pwm->cfg.polarity != config->polarity && !(hp->supportPolarity)) {
        HDF_LOGE("%s: not support set pwm polarity", __func__);
        return HDF_ERR_NOT_SUPPORT;
    }

    if (config->status == PWM_DISABLE_STATUS) {
        HiPwmDisable(hp->reg);
        return HDF_SUCCESS;
    }

    if (config->polarity != PWM_NORMAL_POLARITY && config->polarity != PWM_INVERTED_POLARITY) {
        HDF_LOGE("%s: polarity %u is invalid", __func__, config->polarity);
        return HDF_ERR_INVALID_PARAM;
    }

    if (config->period < PWM_MIN_PERIOD) {
        HDF_LOGE("%s: period %u is not support, min period %u", __func__, config->period, PWM_MIN_PERIOD);
        return HDF_ERR_INVALID_PARAM;
    }
    if (config->duty < 1 || config->duty > config->period) {
        HDF_LOGE("%s: duty %u is not support, min dutyCycle 1 max dutyCycle %u",
            __func__, config->duty, config->period);
        return HDF_ERR_INVALID_PARAM;
    }

    HiPwmDisable(hp->reg);
    if (pwm->cfg.polarity != config->polarity && hp->supportPolarity) {
        HiPwmSetPolarity(hp->reg, config->polarity);
    }
    HiPwmSetPeriod(hp->reg, config->period);
    HiPwmSetDuty(hp->reg, config->duty);
    if (config->number == 0) {
        HiPwmAlwaysOutput(hp->reg);
    } else {
        HiPwmOutputNumberSquareWaves(hp->reg, config->number);
    }
    return HDF_SUCCESS;
}

struct PwmMethod g_pwmOps = {
    .setConfig = HiPwmSetConfig,
};

static void HiPwmRemove(struct HiPwm *hp)
{
    if (hp->base != NULL) {
        OsalIoUnmap((void *)hp->base);
    }
    OsalMemFree(hp);
}

static int32_t HiPwmProbe(struct HiPwm *hp, struct HdfDeviceObject *obj)
{
    uint32_t tmp;
    struct DeviceResourceIface *iface = NULL;

    iface = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (iface == NULL || iface->GetUint32 == NULL) {
        HDF_LOGE("%s: face is invalid", __func__);
        return HDF_FAILURE;
    }

    if (iface->GetUint32(obj->property, "num", &(hp->dev.num), 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read num fail", __func__);
        return HDF_FAILURE;
    }

    if (iface->GetUint32(obj->property, "base", &tmp, 0) != HDF_SUCCESS) {
        HDF_LOGE("%s: read base fail", __func__);
        return HDF_FAILURE;
    }

    hp->base = OsalIoRemap(tmp, sizeof(struct HiPwmRegs));
    if (hp->base == NULL) {
        HDF_LOGE("%s: OsalIoRemap fail", __func__);
        return HDF_FAILURE;
    }

    hp->reg = (struct HiPwmRegs *)hp->base;
    hp->supportPolarity = false;
    hp->dev.method = &g_pwmOps;
    hp->dev.cfg.duty = PWM_DEFAULT_DUTY_CYCLE;
    hp->dev.cfg.period = PWM_DEFAULT_PERIOD;
    hp->dev.cfg.polarity = PWM_DEFAULT_POLARITY;
    hp->dev.cfg.status = PWM_DISABLE_STATUS;
    hp->dev.cfg.number = 0;
    hp->dev.busy = false;
    if (PwmDeviceAdd(obj, &(hp->dev)) != HDF_SUCCESS) {
        OsalIoUnmap((void *)hp->base);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t HdfPwmBind(struct HdfDeviceObject *obj)
{
    (void)obj;
    return HDF_SUCCESS;
}

static int32_t HdfPwmInit(struct HdfDeviceObject *obj)
{
    int ret;
    struct HiPwm *hp = NULL;

    HDF_LOGI("%s: entry", __func__);
    if (obj == NULL) {
        HDF_LOGE("%s: obj is null", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    hp = (struct HiPwm *)OsalMemCalloc(sizeof(*hp));
    if (hp == NULL) {
        HDF_LOGE("%s: OsalMemCalloc hp error", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = HiPwmProbe(hp, obj);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: error probe, ret is %d", __func__, ret);
        OsalMemFree(hp);
    }
    return ret;
}

static void HdfPwmRelease(struct HdfDeviceObject *obj)
{
    struct HiPwm *hp = NULL;

    HDF_LOGI("%s: entry", __func__);
    if (obj == NULL) {
        HDF_LOGE("%s: obj is null", __func__);
        return;
    }
    hp = (struct HiPwm *)obj->service;
    if (hp == NULL) {
        HDF_LOGE("%s: hp is null", __func__);
        return;
    }
    PwmDeviceRemove(obj, &(hp->dev));
    HiPwmRemove(hp);
}

struct HdfDriverEntry g_hdfPwm = {
    .moduleVersion = 1,
    .moduleName = "HDF_PLATFORM_PWM",
    .Bind = HdfPwmBind,
    .Init = HdfPwmInit,
    .Release = HdfPwmRelease,
};

HDF_INIT(g_hdfPwm);
