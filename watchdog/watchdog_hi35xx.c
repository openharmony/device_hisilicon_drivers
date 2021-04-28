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
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "osal_io.h"
#include "osal_mem.h"
#include "osal_spinlock.h"
#include "watchdog_core.h"
#include "watchdog_if.h"

#define HDF_LOG_TAG watchdog_hi35xx

#define HIWDT_UNLOCK_VAL    0x1ACCE551
#define HIWDT_EN_RST_INTR   0x03

#define HIWDT_LOAD      0x000
#define HIWDT_VALUE     0x004
#define HIWDT_CTRL      0x008
#define HIWDT_INTCLR    0x00C
#define HIWDT_RIS       0x010
#define HIWDT_MIS       0x014
#define HIWDT_LOCK      0xC00

struct Hi35xxWatchdog {
    struct WatchdogCntlr wdt;
    volatile unsigned char *regBase;
    uint32_t phyBase;
    uint32_t regStep;
    OsalSpinlock lock;
};

static int32_t Hi35xxWatchdogGetStatus(struct WatchdogCntlr *wdt, int32_t *status)
{
    struct Hi35xxWatchdog *hwdt = NULL;
    unsigned int ctlValue;

    if (wdt == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    hwdt = (struct Hi35xxWatchdog *)wdt;

    ctlValue = (unsigned int)OSAL_READL(hwdt->regBase + HIWDT_CTRL);
    *status = ((ctlValue & 0x01) == 0) ? WATCHDOG_STOP : WATCHDOG_START;
    return HDF_SUCCESS;
}

static int32_t Hi35xxWatchdogStart(struct WatchdogCntlr *wdt)
{
    struct Hi35xxWatchdog *hwdt = NULL;

    if (wdt == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    hwdt = (struct Hi35xxWatchdog *)wdt;
    /* unlock watchdog */
    OSAL_WRITEL(HIWDT_UNLOCK_VAL, hwdt->regBase + HIWDT_LOCK);
    /* 0x00: disable watchdog reset and interrupt */
    OSAL_WRITEL(0x00, hwdt->regBase + HIWDT_CTRL);
    /* 0x00: clear interrupt */
    OSAL_WRITEL(0x00, hwdt->regBase + HIWDT_INTCLR);
    /* 0x03: disable watchdog reset and interrupt */
    OSAL_WRITEL(HIWDT_EN_RST_INTR, hwdt->regBase + HIWDT_CTRL);
    /* write any value to lock watchdog */
    OSAL_WRITEL(0x00, hwdt->regBase + HIWDT_LOCK);
    return HDF_SUCCESS;
}

static int32_t Hi35xxWatchdogStop(struct WatchdogCntlr *wdt)
{
    struct Hi35xxWatchdog *hwdt = NULL;

    if (wdt == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    hwdt = (struct Hi35xxWatchdog *)wdt;

    /* unlock watchdog */
    OSAL_WRITEL(HIWDT_UNLOCK_VAL, hwdt->regBase + HIWDT_LOCK);
    /* 0x00: disable watchdog reset and interrupt */
    OSAL_WRITEL(0x00, hwdt->regBase + HIWDT_CTRL);
    /* 0x00: clear interrupt */
    OSAL_WRITEL(0x00, hwdt->regBase + HIWDT_INTCLR);
    /* write any value to lock watchdog */
    OSAL_WRITEL(0x00, hwdt->regBase + HIWDT_LOCK);

    return HDF_SUCCESS;
}

#define HIWDT_CLOCK_HZ (3 * 1000 * 1000)

static int32_t Hi35xxWatchdogSetTimeout(struct WatchdogCntlr *wdt, uint32_t seconds)
{
    unsigned int value;
    unsigned int maxCnt = ~0x00;
    unsigned int maxSeconds = maxCnt / HIWDT_CLOCK_HZ;
    struct Hi35xxWatchdog *hwdt = NULL;

    if (seconds == 0 || seconds > maxSeconds) {
        value = maxCnt;
    } else {
        value = seconds * HIWDT_CLOCK_HZ;
    }

    if (wdt == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    hwdt = (struct Hi35xxWatchdog *)wdt;

    /* unlock watchdog */
    OSAL_WRITEL(HIWDT_UNLOCK_VAL, hwdt->regBase + HIWDT_LOCK);
    OSAL_WRITEL(value, hwdt->regBase + HIWDT_LOAD);
    OSAL_WRITEL(value, hwdt->regBase + HIWDT_VALUE);
    /* write any value to lock watchdog */
    OSAL_WRITEL(0x00, hwdt->regBase + HIWDT_LOCK);

    return HDF_SUCCESS;
}

static int32_t Hi35xxWatchdogGetTimeout(struct WatchdogCntlr *wdt, uint32_t *seconds)
{
    unsigned int value;
    struct Hi35xxWatchdog *hwdt = NULL;

    if (wdt == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    hwdt = (struct Hi35xxWatchdog *)wdt;

    value = (unsigned int)OSAL_READL(hwdt->regBase + HIWDT_LOAD);
    *seconds = value / HIWDT_CLOCK_HZ;
    return HDF_SUCCESS;
}

static int32_t Hi35xxWatchdogFeed(struct WatchdogCntlr *wdt)
{
    struct Hi35xxWatchdog *hwdt = NULL;

    if (wdt == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    hwdt = (struct Hi35xxWatchdog *)wdt;

    /* unlock watchdog */
    OSAL_WRITEL(HIWDT_UNLOCK_VAL, hwdt->regBase + HIWDT_LOCK);
    /* 0x00: clear interrupt */
    OSAL_WRITEL(0x00, hwdt->regBase + HIWDT_INTCLR);
    /* write any value to lock watchdog */
    OSAL_WRITEL(0x00, hwdt->regBase + HIWDT_LOCK);

    return HDF_SUCCESS;
}

static struct WatchdogMethod g_method = {
    .getStatus = Hi35xxWatchdogGetStatus,
    .start = Hi35xxWatchdogStart,
    .stop = Hi35xxWatchdogStop,
    .setTimeout = Hi35xxWatchdogSetTimeout,
    .getTimeout = Hi35xxWatchdogGetTimeout,
    .feed = Hi35xxWatchdogFeed,
};

static int32_t Hi35xxWatchdogReadDrs(struct Hi35xxWatchdog *hwdt, const struct DeviceResourceNode *node)
{
    int32_t ret;
    struct DeviceResourceIface *drsOps = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetUint32 == NULL) {
        HDF_LOGE("%s: invalid drs ops!", __func__);
        return HDF_FAILURE;
    }

    ret = drsOps->GetUint32(node, "regBase", &hwdt->phyBase, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regBase fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "regStep", &hwdt->regStep, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regStep fail!", __func__);
        return ret;
    }

    return HDF_SUCCESS;
}

static int32_t Hi35xxWatchdogBind(struct HdfDeviceObject *device)
{
    int32_t ret;
    struct Hi35xxWatchdog *hwdt = NULL;

    HDF_LOGI("%s: Enter", __func__);
    if (device == NULL || device->property == NULL) {
        HDF_LOGE("%s: device or property is null!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    hwdt = (struct Hi35xxWatchdog *)OsalMemCalloc(sizeof(*hwdt));
    if (hwdt == NULL) {
        HDF_LOGE("%s: malloc hwdt fail!", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    ret = Hi35xxWatchdogReadDrs(hwdt, device->property);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read drs fail:%d", __func__, ret);
        OsalMemFree(hwdt);
        return ret;
    }

    hwdt->regBase = OsalIoRemap(hwdt->phyBase, hwdt->regStep);
    if (hwdt->regBase == NULL) {
        HDF_LOGE("%s: ioremap regbase fail!", __func__);
        OsalMemFree(hwdt);
        return HDF_ERR_IO;
    }

    hwdt->wdt.priv = (void *)device->property;
    hwdt->wdt.ops = &g_method;
    hwdt->wdt.device = device;
    ret = WatchdogCntlrAdd(&hwdt->wdt);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: err add watchdog:%d", __func__, ret);
        OsalIoUnmap((void *)hwdt->regBase);
        OsalMemFree(hwdt);
        return ret;
    }
    HDF_LOGI("%s: dev service %s init success!", __func__, HdfDeviceGetServiceName(device));
    return HDF_SUCCESS;
}

static int32_t Hi35xxWatchdogInit(struct HdfDeviceObject *device)
{
    (void)device;
    return HDF_SUCCESS;
}

static void Hi35xxWatchdogRelease(struct HdfDeviceObject *device)
{
    struct WatchdogCntlr *wdt = NULL;
    struct Hi35xxWatchdog *hwdt = NULL;

    HDF_LOGI("%s: enter", __func__);
    if (device == NULL) {
        return;
    }

    wdt = WatchdogCntlrFromDevice(device);
    if (wdt == NULL) {
        return;
    }
    WatchdogCntlrRemove(wdt);

    hwdt = (struct Hi35xxWatchdog *)wdt;
    if (hwdt->regBase != NULL) {
        OsalIoUnmap((void *)hwdt->regBase);
        hwdt->regBase = NULL;
    }
    OsalMemFree(hwdt);
}

struct HdfDriverEntry g_watchdogDriverEntry = {
    .moduleVersion = 1,
    .Bind = Hi35xxWatchdogBind,
    .Init = Hi35xxWatchdogInit,
    .Release = Hi35xxWatchdogRelease,
    .moduleName = "HDF_PLATFORM_WATCHDOG",
};
HDF_INIT(g_watchdogDriverEntry);
