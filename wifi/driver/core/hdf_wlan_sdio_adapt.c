/*
 * Copyright (C) 2021 HiSilicon (Shanghai) Technologies CO., LIMITED.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "hdf_wlan_sdio.h"
#include "hdf_wlan_config.h"
#ifdef __KERNEL__
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/host.h>
#include <linux/pm_runtime.h>
#include <linux/random.h>
#include <linux/completion.h>
#else
#include <mmc/host.h>
#include <linux/device.h>
#include <mmc/sdio_func.h>
#include <mmc/sdio.h>
#include <mmc/card.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/delay.h>
#endif
#include "hdf_base.h"
#include "hdf_log.h"
#include "hdf_wlan_config.h"
#include "hdf_wlan_chipdriver_manager.h"

#define HDF_LOG_TAG HDF_WIFI_CORE

#define SDIO_ANY_ID (~0)

#ifdef __LITEOS__
struct sdio_device_id {
    unsigned char class;       /* Standard interface or SDIO_ANY_ID */
    unsigned short int vendor; /* Vendor or SDIO_ANY_ID */
    unsigned short int device; /* Device ID or SDIO_ANY_ID */
};
/*
 * SDIO function device driver
 */
struct sdio_driver {
    char *name;
    const struct sdio_device_id *id_table;
    int (*probe)(struct sdio_func *, const struct sdio_device_id *);
};
#else
extern int hisi_sdio_rescan(int slot);
#endif

/* which chips to detect, it's comes from the hcs */
#ifdef __KERNEL__
#define WLAN_TIMEOUT_MUTIPLE_10 10
struct completion g_wlanSdioComplete;
struct sdio_device_id g_wlanCurrentSido = {
    .class = SDIO_ANY_ID
};
static void HdfWlanWriteDriveTable(const struct sdio_device_id *ids)
{
    if (ids == NULL) {
        HDF_LOGE("%s: input para is null", __func__);
        return;
    }
    g_wlanCurrentSido.vendor = ids->vendor;
    g_wlanCurrentSido.device = ids->device;
    complete(&g_wlanSdioComplete);
    return;
}
static int32_t HdfWlanSdioProbe(struct sdio_func *func, const struct sdio_device_id *ids)
{
    (void)func;
    if (ids == NULL) {
        return HDF_FAILURE;
    }
    HDF_LOGI("%s: detected vendor=0x%x device=0x%x", __func__, ids->vendor, ids->device);
    HdfWlanWriteDriveTable(ids);
    return HDF_SUCCESS;
}

static void HdfWlanSdioRemove(struct sdio_func *func)
{
    sdio_set_drvdata(func, NULL);
    return;
}

static struct sdio_device_id g_wlanRegSdioIds[WLAN_MAX_CHIP_NUM];
static struct sdio_driver g_wlanSdioDriver = {
    .name = "wlan_sdio",
    .id_table = g_wlanRegSdioIds,
    .probe = HdfWlanSdioProbe,
    .remove = HdfWlanSdioRemove,
};

#endif

void HdfWlanGetSdioTableByConfig(void)
{
#ifdef __KERNEL__
    uint16_t chipCnt;
    struct HdfConfigWlanRoot *rootConfig = NULL;
    struct HdfConfigWlanChipList *tmpChipList = NULL;

    rootConfig = HdfWlanGetModuleConfigRoot();
    tmpChipList = &rootConfig->wlanConfig.chipList;
    if (tmpChipList->chipInstSize > WLAN_MAX_CHIP_NUM) {
        HDF_LOGE("%s: chipInstSize may cause some data loss %d", __func__, tmpChipList->chipInstSize);
    }
    /* get chip factory and init it according to the config chipName */
    for (chipCnt = 0; (chipCnt < tmpChipList->chipInstSize) && (chipCnt < WLAN_MAX_CHIP_NUM); chipCnt++) {
        g_wlanRegSdioIds[chipCnt].class = SDIO_ANY_ID;
        g_wlanRegSdioIds[chipCnt].vendor = tmpChipList->chipInst[chipCnt].chipSdio.vendorId;
        g_wlanRegSdioIds[chipCnt].device = tmpChipList->chipInst[chipCnt].chipSdio.deviceId[0];
    }
#endif
    return;
}

void HdfWlanSdioScanTriggerByBusIndex(int32_t busIdex)
{
#ifdef __KERNEL__
    if (hisi_sdio_rescan(busIdex) != HDF_SUCCESS) {
        HDF_LOGE("%s: hisi_sdio_rescan fail", __func__);
    }
    init_completion(&g_wlanSdioComplete);
    if (sdio_register_driver(&g_wlanSdioDriver) != HDF_SUCCESS) {
        HDF_LOGE("%s: sdio_register_driver fail", __func__);
        sdio_unregister_driver(&g_wlanSdioDriver);
        sdio_register_driver(&g_wlanSdioDriver);
    }
    wait_for_completion_timeout(&g_wlanSdioComplete, WLAN_TIMEOUT_MUTIPLE_10 * HZ);
#else
    hisi_sdio_rescan(busIdex);
#endif
    return;
}

void HdfWlanSdioDriverUnReg(void)
{
#ifdef __KERNEL__
    sdio_unregister_driver(&g_wlanSdioDriver);
#endif
}

#ifdef __KERNEL__
int HdfWlanGetDetectedChip(struct HdfWlanDevice *device, const struct HdfConfigWlanBus *busConfig)
{
    int32_t cnt;
    struct HdfConfigWlanChipList *tmpChipList = NULL;
    struct HdfConfigWlanRoot *rootConfig = HdfWlanGetModuleConfigRoot();
    (void)busConfig;
    if (device == NULL || rootConfig == NULL) {
        HDF_LOGE("%s: NULL ptr!", __func__);
        return HDF_FAILURE;
    }
    tmpChipList = &rootConfig->wlanConfig.chipList;
    for (cnt = 0; (cnt < tmpChipList->chipInstSize) && (cnt < WLAN_MAX_CHIP_NUM); cnt++) {
        if (tmpChipList->chipInst[cnt].chipSdio.vendorId == g_wlanCurrentSido.vendor &&
            tmpChipList->chipInst[cnt].chipSdio.deviceId[0] == g_wlanCurrentSido.device) {
            /* once detected card break */
            device->manufacturer.deviceId = tmpChipList->chipInst[cnt].chipSdio.deviceId[0];
            device->manufacturer.vendorId = tmpChipList->chipInst[cnt].chipSdio.vendorId;
            device->driverName = tmpChipList->chipInst[cnt].driverName;
            return HDF_SUCCESS;
        }
    }
    HDF_LOGE("%s: NO SDIO card detected!", __func__);
    return HDF_FAILURE;
}
#else
int HdfWlanGetDetectedChip(struct HdfWlanDevice *device, const struct HdfConfigWlanBus *busConfig)
{
    int32_t cnt;
    struct sdio_func *func = NULL;
    struct HdfConfigWlanChipList *tmpChipList = NULL;
    struct HdfConfigWlanRoot *rootConfig = HdfWlanGetModuleConfigRoot();
    if (device == NULL || busConfig == NULL || rootConfig == NULL) {
        HDF_LOGE("%s: NULL ptr!", __func__);
        return HDF_FAILURE;
    }
    tmpChipList = &rootConfig->wlanConfig.chipList;

    for (cnt = 0; (cnt < tmpChipList->chipInstSize) && (cnt < WLAN_MAX_CHIP_NUM); cnt++) {
        func = sdio_get_func(busConfig->funcNum[0], tmpChipList->chipInst[cnt].chipSdio.vendorId,
            tmpChipList->chipInst[cnt].chipSdio.deviceId[0]);
        if (func != NULL) {
            /* once detected card break */
            device->manufacturer.deviceId = tmpChipList->chipInst[cnt].chipSdio.deviceId[0];
            device->manufacturer.vendorId = tmpChipList->chipInst[cnt].chipSdio.vendorId;
            device->driverName = tmpChipList->chipInst[cnt].driverName;
            return HDF_SUCCESS;
        }
    }
    HDF_LOGE("%s: NO sdio card detected!", __func__);
    return HDF_FAILURE;
}
#endif

#ifdef __KERNEL__
#define REG_WRITE(ADDR, VALUE)                                                                     \
    do {                                                                                           \
        void __iomem *reg = ioremap(ADDR, sizeof(uint32_t));                                       \
        if (reg == NULL) {                                                                         \
            HDF_LOGE("%s:ioremap failed!addr=0x%08x", __func__, ADDR);                             \
            break;                                                                                 \
        }                                                                                          \
        HDF_LOGW("%s: Change register[0x%08x] %04x to %04x", __func__, ADDR, readl(reg), (VALUE)); \
        writel(VALUE, reg);                                                                        \
        iounmap(reg);                                                                              \
    } while (0)

#define REG_SET_BITS(ADDR, VALUE)                                                                               \
    do {                                                                                                        \
        void __iomem *reg = ioremap(ADDR, sizeof(uint32_t));                                                    \
        if (reg == NULL) {                                                                                      \
            HDF_LOGE("%s: ioremap failed!addr=0x%08x", __func__, ADDR);                                         \
            break;                                                                                              \
        }                                                                                                       \
        HDF_LOGW("%s: Change register[0x%08x] %04x to %04x", __func__, ADDR, readl(reg), readl(reg) | (VALUE)); \
        writel(readl(reg) | (VALUE), reg);                                                                      \
        iounmap(reg);                                                                                           \
    } while (0)
#else
#define REG_WRITE(ADDR, VALUE)                                                                     \
    do {                                                                                           \
        int reg = IO_DEVICE_ADDR(ADDR);                                                            \
        HDF_LOGW("%s: Change register[0x%08x] %04x to %04x", __func__, ADDR, readl(reg), (VALUE)); \
        writel(VALUE, reg);                                                                        \
    } while (0)

#define REG_SET_BITS(ADDR, VALUE)                                                                               \
    do {                                                                                                        \
        int reg = IO_DEVICE_ADDR(ADDR);                                                                         \
        HDF_LOGW("%s: Change register[0x%08x] %04x to %04x", __func__, ADDR, readl(reg), readl(reg) | (VALUE)); \
        writel(readl(reg) | (VALUE), reg);                                                                      \
    } while (0)
#endif


static int32_t ConfigHi3516DV300SDIO(uint8_t busId)
{
    if (busId == 2) {
        HDF_LOGE("%s: Config Hi3516DV300 SDIO bus %d", __func__, busId);
        const uint32_t PMC_REG_ADDR_REG0 = 0x12090000;
        const uint32_t PIN_REG_ADDR_CLK = 0x112F0008;
        const uint32_t PIN_REG_ADDR_CMD = 0x112F000C;
        const uint32_t PIN_REG_ADDR_DATA0 = 0x112F0010;
        const uint32_t PIN_REG_ADDR_DATA1 = 0x112F0014;
        const uint32_t PIN_REG_ADDR_DATA2 = 0x112F0018;
        const uint32_t PIN_REG_ADDR_DATA3 = 0x112F001C;

        REG_SET_BITS(PMC_REG_ADDR_REG0, 0x0080);
        REG_WRITE(PIN_REG_ADDR_CLK, 0x601);
        REG_WRITE(PIN_REG_ADDR_CMD, 0x501);
        REG_WRITE(PIN_REG_ADDR_DATA0, 0x501);
        REG_WRITE(PIN_REG_ADDR_DATA1, 0x501);
        REG_WRITE(PIN_REG_ADDR_DATA2, 0x501);
        REG_WRITE(PIN_REG_ADDR_DATA3, 0x501);
        return HDF_SUCCESS;
    }

    HDF_LOGE("%s: SDIO bus ID %d not supportted!", __func__, busId);
    return HDF_FAILURE;
}

static int32_t ConfigHi3518EV300SDIO(uint8_t busId)
{
    if (busId == 1) {
        HDF_LOGE("%s: Config Hi3518EV300 SDIO bus %d", __func__, busId);
        const uint32_t PIN_REG_ADDR_CLK = 0x112c0048;
        const uint32_t PIN_REG_ADDR_CMD = 0x112C004C;
        const uint32_t PIN_REG_ADDR_DATA0 = 0x112C0064;
        const uint32_t PIN_REG_ADDR_DATA1 = 0x112c0060;
        const uint32_t PIN_REG_ADDR_DATA2 = 0x112c005c;
        const uint32_t PIN_REG_ADDR_DATA3 = 0x112c0058;

        REG_WRITE(PIN_REG_ADDR_CLK, 0x1a04);
        REG_WRITE(PIN_REG_ADDR_CMD, 0x1004);
        REG_WRITE(PIN_REG_ADDR_DATA0, 0x1004);
        REG_WRITE(PIN_REG_ADDR_DATA1, 0x1004);
        REG_WRITE(PIN_REG_ADDR_DATA2, 0x1004);
        REG_WRITE(PIN_REG_ADDR_DATA3, 0x1004);
        return HDF_SUCCESS;
    }
    HDF_LOGE("%s: SDIO bus ID %d not supportted!", __func__, busId);
    return HDF_FAILURE;
}

int32_t HdfWlanConfigSDIO(uint8_t busId)
{
    struct HdfConfigWlanRoot *config = HdfWlanGetModuleConfigRoot();
    if (config == NULL || config->wlanConfig.hostChipName == NULL) {
        HDF_LOGE("%s: No config or chip name is NULL!", __func__);
        return HDF_FAILURE;
    }
    if (strcmp("hi3516dv300", config->wlanConfig.hostChipName) == 0) {
        return ConfigHi3516DV300SDIO(busId);
    }
    if (strcmp("hi3518ev300", config->wlanConfig.hostChipName) == 0) {
        return ConfigHi3518EV300SDIO(busId);
    }
    HDF_LOGE("%s: platform chip not supported!", __func__);
    return HDF_FAILURE;
}
