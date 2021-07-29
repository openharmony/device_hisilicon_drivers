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

#ifndef ETH_MAC_H
#define ETH_MAC_H

#include <asm/platform.h>
#include <hdf_netbuf.h>
#include <los_base.h>
#include <osal_timer.h>

#include "eth_phy.h"
#include "eth_drv.h"
#include "hieth_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define U_MAC_PORTSEL     0x0200
#define D_MAC_PORTSEL     0x2200
#define U_MAC_RO_STAT     0x0204
#define D_MAC_RO_STAT     0x2204
#define U_MAC_PORTSET     0x0208
#define D_MAC_PORTSET     0x2208
#define U_MAC_STAT_CHANGE 0x020C
#define D_MAC_STAT_CHANGE 0x220C
#define U_MAC_SET         0x0210
#define D_MAC_SET         0x2210
#define U_MAC_RX_IPGCTRL  0x0214
#define D_MAC_RX_IPGCTRL  0x2214
#define U_MAC_TX_IPGCTRL  0x0218
#define D_MAC_TX_IPGCTRL  0x2218

/* bits of UD_MAC_PORTSET and UD_MAC_RO_STAT */
#define BITS_MACSTAT MK_BITS(0, 3)

/* bits of U_MAC_PORTSEL and D_MAC_PORTSEL */
#define BITS_NEGMODE  MK_BITS(0, 1)
#define BITS_MII_MODE MK_BITS(1, 1)

/* bits of U_MAC_TX_IPGCTRL and D_MAC_TX_IPGCTRL */
#define BITS_PRE_CNT_LIMIT MK_BITS(23, 3)
#define BITS_IPG           MK_BITS(16, 7)
#define BITS_FC_INTER      MK_BITS(0, 16)

#define HIETH_SPD_100M (1 << 2)
#define HIETH_LINKED   (1 << 1)
#define HIETH_DUP_FULL 1

#define HIETH_MAX_RCV_LEN 1535
#define BITS_LEN_MAX      MK_BITS(0, 10)

#define HIETH_NEGMODE_CPUSET 1
#define HIETH_NEGMODE_AUTO   0

#define HIETH_MII_MODE  0
#define HIETH_RMII_MODE 1

struct PbufInfo {
    struct pbuf_dma_info *dmaInfo[MAX_ETH_DRV_SG];
    uint32_t sgLen;
    NetBuf *buf;
};

void NetDmaCacheInv(void *addr, uint32_t size);
void NetDmaCacheClean(void *addr, uint32_t size);

typedef struct EthRamCfg {
    struct TxPktInfo *txqInfo;
    NetBuf **rxNetbuf;
    struct PbufInfo *pbufInfo;
} EthRamCfg;

typedef struct HiethPriv {
    uint32_t vector;
    uint8_t *enaddr;
    uint32_t base;    /* Base address of device */
    EthPhyAccess *phy;
    uint32_t totalLen;
    uint8_t iterator;
    volatile EthRamCfg *ram;
    uint32_t rxFeed;
    uint32_t rxRelease;
#ifdef INT_IO_ETH_INT_SUPPORT_REQUIRED
    interrupt intr;
    handle_t intr_handle;
#endif
    OsalTimer phyTimer;
    OsalTimer monitorTimer;
    uint32_t index;   /* dev id */
} HiethPriv;

static inline int32_t IsMulticastEtherAddr(const uint8_t *addr)
{
    return 0x01 & addr[0];
}

void EthHisiRandomAddr(uint8_t *addr, int32_t len);

int32_t HiethSetMacLeadcodeCntLimit(struct HiethNetdevLocal *ld, int32_t cnt);
int32_t HiethSetMacTransIntervalBits(struct HiethNetdevLocal *ld, int32_t nbits);
int32_t HiethSetMacFcInterval(struct HiethNetdevLocal *ld, int32_t para);

int32_t HiethSetLinkStat(struct HiethNetdevLocal *ld, unsigned long mode);
int32_t HiethGetLinkStat(struct HiethNetdevLocal *ld);

int32_t HiethSetNegMode(struct HiethNetdevLocal *ld, int32_t mode);
int32_t HiethGetNegmode(struct HiethNetdevLocal *ld);

int32_t HiethSetMiiMode(struct HiethNetdevLocal *ld, int32_t mode);
void HiethSetRcvLenMax(struct HiethNetdevLocal *ld, int32_t cnt);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* ETH_MAC_H */
