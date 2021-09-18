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

#include "eth_phy.h"
#include "linux/kernel.h"
#include "hieth.h"
#include "mdio.h"
#include "stdio.h"

#define WAIT_PHY_AUTO_NEG_TIMES 25

bool HiethGetPhyStat(struct HiethNetdevLocal *ld, EthPhyAccess *phyAccess, uint32_t *state)
{
    int32_t phyState;
    int32_t i;

    *state = 0;
    phyState = HiethMdioRead(ld, phyAccess->phyAddr, PHY_BMSR);
    if (phyState < 0) {
        return false;
    }
    if (!((uint32_t)phyState & BMSR_AN_COMPLETE)) {
        HDF_LOGE("waiting for auto-negotiation completed!");
        for (i = 0; i < WAIT_PHY_AUTO_NEG_TIMES; i++) {
            phyState = HiethMdioRead(ld, phyAccess->phyAddr, PHY_BMSR);
            if ((phyState >= 0) && ((uint32_t)phyState & BMSR_AN_COMPLETE)) {
                break;
            }
            msleep(10);
        }
    }
    if ((uint32_t)phyState & BMSR_AN_COMPLETE) {
        if ((uint32_t)phyState & BMSR_LINK) {
            *state |= ETH_PHY_STAT_LINK;
        }
        return true;
    }
    return false;
}

int32_t MiiphyLink(struct HiethNetdevLocal *ld, EthPhyAccess *phyAccess)
{
    int32_t reg;

    reg = HiethMdioRead(ld, phyAccess->phyAddr, PHY_BMSR);
    if (reg < 0) {
        HDF_LOGE("PHY_BMSR read failed, assuming no link");
        return HDF_SUCCESS;
    }

    /* Determine if a link is active */
    if (((uint32_t)reg & BMSR_LINK) != 0) {
        return HDF_FAILURE;
    } else {
        return HDF_SUCCESS;
    }
}

/*****************************************************************************
 *
 * Return 1 if PHY supports 1000BASE-X, 0 if PHY supports 10BASE-T/100BASE-TX/
 * 1000BASE-T, or on error.
 */
static int32_t MiiphyIs1000baseX(struct HiethNetdevLocal *ld, EthPhyAccess *phyAccess)
{
    int32_t reg;

    reg = HiethMdioRead(ld, phyAccess->phyAddr, PHY_EXSR);
    if (reg < 0) {
        HDF_LOGE("PHY_EXSR read failed, assume no 1000BASE-X");
        return HDF_SUCCESS;
    }
    return ((uint32_t)reg & (EXSR_1000XF | EXSR_1000XH)) != 0;
}

/*****************************************************************************
 *
 * Determine the ethernet speed (10/100/1000).  Return 10 on error.
 */
int32_t MiiphySpeed(struct HiethNetdevLocal *ld, EthPhyAccess *phyAccess)
{
    int32_t bmcr, anlpar;
    int32_t btsr, val;

    val = HiethMdioRead(ld, phyAccess->phyAddr, PHY_BMSR);
    if (val < 0) {
        HDF_LOGE("PHY 1000BT status[read PHY_BMSR]\n");
        return PHY_SPEED_10;
    }

    if ((uint32_t)val & BMSR_ESTATEN) {
        if (MiiphyIs1000baseX(ld, phyAccess)) {
            return PHY_SPEED_1000;
        }
        btsr = HiethMdioRead(ld, phyAccess->phyAddr, PHY_1000BTSR);
        if (btsr < 0) {
            HDF_LOGE("PHY 1000BT status[read PHY_1000BTSR]\n");
            return PHY_SPEED_10;
        }
        if (btsr != 0xFFFF && ((uint32_t)btsr & (PHY_1000BTSR_1000FD | PHY_1000BTSR_1000HD))) {
            return PHY_SPEED_1000;
        }
    }

    /* Check Basic Management Control Register first. */
    bmcr = HiethMdioRead(ld, phyAccess->phyAddr, PHY_BMCR);
    if (bmcr < 0) {
        HDF_LOGE("PHY speed read failed[read PHY_BMCR]");
        return PHY_SPEED_10;
    }

    if ((uint32_t)bmcr & BMCR_AN_ENABLE) {
        anlpar = HiethMdioRead(ld, phyAccess->phyAddr, PHY_ANLPAR);
        if (anlpar < 0) {
            HDF_LOGE("PHY AN speed failed[anlpar]");
            return PHY_SPEED_10;
        }
        return ((uint32_t)anlpar & ANLPAR_100) ? PHY_SPEED_100 : PHY_SPEED_10;
    }
    return ((uint32_t)bmcr & BMCR_SPEED100) ? PHY_SPEED_100 : PHY_SPEED_10;
}

/*
 * Determine full/half duplex.  Return half on error.
 */
int32_t MiiphyDuplex(struct HiethNetdevLocal *ld, EthPhyAccess *phyAccess)
{
    int32_t bmcr, anlpar, val;
    val = HiethMdioRead(ld, phyAccess->phyAddr, PHY_BMSR);
    if (val < 0) {
        HDF_LOGE("PHY duplex read failed");
        return PHY_DUPLEX_HALF;
    }

    if ((uint32_t)val & BMSR_ESTATEN) {
        int32_t btsr;
        /* Check for 1000BASE-X */
        if (MiiphyIs1000baseX(ld, phyAccess)) {
            /* 1000BASE-X */
            anlpar = HiethMdioRead(ld, phyAccess->phyAddr, PHY_ANLPAR);
            if (anlpar < 0) {
                HDF_LOGE("1000BASE-X PHY AN duplex failed");
                return PHY_DUPLEX_HALF;
            }
        }
        /* No 1000BASE-X, so assume 1000BASE-T/1000BASE-TX 10BASE-T register set */
        /* Check for 1000BASE-T. */
        btsr = HiethMdioRead(ld, phyAccess->phyAddr, PHY_1000BTSR);
        if (btsr < 0) {
            HDF_LOGE("PHY 1000BT status");
            return PHY_DUPLEX_HALF;
        }

        if (btsr != 0xFFFF) {
            if ((uint32_t)btsr & PHY_1000BTSR_1000FD) {
                return PHY_DUPLEX_FULL;
            } else if ((uint32_t)btsr & PHY_1000BTSR_1000HD) {
                return PHY_DUPLEX_HALF;
            }
        }
    }
    /* Check Basic Management Control Register first. */
    bmcr = HiethMdioRead(ld, phyAccess->phyAddr, PHY_BMCR);
    if (bmcr < 0) {
        HDF_LOGE("PHY duplex");
        return PHY_DUPLEX_HALF;
    }
    /* Check if auto-negotiation is on. */
    if ((uint32_t)bmcr & BMCR_AN_ENABLE) {
        /* Get auto-negotiation results. */
        anlpar = HiethMdioRead(ld, phyAccess->phyAddr, PHY_ANLPAR);
        if (anlpar < 0) {
            HDF_LOGE("PHY AN duplex");
            return PHY_DUPLEX_HALF;
        }
        return ((uint32_t)anlpar & (ANLPAR_10FD | ANLPAR_TXFD)) ? PHY_DUPLEX_FULL : PHY_DUPLEX_HALF;
    }
    /* Get speed from basic control settings. */
    return ((uint32_t)bmcr & BMCR_FULL_DUPLEX) ? PHY_DUPLEX_FULL : PHY_DUPLEX_HALF;
}
