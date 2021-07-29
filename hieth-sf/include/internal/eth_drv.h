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

#ifndef ETH_DRV_H
#define ETH_DRV_H

#include "net_device.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define PHY_STATE_TIME 1000
#define HIETH_MONITOR_TIME 50
#define NETNUM_IO_ETH_DRIVERS_SG_LIST_SIZE 18
#define MAX_ETH_DRV_SG NETNUM_IO_ETH_DRIVERS_SG_LIST_SIZE
#define MAX_ETH_MSG 1540

typedef int32_t (*EthCanSend)(struct EthDevice *ethDevice);
typedef void (*EthSend)(struct EthDevice *ethDevice, NetBuf *netBuf);
typedef void (*EthRecv)(struct EthDevice *ethDevice, NetBuf *netBuf);
typedef void (*EthDeliver)(struct EthDevice *ethDevice);
typedef int32_t (*EthIntVector)(struct EthDevice *ethDevice);

struct EthHwrFuns {
    /* Query - can a packet be sent? */
    EthCanSend canSend;
    /* Send a packet of data */
    EthSend send;
    /* Receive [unload] a packet of data */
    EthRecv recv;
    /* Deliver data from device to network stack */
    EthDeliver deliver;
    /* Get interrupt information from hardware driver */
    EthIntVector intVector;
};

struct EthDrvSg {
    uint32_t buf;
    uint32_t len;
};

struct EthDrvSc {
    struct EthHwrFuns *funs;
    void *driverPrivate;
    const char *devName;
    int32_t state;
};

void HiethLinkStatusChanged(struct EthDevice *ethDevice);
uint8_t HiethSetHwaddr(struct EthDevice *ethDevice, uint8_t *addr, uint8_t len);
void HiethConfigMode(struct netif *netif, uint32_t configFlags, uint8_t setBit);

int32_t HisiEthSetPhyMode(const char *phyMode);

void EthDrvSend(struct EthDevice *ethDevice, NetBuf *netBuf);
void InitEthnetDrvFun(struct EthDrvSc *drvFun);
void UnRegisterTimerFunction(struct EthDevice *ethDevice);

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN 6
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* ETH_DRV_H */
