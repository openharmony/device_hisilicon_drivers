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

#include "net_adapter.h"
#include "ctrl.h"
#include "eth_chip_driver.h"
#include "eth_drv.h"
#include "net_device.h"

static int32_t EthSetMacAddr(struct NetDevice *netDev, void *addr)
{
    int32_t ret;

    if (netDev == NULL) {
        HDF_LOGE("%s:input is NULL!", __func__);
        return HDF_FAILURE;
    }
    ret = HiethSetHwaddr((struct EthDevice *)netDev->mlPriv, (unsigned char *)addr, ETHER_ADDR_LEN);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: HiethSetHwaddr is fail!", __func__);
    }
    return ret;
}

static NetDevTxResult EthXmit(struct NetDevice *netDev, NetBuf *netbuf)
{
    EthDrvSend((struct EthDevice *)netDev->mlPriv, netbuf);
    return NETDEV_TX_OK;
}

static void LinkStatusChanged(struct NetDevice *netDev)
{
    HiethLinkStatusChanged((struct EthDevice *)netDev->mlPriv);
}

struct NetDeviceInterFace g_ethNetDevOps = {
    .xmit = EthXmit,
    .setMacAddr = EthSetMacAddr,
    .linkStatusChanged = LinkStatusChanged,
};

int32_t EthernetInitNetdev(NetDevice *netdev)
{
    int32_t ret;

    if (netdev == NULL) {
        HDF_LOGE("%s netdev is null!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    netdev->netDeviceIf = &g_ethNetDevOps;

    ret = NetDeviceAdd(netdev);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s NetDeviceAdd return error code %d!", __func__, ret);
        return ret;
    }
    return ret;
}
