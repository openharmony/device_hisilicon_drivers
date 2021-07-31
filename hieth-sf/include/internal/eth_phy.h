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

#ifndef ETH_PHY_H
#define ETH_PHY_H

#include "los_typedef.h"

#include "hieth_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define PHY_BMCR         0x00
#define BMCR_RESET       0x8000
#define BMCR_LOOPBACK    0x4000
#define BMCR_SPEED100    0x2000
#define BMCR_AN_ENABLE   0x1000
#define BMCR_POWER_DOWN  0x0800
#define BMCR_ISOLATE     0x0400
#define BMCR_AN_RESTART  0x0200
#define BMCR_FULL_DUPLEX 0x0100
#define BMCR_COLL_TEST   0x0080

#define PHY_BMSR         0x01
#define BMSR_100T4       0x8000
#define BMSR_100FULL     0x4000
#define BMSR_100HALF     0x2000
#define BMSR_10FULL      0x1000
#define BMSR_10HALF      0x0800
#define BMSR_ESTATEN     0x0100
#define BMSR_AN_COMPLETE 0x0020
#define BMSR_LINK        0x0004

#define PHY_ID1 0x02 /* PHY ID register 1 (high 16 bits) */
#define PHY_ID2 0x03 /* PHY ID register 2 (low 16 bits) */

#define PHY_ANLPAR   0x05 /* Auto negotiation link partner ability */
#define ANLPAR_NP    0x8000
#define ANLPAR_ACK   0x4000
#define ANLPAR_RF    0x2000
#define ANLPAR_ASYMP 0x0800
#define ANLPAR_PAUSE 0x0400
#define ANLPAR_T4    0x0200
#define ANLPAR_TXFD  0x0100
#define ANLPAR_TX    0x0080
#define ANLPAR_10FD  0x0040
#define ANLPAR_10    0x0020
#define ANLPAR_100   0x0380

#define PHY_1000BTSR        0x0A
#define PHY_1000BTSR_MSCF   0x8000
#define PHY_1000BTSR_MSCR   0x4000
#define PHY_1000BTSR_LRS    0x2000
#define PHY_1000BTSR_RRS    0x1000
#define PHY_1000BTSR_1000FD 0x0800
#define PHY_1000BTSR_1000HD 0x0400

#define PHY_EXSR    0x0F
#define EXSR_1000XF 0x8000
#define EXSR_1000XH 0x4000
#define EXSR_1000TF 0x2000
#define EXSR_1000TH 0x1000

/* The ethernet phy speed: 10Mb, 100Mb, 1Gb */
#define PHY_SPEED_10      10
#define PHY_SPEED_100     100
#define PHY_SPEED_1000    1000
#define PHY_SPEED_UNKNOWN -1

/* The ethernet phy duplex: half or full */
#define PHY_DUPLEX_HALF 0x00
#define PHY_DUPLEX_FULL 0x01

#define MAX_PHY_ADDR 31

/* Physical device access - defined by hardware instance */
typedef struct {
    bool initDone;
    void (*init)(void);
    void (*reset)(void);
    int32_t phyAddr;
    int32_t phyMode;
} EthPhyAccess;

#define ETH_PHY_STAT_LINK  0x0001   /* Link up/down */
#define ETH_PHY_STAT_100MB 0x0002   /* Connection is 100Mb/10Mb */
#define ETH_PHY_STAT_FDX   0x0004   /* Connection is full/half duplex */

bool HiethGetPhyStat(struct HiethNetdevLocal *pld, EthPhyAccess *phyAccess, uint32_t *state);
int32_t MiiphyLink(struct HiethNetdevLocal *pld, EthPhyAccess *phyAccess);
int32_t MiiphySpeed(struct HiethNetdevLocal *pld, EthPhyAccess *phyAccess);
int32_t MiiphyDuplex(struct HiethNetdevLocal *pld, EthPhyAccess *phyAccess);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* ETH_PHY_H */
