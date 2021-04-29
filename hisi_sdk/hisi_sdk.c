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

#include "hdf_base.h"
#include "hdf_device_desc.h"
#include "hdf_log.h"

extern void SDK_init(void);
extern void SDK_init2(void);

static int32_t HisiSdkBind(struct HdfDeviceObject *device)
{
    static struct IDeviceIoService service;
    if (device == NULL) {
        HDF_LOGE("%s: device is null!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    device->service = &service;
    HDF_LOGI("%s: calling SDK_init form HDF", __func__);
    SDK_init();
    HDF_LOGI("%s: SDK service init done!", __func__);
    return HDF_SUCCESS;
}

static int32_t HisiSdkInit(struct HdfDeviceObject *device)
{
    (void)device;
    return HDF_SUCCESS;
}

static void HisiSdkRelease(struct HdfDeviceObject *device)
{
    (void)device;
}

struct HdfDriverEntry g_hisiSdkEntry = {
    .moduleVersion = 1,
    .Bind = HisiSdkBind,
    .Init = HisiSdkInit,
    .Release = HisiSdkRelease,
    .moduleName = "HDF_PLATFORM_HISI_SDK",
};
HDF_INIT(g_hisiSdkEntry);

static int32_t HisiSdkBind2(struct HdfDeviceObject *device)
{
    static struct IDeviceIoService service;
    if (device == NULL) {
        HDF_LOGE("%s: device is null!", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }
    device->service = &service;
    HDF_LOGI("%s: calling SDK_init2 form HDF", __func__);
    SDK_init2();
    HDF_LOGI("%s: SDK service init done!", __func__);
    return HDF_SUCCESS;
}

static int32_t HisiSdkInit2(struct HdfDeviceObject *device)
{
    (void)device;
    return HDF_SUCCESS;
}

static void HisiSdkRelease2(struct HdfDeviceObject *device)
{
    (void)device;
}

struct HdfDriverEntry g_hisiSdkEntry2 = {
    .moduleVersion = 1,
    .Bind = HisiSdkBind2,
    .Init = HisiSdkInit2,
    .Release = HisiSdkRelease2,
    .moduleName = "HDF_PLATFORM_HISI_SDK2",
};
HDF_INIT(g_hisiSdkEntry2);
