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

#include "hieth_phy.h"
#include "ctrl.h"
#include "eth_phy.h"
#include <linux/delay.h>
#include "mdio.h"
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "net_adapter.h"

#define WAIT_LINK_UP_TIMES      100
#define WAIT_PHY_AUTO_NEG_TIMES 25
#define PRIV_DATA_VECTOR    0x40
#define PRIV_DATA_BASE      0x88010000
#define ETH_PHY_STAT_LINK   0x0001  /* Link up/down */

int32_t CreateHiethPrivData(HiethPriv *pstPrivData)
{
    pstPrivData->phy = (EthPhyAccess *)OsalMemCalloc(sizeof(EthPhyAccess));
    if (pstPrivData->phy == NULL) {
        HDF_LOGE("%s fail : EthPhyAccess OsalMemCalloc is fail!", __func__);
        goto PRIVDATA_PHY_INIT_FAIL;
    }
    pstPrivData->phy->initDone = false;
    pstPrivData->phy->init = NULL;
    pstPrivData->phy->reset = NULL;

    pstPrivData->ram = (EthRamCfg *)OsalMemCalloc(sizeof(EthRamCfg));
    if (pstPrivData->ram == NULL) {
        HDF_LOGE("%s fail : EthRamCfg OsalMemCalloc is fail!", __func__);
        goto PRIVDATA_RAM_INIT_FAIL;
    }
    pstPrivData->ram->txqInfo = OsalMemCalloc(HIETH_HWQ_TXQ_SIZE * sizeof(struct TxPktInfo));
    if (!pstPrivData->ram->txqInfo) {
        HDF_LOGE("%s fail : TxPktInfo OsalMemCalloc is fail!", __func__);
        goto PRIVDATA_TXQ_INFO_INIT_FAIL;
    }
    pstPrivData->ram->rxNetbuf = OsalMemCalloc(HIETH_HWQ_RXQ_DEPTH * sizeof(NetBuf *));
    if (!pstPrivData->ram->rxNetbuf) {
        HDF_LOGE("%s fail : netBuf OsalMemCalloc is fail!", __func__);
        goto PRIVDATA_NETBUFF_INIT_FAIL;
    }

    pstPrivData->ram->pbufInfo = OsalMemCalloc(HIETH_HWQ_TXQ_SIZE * sizeof(struct PbufInfo));
    if (!pstPrivData->ram->pbufInfo) {
        HDF_LOGE("%s fail : PbufInfo OsalMemCalloc is fail!", __func__);
        goto PRIVDATA_PBUF_INFO_INIT_FAIL;
    }
    return HDF_SUCCESS;

PRIVDATA_PBUF_INFO_INIT_FAIL:
    OsalMemFree((void *)pstPrivData->ram->rxNetbuf);
PRIVDATA_NETBUFF_INIT_FAIL:
    OsalMemFree((void *)pstPrivData->ram->txqInfo);
PRIVDATA_TXQ_INFO_INIT_FAIL:
    OsalMemFree((void *)pstPrivData->ram);
PRIVDATA_RAM_INIT_FAIL:
    OsalMemFree((void *)pstPrivData->phy);
PRIVDATA_PHY_INIT_FAIL:
    OsalMemFree((void *)pstPrivData);
    return HDF_FAILURE;
}

int32_t InitHiethDriver(struct EthDevice *ethDevice)
{
    int32_t ret;
    HiethPriv *pstPrivData = NULL;
    if (ethDevice == NULL) {
        HDF_LOGE("%s input is NULL!", __func__);
        return HDF_FAILURE;
    }
    ret = EthernetInitNetdev(ethDevice->netdev);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s failed to init ethernet netDev", __func__);
        return HDF_FAILURE;
    }

    pstPrivData = (HiethPriv *)OsalMemCalloc(sizeof(HiethPriv));
    if (pstPrivData == NULL) {
        HDF_LOGE("%s fail : HiethPriv OsalMemCalloc is fail!", __func__);
        return HDF_FAILURE;
    }
    pstPrivData->index = 0;
    pstPrivData->vector = PRIV_DATA_VECTOR;
    pstPrivData->base = PRIV_DATA_BASE;

    if (CreateHiethPrivData(pstPrivData) != HDF_SUCCESS) {
        HDF_LOGE("%s fail : CreateHiethPrivData is fail!", __func__);
        return HDF_FAILURE;
    }
    struct EthDrvSc *pstDrvInfo = (struct EthDrvSc *)OsalMemCalloc(sizeof(struct EthDrvSc));
    if (pstDrvInfo == NULL) {
        HDF_LOGE("%s fail : EthDrvSc OsalMemCalloc error!", __func__);
        return HDF_FAILURE;
    }
    pstDrvInfo->devName = "eth1";
    pstDrvInfo->driverPrivate = (void *)pstPrivData;
    InitEthnetDrvFun(pstDrvInfo);
    ethDevice->priv = pstDrvInfo;
    ret = HiethInit(ethDevice);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s fail : HiethHwInit error!", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

int32_t DeinitHiethDriver(struct EthDevice *ethDevice)
{
    if (ethDevice == NULL) {
        HDF_LOGE("%s input ethDevice is NULL!", __func__);
        return HDF_FAILURE;
    }

    struct EthDrvSc *drvInfo = (struct EthDrvSc *)(ethDevice->priv);
    if (drvInfo == NULL) {
        return HDF_SUCCESS;
    }
    HiethPriv *priv = (HiethPriv *)drvInfo->driverPrivate;
    if (priv == NULL) {
        OsalMemFree(drvInfo);
        return HDF_SUCCESS;
    }
    UnRegisterTimerFunction(ethDevice);

    if (priv->phy != NULL) {
        OsalMemFree((void *)priv->phy);
    }
    if (priv->ram != NULL) {
        if (priv->ram->txqInfo != NULL) {
            OsalMemFree((void *)priv->ram->txqInfo);
        }
        if (priv->ram->rxNetbuf != NULL) {
            OsalMemFree((void *)priv->ram->rxNetbuf);
        }
        if (priv->ram->pbufInfo != NULL) {
            OsalMemFree((void *)priv->ram->pbufInfo);
        }
        OsalMemFree((void *)priv->ram);
    }
    OsalMemFree(priv);
    OsalMemFree(drvInfo);
    return HDF_SUCCESS;
}
