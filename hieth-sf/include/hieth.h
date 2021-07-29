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

#ifndef HIETH_H
#define HIETH_H

#include <osal_spinlock.h>
#include <los_event.h>
#include "eth_device.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define HIETH_TSO_SUPPORTED
#define CONFIG_HIETH_HWQ_XMIT_DEPTH 12
#define HIETH_HWQ_TXQ_SIZE          (2 * CONFIG_HIETH_HWQ_XMIT_DEPTH)

/* Interface Mode definitions */
typedef enum {
    PHY_INTERFACE_MODE_MII,
    PHY_INTERFACE_MODE_RMII,
    PHY_INTERFACE_MODE_MAX,
} PhyInterfaceMode;

/* port */
#define UP_PORT     0
#define DOWN_PORT   1

struct TxPktInfo {
    union {
        struct {
            uint32_t dataLen : 11;
            uint32_t nfragsNum : 5;
            uint32_t protHdrLen : 4;
            uint32_t ipHdrLen : 4;
            uint32_t reserved : 2;
            uint32_t sgFlag : 1;
            uint32_t coeFlag : 1;
            uint32_t portType : 1;
            uint32_t ipVer : 1;
            uint32_t vlanFlag : 1;
            uint32_t tsoFlag : 1;
        } info;
        uint32_t val;
    } tx;
    uint32_t txAddr; /* normal pkt: data buffer, sg pkt: sg desc buffer */
    uint32_t sgDescOffset; /* TSO pkt, desc addr */
};

struct HiethNetdevLocal {
#ifdef HIETH_TSO_SUPPORTED
    uint32_t sgHead;
    uint32_t sgTail;
#endif
    char *iobase;   /* virtual io addr */
    unsigned long iobasePhys; /* physical io addr */
    uint8_t port; /* 0 => up port, 1 => down port */

#ifdef HIETH_TSO_SUPPORTED
    struct TxPktInfo *txq;
    uint32_t txqHead;
    uint32_t txqTail;
    int32_t qSize;
#endif
    int32_t txHwCnt;

    struct {
        int32_t hwXmitq;
    } depth;

#define SKB_SIZE (HIETH_MAX_FRAME_SIZE)
    uint32_t phyId;
    uint32_t mdioFrqdiv;
    int32_t linkStat;
    int32_t txBusy;

    int32_t phyMode;
    int32_t phyAddr;
    OsalSpinlock tx_lock;
    OsalSpinlock rx_lock;
};

/* hieth context */
struct HiethPlatformData {
    struct HiethNetdevLocal stNetdevLocal;
    EVENT_CB_S stEvent;
};

struct HiethPlatformData *GetHiethPlatformData(void);
struct HiethNetdevLocal *GetHiethNetDevLocal(struct EthDevice *ethDevice);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* HIETH_H */
