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

#include "devsvc_manager_clnt.h"
#include "eth_chip_driver.h"
#include "eth_mac.h"
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "hieth_mac.h"
#include "hieth_phy.h"
#include "osal_mem.h"

static const char* const HISI_ETHERNET_DRIVER_NAME = "hieth-sf";

static int32_t HdfEthRegHisiDriverFactory(void)
{
    static struct HdfEthChipDriverFactory tmpFactory = { 0 };
    struct HdfEthChipDriverManager *driverMgr = HdfEthGetChipDriverMgr();

    if (driverMgr == NULL && driverMgr->RegChipDriver == NULL) {
        HDF_LOGE("%s fail: driverMgr is NULL", __func__);
        return HDF_FAILURE;
    }
    tmpFactory.driverName = HISI_ETHERNET_DRIVER_NAME;
    tmpFactory.InitEthDriver = InitHiethDriver;
    tmpFactory.GetMacAddr = EthHisiRandomAddr;
    tmpFactory.DeinitEthDriver = DeinitHiethDriver;
    tmpFactory.BuildMacDriver = BuildHisiMacDriver;
    tmpFactory.ReleaseMacDriver = ReleaseHisiMacDriver;
    if (driverMgr->RegChipDriver(&tmpFactory) != HDF_SUCCESS) {
        HDF_LOGE("%s fail: driverMgr is NULL", __func__);
        return HDF_FAILURE;
    }
    HDF_LOGI("hisi eth driver register success");
    return HDF_SUCCESS;
}

static int32_t HdfEthHisiChipDriverInit(struct HdfDeviceObject *device)
{
    (void)device;
    return HdfEthRegHisiDriverFactory();
}

static int32_t HdfEthHisiDriverBind(struct HdfDeviceObject *dev)
{
    (void)dev;
    return HDF_SUCCESS;
}

static void HdfEthHisiChipRelease(struct HdfDeviceObject *object)
{
    (void)object;
}

struct HdfDriverEntry g_hdfHisiEthChipEntry = {
    .moduleVersion = 1,
    .Bind = HdfEthHisiDriverBind,
    .Init = HdfEthHisiChipDriverInit,
    .Release = HdfEthHisiChipRelease,
    .moduleName = "HDF_ETHERNET_CHIPS"
};

HDF_INIT(g_hdfHisiEthChipEntry);
