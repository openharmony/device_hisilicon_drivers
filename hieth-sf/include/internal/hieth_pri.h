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

#ifndef HIETH_PRI_H
#define HIETH_PRI_H

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include "hieth.h"
#include "hisoc/net.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define HIETH_TSO_DEBUG
#define HIETH_RXCSUM_SUPPORTED

#define HIETH_PHY_RMII_MODE 1
#define HIETH_PHY_MII_MODE 0
#define CONFIG_HIETH_TRACE_LEVEL 18
#define HIETH_MAX_QUEUE_DEPTH 64
#define HIETH_HWQ_RXQ_DEPTH 128

#define HIETH_MIIBUS_NAME "himii"

#define HIETH_MAX_FRAME_SIZE (1520)

#define HIETH_MAX_MAC_FILTER_NUM      8
#define HIETH_MAX_UNICAST_ADDRESSES   2
#define HIETH_MAX_MULTICAST_ADDRESSES (HIETH_MAX_MAC_FILTER_NUM - HIETH_MAX_UNICAST_ADDRESSES)

#define HIETHTRACE_LEVEL_L2 2
#define HIETHTRACE_LEVEL_L4 4

#define HiethTrace(level, msg...) \
    do { \
        if ((level) >= CONFIG_HIETH_TRACE_LEVEL) { \
            pr_info("HiethTrace:%s:%d: ", __func__, __LINE__); \
            printk(msg); \
            printk("\n"); \
        } \
    } while (0)

#define HiethError(s...) \
    do { \
        pr_err("hieth:%s:%d: ", __func__, __LINE__); \
        pr_err(s); \
        pr_err("\n"); \
    } while (0)

#define HiethAssert(cond) \
    do { \
        if (!(cond)) { \
            pr_err("Assert:hieth:%s:%d\n", __func__, __LINE__); \
            BUG(); \
        } \
    } while (0)

static inline const char *PhyModes(PhyInterfaceMode mode)
{
    switch (mode) {
        case PHY_INTERFACE_MODE_MII:
            return "mii";
        case PHY_INTERFACE_MODE_RMII:
            return "rmii";
        default:
            return "unknown";
    }
}

#define FC_ACTIVE_MIN       1
#define FC_ACTIVE_DEFAULT   3
#define FC_ACTIVE_MAX       31
#define FC_DEACTIVE_MIN     1
#define FC_DEACTIVE_DEFAULT 5
#define FC_DEACTIVE_MAX     31

#define NO_EEE      0
#define MAC_EEE     1
#define PHY_EEE     2
#define PARTNER_EEE 2
#define DEBUG       0

#define EVENT_NET_TX_RX    0x1
#define EVENT_NET_CAN_SEND 0x2

extern OsalSpinlock hiethGlbRegLock;

#ifdef HIETH_TSO_DEBUG
#define MAX_RECORD (100)
struct SendPktInfo {
    uint32_t regAddr;
    uint32_t regPktInfo;
    uint32_t status;
};
#endif

/* read/write IO */
#define HiethRead(ld, ofs) \
    ({ unsigned long reg = readl((ld)->iobase + (ofs)); \
        HiethTrace(HIETHTRACE_LEVEL_L2, "readl(0x%04X) = 0x%08lX", (ofs), reg); \
        reg; })

#define HiethWrite(ld, v, ofs) \
    do { \
        writel(v, (ld)->iobase + (ofs)); \
        HiethTrace (HIETHTRACE_LEVEL_L2, "writel(0x%04X) = 0x%08lX", (ofs), (unsigned long)(v)); \
    } while (0)

#define MK_BITS(shift, nbits) ((((shift)&0x1F) << 16) | ((nbits)&0x3F))

#define HiethWritelBits(ld, v, ofs, bits_desc) \
    do { \
        unsigned long _bits_desc = bits_desc; \
        unsigned long _shift = (_bits_desc) >> 16; \
        unsigned long _reg = HiethRead(ld, ofs); \
        unsigned long _mask = \
            ((_bits_desc & 0x3F) < 32) ? (((1 << (_bits_desc & 0x3F)) - 1) << (_shift)) : 0xffffffff; \
        HiethWrite(ld, (_reg & (~_mask)) | (((unsigned long)(v) << (_shift)) & _mask), ofs); \
    } while (0)

#define HiethReadlBits(ld, ofs, bits_desc) ({ \
    unsigned long _bits_desc = bits_desc; \
    unsigned long _shift = (_bits_desc)>>16; \
    unsigned long _mask = \
        ((_bits_desc & 0x3F) < 32) ? (((1 << (_bits_desc & 0x3F)) - 1)<<(_shift)) : 0xffffffff; \
    (HiethRead(ld, ofs)&_mask)>>(_shift); })

#define HIETH_TRACE_LEVEL 8

#define HiregTrace(level, msg...) \
    do { \
        if ((level) >= HIETH_TRACE_LEVEL) {  \
            pr_info("HiregTrace:%s:%d: ", __func__, __LINE__); \
            printk(msg); \
            printk("\n"); \
        } \
    } while (0)

#define HiregReadl(base, ofs) ({ unsigned long reg = readl((base) + (ofs)); \
        HiregTrace(HIETHTRACE_LEVEL_L2, "_readl(0x%04X) = 0x%08lX", (ofs), reg); \
        reg; })

#define HiregWritel(base, v, ofs) \
    do { \
        writel((v), (base) + (ofs)); \
        HiregTrace(2, "_writel(0x%04X) = 0x%08lX", \
           (ofs), (unsigned long)(v));  \
    } while (0)

#define HiregWritelBits(base, v, ofs, bits_desc) \
    do { \
        unsigned long _bits_desc = bits_desc; \
        unsigned long _shift = (_bits_desc) >> 16; \
        unsigned long _reg = HiregReadl(base, ofs); \
        unsigned long _mask = \
            ((_bits_desc & 0x3F) < 32) ? (((1 << (_bits_desc & 0x3F)) - 1) << (_shift)) : 0xffffffff; \
        HiregWritel(base, (_reg & (~_mask)) | (((v) << (_shift)) & _mask), ofs); \
    } while (0)

#define HiregReadlBits(base, ofs, bits_desc) ({ \
    unsigned long _bits_desc = bits_desc; \
    unsigned long _shift = (_bits_desc)>>16; \
    unsigned long _mask = \
        ((_bits_desc & 0x3F) < 32) ? (((1 << (_bits_desc & 0x3F)) - 1)<<(_shift)) : 0xffffffff; \
    (HiregReadl(base, ofs)&_mask)>>(_shift); })

#define UD_REG_NAME(name) ((ld->port == UP_PORT) ? U_##name : D_##name)
#define UD_BIT_NAME(name) ((ld->port == UP_PORT) ? name##_U : name##_D)
#define UD_PHY_NAME(name) ((ld->port == UP_PORT) ? name##_U : name##_D)

#define UD_BIT(port, name) (((port) == UP_PORT) ? name##_U : name##_D)

#define GLB_MAC_H16(port, reg) ((((port) == UP_PORT) ? GLB_MAC_H16_BASE : GLB_MAC_H16_BASE_D) + (reg * 0x8))
#define GLB_MAC_L32(port, reg) ((((port) == UP_PORT) ? GLB_MAC_L32_BASE : GLB_MAC_L32_BASE_D) + (reg * 0x8))

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* HIETH_PRI_H */
