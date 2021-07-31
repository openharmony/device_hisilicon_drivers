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

#ifndef MDIO_H
#define MDIO_H

#include "eth_mac.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define MDIO_RWCTRL      0x1100
#define MDIO_RO_DATA     0x1104
#define U_MDIO_PHYADDR   0x0108
#define D_MDIO_PHYADDR   0x2108
#define U_MDIO_RO_STAT   0x010C
#define D_MDIO_RO_STAT   0x210C
#define U_MDIO_ANEG_CTRL 0x0110
#define D_MDIO_ANEG_CTRL 0x2110
#define U_MDIO_IRQENA    0x0114
#define D_MDIO_IRQENA    0x2114

#define MDIO_MK_RWCTL(cpuDataIn, finish, rw, phyExAddr, frqDiv, phyRegNum) \
    (((uint32_t)(cpuDataIn) << 16) |  \
        (((finish) & 0x01) << 15) |  \
        (((rw) & 0x01) << 13) |  \
        (((uint32_t)(phyExAddr) & 0x1F) << 8) |  \
        (((uint32_t)(frqDiv) & 0x7) << 5) |  \
        ((uint32_t)(phyRegNum) & 0x1F))

/* hardware set bit'15 of MDIO_REG(0) if mdio ready */
#define TestMdioReady(ld) (HiethRead(ld, MDIO_RWCTRL) & (1 << 15))

#define MdioStartPhyread(ld, phyAddr, regNum) \
    HiethWrite(ld, MDIO_MK_RWCTL(0, 0, 0, phyAddr, (ld)->mdioFrqdiv, regNum), MDIO_RWCTRL)

#define MdioGetPhyreadVal(ld) (HiethRead(ld, MDIO_RO_DATA) & 0xFFFF)

#define MdioPhyWrite(ld, phyAddr, regNum, val) \
    HiethWrite(ld, MDIO_MK_RWCTL(val, 0, 1, phyAddr, (ld)->mdioFrqdiv, regNum), MDIO_RWCTRL)

/* write mdio registers reset value */
#define MdioRegReset(ld) \
    do { \
        HiethWrite(ld, 0x00008000, MDIO_RWCTRL); \
        HiethWrite(ld, 0x00000001, U_MDIO_PHYADDR); \
        HiethWrite(ld, 0x00000001, D_MDIO_PHYADDR); \
        HiethWrite(ld, 0x04631EA9, U_MDIO_ANEG_CTRL); \
        HiethWrite(ld, 0x04631EA9, D_MDIO_ANEG_CTRL); \
        HiethWrite(ld, 0x00000000, U_MDIO_IRQENA); \
        HiethWrite(ld, 0x00000000, D_MDIO_IRQENA); \
    } while (0)

int32_t HiethMdioRead(struct HiethNetdevLocal *ld, int32_t phyAddr, int32_t regNum);
int32_t HiethMdioWrite(struct HiethNetdevLocal *ld, int32_t phyAddr, int32_t regNum, int32_t val);
int32_t HiethMdioReset(struct HiethNetdevLocal *ld);
int32_t HiethMdioInit(struct HiethNetdevLocal *ld);
void HiethMdioExit(struct HiethNetdevLocal *ld);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* MDIO_H */
