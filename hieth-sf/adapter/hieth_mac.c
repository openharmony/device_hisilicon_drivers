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

#include "hieth_mac.h"
#include "ctrl.h"

void HiethMacCoreInit(void)
{
    uint32_t v;

    READ_UINT32(v, HIETH_CRG_IOBASE);
    v |= ETH_CORE_CLK_SELECT_54M;
    v |= (0x1 << 1); /* enable clk */
    WRITE_UINT32(v, HIETH_CRG_IOBASE);

    /* set reset bit */
    READ_UINT32(v, HIETH_CRG_IOBASE);
    v |= 0x1;
    WRITE_UINT32(v, HIETH_CRG_IOBASE);

    LOS_Udelay(DELAY_TIME_MEDIUM);

    /* clear reset bit */
    READ_UINT32(v, HIETH_CRG_IOBASE);
    v &= ~(0x1);
    WRITE_UINT32(v, HIETH_CRG_IOBASE);
}

int32_t HiethPortReset(struct EthDevice *ethDevice)
{
    struct HiethNetdevLocal *ld = GetHiethNetDevLocal(ethDevice);
    if (ld == NULL) {
        HDF_LOGE("%s: get ld fail!", __func__);
        return HDF_FAILURE;
    }

    /* soft reset: sf ip need reset twice */
    if (ld->port == UP_PORT) {
        /* Note: sf ip need reset twice */
        HiethWritelBits(ld, 1, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_ALL);
        msleep(1);
        HiethWritelBits(ld, 0, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_ALL);
        msleep(1);
        HiethWritelBits(ld, 1, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_ALL);
        msleep(1);
        HiethWritelBits(ld, 0, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_ALL);
    } else if (ld->port == DOWN_PORT) {
        /* Note: sf ip need reset twice */
        HiethWritelBits(ld, 1, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_DOWN);
        msleep(1);
        HiethWritelBits(ld, 0, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_DOWN);
        msleep(1);
        HiethWritelBits(ld, 1, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_DOWN);
        msleep(1);
        HiethWritelBits(ld, 0, GLB_SOFT_RESET, BITS_ETH_SOFT_RESET_DOWN);
    }
    return HDF_SUCCESS;
}

int32_t HiethPortInit(struct EthDevice *ethDevice)
{
    struct HiethNetdevLocal *ld = GetHiethNetDevLocal(ethDevice);
    if (ld == NULL) {
        HDF_LOGE("%s: get ld fail!", __func__);
        return HDF_FAILURE;
    }

    HiethSetEndianMode(ld, HIETH_LITTLE_ENDIAN);
    HiethSetLinkStat(ld, 0);
    HiethSetNegMode(ld, HIETH_NEGMODE_CPUSET);
    /* clear all interrupt status */
    HiethClearIrqstatus(ld, UD_BIT_NAME(BITS_IRQS_MASK));
    /* disable interrupts */
    HiethWritelBits(ld, 0, GLB_RW_IRQ_ENA, UD_BIT_NAME(BITS_IRQS_ENA));
    HiethIrqDisable(ld, UD_BIT_NAME(BITS_IRQS_MASK));

#ifdef HIETH_TSO_SUPPORTED
    /* enable TSO debug for error handle */
    HiethWritelBits(ld, 1, UD_REG_NAME(GLB_TSO_DBG_EN), BITS_TSO_DBG_EN);
#endif

    /* disable vlan func */
    HiethWritelBits(ld, 0, GLB_FWCTRL, BITS_VLAN_ENABLE);
    /* enable UpEther<->CPU */
    HiethWritelBits(ld, 1, GLB_FWCTRL, UD_BIT(ld->port, BITS_FW2CPU_ENA));
    HiethWritelBits(ld, 0, GLB_FWCTRL, UD_BIT(ld->port, BITS_FWALL2CPU));
    HiethWritelBits(ld, 1, GLB_MACTCTRL, UD_BIT(ld->port, BITS_BROAD2CPU));
    HiethWritelBits(ld, 1, GLB_MACTCTRL, UD_BIT(ld->port, BITS_MACT_ENA));
    HiethWritelBits(ld, 1, GLB_MACTCTRL, UD_BIT(ld->port, BITS_MULTI2CPU));

    HiethSetMacLeadcodeCntLimit(ld, 0);
    HiethSetRcvLenMax(ld, HIETH_MAX_RCV_LEN);
    RegisterHiethData(ethDevice);

    return HDF_SUCCESS;
}

static struct EthMacOps g_macOps = {
    .MacInit = HiethMacCoreInit,
    .PortReset = HiethPortReset,
    .PortInit = HiethPortInit,
};

struct HdfEthMacChipDriver *BuildHisiMacDriver(void)
{
    struct HdfEthMacChipDriver *macChipDriver = (struct HdfEthMacChipDriver *)OsalMemCalloc(
        sizeof(struct HdfEthMacChipDriver));
    if (macChipDriver == NULL) {
        HDF_LOGE("%s fail: OsalMemCalloc fail!", __func__);
        return NULL;
    }
    macChipDriver->ethMacOps = &g_macOps;
    return macChipDriver;
}

void ReleaseHisiMacDriver(struct HdfEthMacChipDriver *chipDriver)
{
    if (chipDriver == NULL) {
        return;
    }
    OsalMemFree(chipDriver);
}

struct HdfEthMacChipDriver *GetEthMacChipDriver(const struct NetDevice *netDev)
{
    struct HdfEthNetDeviceData *data = GetEthNetDeviceData(netDev);
    if (data != NULL) {
        return data->macChipDriver;
    }
    return NULL;
}
