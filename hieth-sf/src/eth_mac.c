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

#include "eth_mac.h"
#include "hieth_pri.h"

extern uint64_t hi_sched_clock(void);
#define MICROSECOND_TIME_LONG 1000
#define MICROSECOND_TIME_SHORT 100
#define MILLISECOND_TIME 200

static int32_t SetLinkStat(struct HiethNetdevLocal *ld, unsigned long mode)
{
    int32_t old;

    old = HiethReadlBits(ld, UD_REG_NAME(MAC_PORTSET), BITS_MACSTAT);
    HiethWritelBits(ld, mode, UD_REG_NAME(MAC_PORTSET), BITS_MACSTAT);
    return old;
}

static int32_t SetNegMode(struct HiethNetdevLocal *ld, int32_t mode)
{
    int32_t old;

    old = HiethReadlBits(ld, UD_REG_NAME(MAC_PORTSEL), BITS_NEGMODE);
    HiethWritelBits(ld, mode, UD_REG_NAME(MAC_PORTSEL), BITS_NEGMODE);
    return old;
}

static int32_t GetNegMode(struct HiethNetdevLocal *ld)
{
    return HiethReadlBits(ld, UD_REG_NAME(MAC_PORTSEL), BITS_NEGMODE);
}

int32_t HiethSetLinkStat(struct HiethNetdevLocal *ld, unsigned long mode)
{
    return SetLinkStat(ld, mode);
}

int32_t HiethGetLinkStat(struct HiethNetdevLocal *ld)
{
    return HiethReadlBits(ld, UD_REG_NAME(MAC_RO_STAT), BITS_MACSTAT);
}

int32_t HiethSetMacLeadcodeCntLimit(struct HiethNetdevLocal *ld, int32_t cnt)
{
    int32_t old;

    OsalSpinLockIrq(&hiethGlbRegLock);
    old = HiethReadlBits(ld, UD_REG_NAME(MAC_TX_IPGCTRL), BITS_PRE_CNT_LIMIT);
    HiethWritelBits(ld, cnt, UD_REG_NAME(MAC_TX_IPGCTRL), BITS_PRE_CNT_LIMIT);
    OsalSpinUnlockIrq(&hiethGlbRegLock);
    return old;
}

int32_t HiethSetMacTransIntervalBits(struct HiethNetdevLocal *ld, int32_t nbits)
{
    int32_t old;
    int32_t linkstat, negmode;

    OsalSpinLockIrq(&hiethGlbRegLock);

    negmode = SetNegMode(ld, HIETH_NEGMODE_CPUSET);
    linkstat = SetLinkStat(ld, 0);
    udelay(MICROSECOND_TIME_LONG);

    old = HiethReadlBits(ld, UD_REG_NAME(MAC_TX_IPGCTRL), BITS_IPG);
    HiethWritelBits(ld, nbits, UD_REG_NAME(MAC_TX_IPGCTRL), BITS_IPG);
    udelay(MICROSECOND_TIME_SHORT);

    SetNegMode(ld, negmode);
    SetLinkStat(ld, linkstat);

    OsalSpinUnlockIrq(&hiethGlbRegLock);
    return old;
}

int32_t HiethSetMacFcInterval(struct HiethNetdevLocal *ld, int32_t para)
{
    int32_t old;

    OsalSpinLockIrq(&hiethGlbRegLock);
    old = HiethReadlBits(ld, UD_REG_NAME(MAC_TX_IPGCTRL), BITS_FC_INTER);
    HiethWritelBits(ld, para, UD_REG_NAME(MAC_TX_IPGCTRL), BITS_FC_INTER);
    OsalSpinUnlockIrq(&hiethGlbRegLock);
    return old;
}

int32_t HiethSetNegMode(struct HiethNetdevLocal *ld, int32_t mode)
{
    int32_t old;

    OsalSpinLockIrq(&hiethGlbRegLock);
    old = SetNegMode(ld, mode);
    OsalSpinUnlockIrq(&hiethGlbRegLock);
    return old;
}

int32_t HiethGetNegmode(struct HiethNetdevLocal *ld)
{
    int32_t old;

    OsalSpinLockIrq(&hiethGlbRegLock);
    old = GetNegMode(ld);
    OsalSpinUnlockIrq(&hiethGlbRegLock);
    return old;
}

int32_t HiethSetMiiMode(struct HiethNetdevLocal *ld, int32_t mode)
{
    int32_t old;

    old = HiethReadlBits(ld, UD_REG_NAME(MAC_PORTSEL), BITS_MII_MODE);
    HiethWritelBits(ld, mode, UD_REG_NAME(MAC_PORTSEL), BITS_MII_MODE);
    return old;
}

void HiethSetRcvLenMax(struct HiethNetdevLocal *ld, int32_t cnt)
{
    OsalSpinLockIrq(&hiethGlbRegLock);
    HiethWritelBits(ld, cnt, UD_REG_NAME(MAC_SET), BITS_LEN_MAX);
    OsalSpinUnlockIrq(&hiethGlbRegLock);
}

extern void HiRandomHwInit(void);
extern void HiRandomHwDeinit(void);
extern int32_t HiRandomHwGetInteger(uint32_t *result);

void EthHisiRandomAddr(uint8_t *addr, int32_t len)
{
    uint32_t randVal;
    int32_t ret;

    msleep(MILLISECOND_TIME);
    HiRandomHwInit();
    ret = HiRandomHwGetInteger(&randVal);
    if (ret != 0) {
        randVal = (uint32_t)(hi_sched_clock() & 0xffffffff);
    }
    addr[0] = randVal & 0xff;
    addr[1] = (randVal >> MAC_ADDR_OFFSET_L8) & 0xff;
    addr[2] = (randVal >> MAC_ADDR_OFFSET_L16) & 0xff;
    addr[3] = (randVal >> MAC_ADDR_OFFSET_L24) & 0xff;

    msleep(MILLISECOND_TIME);
    ret = HiRandomHwGetInteger(&randVal);
    if (ret != 0) {
        randVal = (uint32_t)(hi_sched_clock() & 0xffffffff);
    }
    addr[4] = randVal & 0xff;
    addr[5] = ((uint32_t)randVal >> MAC_ADDR_OFFSET_L8) & 0xff;

    addr[0] &= 0xfe; /* clear multicast bit */
    addr[0] |= 0x02; /* set local assignment bit (IEEE802) */

    HiRandomHwDeinit();
}
