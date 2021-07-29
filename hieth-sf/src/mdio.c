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

#include "hieth.h"
#include "mdio.h"

static int32_t WaitMdioReady(struct HiethNetdevLocal *ld)
{
    int32_t timeout = 1000;

    while (--timeout && !TestMdioReady(ld)) {
        udelay(1);
    }
    return timeout;
}

int32_t HiethMdioRead(struct HiethNetdevLocal *ld, int32_t phyAddr, int32_t regNum)
{
    int32_t val = 0;

    HiethAssert((!((uint32_t)phyAddr & (~0x1F))) && (!((uint32_t)regNum & (~0x1F))));
    if (!WaitMdioReady(ld)) {
        HiethError("mdio busy");
        return val;
    }
    MdioStartPhyread(ld, phyAddr, regNum);
    if (WaitMdioReady(ld)) {
        val = MdioGetPhyreadVal(ld);
    } else {
        HiethError("read timeout");
    }

    return val;
}

int32_t HiethMdioWrite(struct HiethNetdevLocal *ld, int32_t phyAddr, int32_t regNum, int32_t val)
{
    HiethAssert((!((uint32_t)phyAddr & (~0x1F))) && (!((uint32_t)regNum & (~0x1F))));
    HiethTrace(HIETHTRACE_LEVEL_L4, "phyAddr = %d, regNum = %d", phyAddr, regNum);

    OsalSpinLockIrq(&hiethGlbRegLock);

    if (!WaitMdioReady(ld)) {
        HiethError("mdio busy");
        OsalSpinUnlockIrq(&hiethGlbRegLock);
        return HDF_FAILURE;
    }
    MdioPhyWrite(ld, phyAddr, regNum, val);
    OsalSpinUnlockIrq(&hiethGlbRegLock);
    return HDF_SUCCESS;
}

int32_t HiethMdioReset(struct HiethNetdevLocal *ld)
{
    MdioRegReset(ld);
    return HDF_SUCCESS;
}

int32_t HiethMdioInit(struct HiethNetdevLocal *ld)
{
    int32_t ret;
    ret = HiethMdioReset(ld);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: HiethMdioReset failed", __func__);
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

void HiethMdioExit(struct HiethNetdevLocal *ld)
{
}
